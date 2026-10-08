cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# libffi -- a line-for-line CMake translation of the upstream autotools build.
#
# Upstream builds from three files, and this one reproduces what they do rather
# than re-deriving it:
#
#   configure.ac     the probes, the `fficonfig.h` contents (autoheader +
#                    AH_BOTTOM), the `include/ffi.h` substitution, the copy of
#                    `src/$TARGETDIR/ffitarget.h`, the FFI_DEBUG / structs /
#                    raw-api switches, and the `libffi.map` rule.
#   configure.host   `TARGET`, `TARGETDIR`, `MSVC` and `SOURCES` per host, plus
#                    the second case block that turns `TARGET` into the final
#                    `SOURCES` list.
#   Makefile.am      `libffi_la_SOURCES` + `TARGET_OBJ`: the object list the
#                    library is actually made of.
#
# The upstream variable names are kept where CMake allows it -- `SOURCES`,
# `TARGET_X32`, `FFI_VERSION_*` -- so the two can be read side by side.  The two
# that cannot be are `TARGET` and `TARGETDIR`: `if(TARGET ...)` is CMake's own
# "does this target exist" test, so those become `FFI_TARGET`/`FFI_TARGETDIR`.
# `MSVC` is not defined here at all -- configure.host sets it from the compiler
# vendor, which is exactly what CMake's built-in `MSVC` already means.
#
# Everything libffi does *not* need here is deliberately absent: the libtool
# install rules, the `.pc` file, the versioned-symbol map (only meaningful for
# a shared ELF library), the docs, and the testsuite.  A static library is the
# only artefact, which is what `make libffi_convenience.la` would leave behind.
#
# A host configure.host knows but this file does not falls through to the same
# `UNSUPPORTED` error configure.ac raises, so an unsupported build stops here
# with a message instead of quietly compiling the wrong backend.
# ---------------------------------------------------------------------------

# --- AC_INIT([libffi],[3.8.0]) --------------------------------------------
# FFI_VERSION_NUMBER is X*10000 + Y*100 + Z, derived from the version string so
# the two cannot drift (configure.ac does the same with cut/sed/expr).
set(FFI_VERSION_MAJOR 3)
set(FFI_VERSION_MINOR 8)
set(FFI_VERSION_MICRO 0)
set(FFI_VERSION_STRING
  "${FFI_VERSION_MAJOR}.${FFI_VERSION_MINOR}.${FFI_VERSION_MICRO}")
math(EXPR FFI_VERSION_NUMBER
  "${FFI_VERSION_MAJOR} * 10000 + ${FFI_VERSION_MINOR} * 100 + ${FFI_VERSION_MICRO}")

# ---------------------------------------------------------------------------
# Options
#
# configure.ac's --enable-debug / --disable-structs / --disable-raw-api, as
# CMake options.  All three default the way configure.ac leaves them (off, so
# the full-featured build), and each one only ever *adds* a define.
# ---------------------------------------------------------------------------
option(FFI_ENABLE_DEBUG   "extra debugging (--enable-debug)" OFF)
option(FFI_ENABLE_STRUCTS "support for aggregate types"      ON)
option(FFI_ENABLE_RAW_API "support for the raw API"          ON)

# ---------------------------------------------------------------------------
# Probes configure.ac runs
# ---------------------------------------------------------------------------

# `AC_CHECK_SIZEOF(size_t)`, `AC_CHECK_SIZEOF(double)`, `AC_CHECK_SIZEOF(long
# double)`.  check_type_size compiles a probe per type, like autoconf.
include(CheckTypeSize)
check_type_size("size_t" SIZEOF_SIZE_T)
check_type_size("double" SIZEOF_DOUBLE)
check_type_size("long double" SIZEOF_LONG_DOUBLE)

# check_type_size leaves a variable empty when it cannot measure the type at
# all, and the generated header would then emit `#define SIZEOF_X` with no
# value.  Fall back to the size configure.ac's defaults imply.
foreach(size_var SIZEOF_SIZE_T SIZEOF_DOUBLE SIZEOF_LONG_DOUBLE)
  if(NOT "${${size_var}}")
    if(size_var STREQUAL "SIZEOF_SIZE_T")
      set(${size_var} "${CMAKE_SIZEOF_VOID_P}")
    else()
      set(${size_var} 8)
    endif()
  endif()
endforeach()

# `AC_CHECK_TYPE(__int128_t, AC_DEFINE(HAVE_INT128))`.
include(CheckCSourceCompiles)
check_c_source_compiles(
  "__int128_t a = 0; __uint128_t b = 0;
   int main(void) { return (int)(a + b); }"
  HAVE_INT128)

# The probe behind `libffi_cv_hidden_visibility_attribute`.  MSVC fails it,
# which is the point: HAVE_HIDDEN_VISIBILITY_ATTRIBUTE being absent is what
# makes AH_BOTTOM expand FFI_HIDDEN to nothing.
check_c_source_compiles(
  "int __attribute__((visibility(\"hidden\"))) ffi_probe(void) { return 1; }
   int main(void) { return ffi_probe(); }"
  HAVE_HIDDEN_VISIBILITY_ATTRIBUTE)

# `AX_COMPILER_VENDOR`, which configure.host branches on to decide whether the
# MSVC assembly sources are the ones to use.
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
  set(ax_cv_c_compiler_vendor "microsoft")
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
  set(ax_cv_c_compiler_vendor "clang")
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  set(ax_cv_c_compiler_vendor "gnu")
else()
  set(ax_cv_c_compiler_vendor "unknown")
endif()

# `AC_CANONICAL_TARGET` + the `$host` string configure.host matches on.  CMake
# hands over the processor and the system name separately, so those two are
# matched directly; every branch below is one case of configure.host.
set(host_cpu "${CMAKE_SYSTEM_PROCESSOR}")
if(NOT host_cpu)
  set(host_cpu "${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()
string(TOLOWER "${host_cpu}" host_cpu)
string(TOLOWER "${CMAKE_SYSTEM_NAME}" host_os)

# `ac_cv_sizeof_size_t`, which configure.host uses to tell a 32-bit build from a
# 64-bit one on the same architecture.
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(ac_cv_sizeof_size_t 8)
else()
  set(ac_cv_sizeof_size_t 4)
endif()

# ---------------------------------------------------------------------------
# configure.host -- first case block: TARGET / TARGETDIR / MSVC
# ---------------------------------------------------------------------------

set(FFI_TARGETDIR "unknown")
set(FFI_TARGET "")
set(SOURCES "")
set(TARGET_X32 FALSE)
set(HAVE_LONG_DOUBLE_VARIANT 0)

if(host_os STREQUAL "windows" OR host_os STREQUAL "cygwin" OR host_os STREQUAL "msys")
  if(host_cpu MATCHES "^(amd64|x86_64|x64)$")
    # i?86-*-win* | x86_64-*-win* (and the cygwin/msys/mingw spellings)
    set(FFI_TARGETDIR x86)
    if(ac_cv_sizeof_size_t EQUAL 4)
      set(FFI_TARGET X86_WIN32)
    else()
      set(FFI_TARGET X86_WIN64)
    endif()
  elseif(host_cpu MATCHES "^(x86|i[3-6]86)$")
    set(FFI_TARGETDIR x86)
    set(FFI_TARGET X86_WIN32)
  elseif(host_cpu MATCHES "^(aarch64|arm64)$")
    set(FFI_TARGET ARM_WIN64)
    set(FFI_TARGETDIR aarch64)
  elseif(host_cpu MATCHES "^arm")
    set(FFI_TARGET ARM_WIN32)
    set(FFI_TARGETDIR arm)
  endif()
elseif(host_os STREQUAL "darwin" OR host_os STREQUAL "ios")
  if(host_cpu MATCHES "^(amd64|x86_64|x64)$")
    set(FFI_TARGETDIR x86)
    if(ac_cv_sizeof_size_t EQUAL 4)
      set(FFI_TARGET X86_DARWIN)
    else()
      set(FFI_TARGET X86_64)
    endif()
  elseif(host_cpu MATCHES "^(aarch64|arm64)$")
    set(FFI_TARGET AARCH64)
    set(FFI_TARGETDIR aarch64)
  endif()
elseif(host_os MATCHES "^(emscripten|html)$")
  # wasm32-*-* / wasm64-*-*; both use the same backend directory.
  if(ac_cv_sizeof_size_t EQUAL 4)
    set(FFI_TARGET wasm32)
  else()
    set(FFI_TARGET wasm64)
  endif()
  set(FFI_TARGETDIR wasm)
elseif(host_cpu MATCHES "^(amd64|x86_64|x64)$")
  # i?86-*-* | x86_64-*-* | amd64-* on a POSIX host.
  set(FFI_TARGETDIR x86)
  if(ac_cv_sizeof_size_t EQUAL 4)
    # A 32-bit `size_t` on an x86-64 compiler is x32 unless the compiler
    # refuses to emit 64-bit code; configure.host compiles a probe for that.
    check_c_source_compiles(
      "int main(void) { return __x86_64__; }" LIBFFI_CV_TARGET_X32)
    if(LIBFFI_CV_TARGET_X32)
      set(TARGET_X32 TRUE)
      set(FFI_TARGET X86_64)
    else()
      set(FFI_TARGET X86)
    endif()
  else()
    set(FFI_TARGET X86_64)
  endif()
elseif(host_cpu MATCHES "^(aarch64|arm64)$")
  set(FFI_TARGET AARCH64)
  set(FFI_TARGETDIR aarch64)
elseif(host_cpu MATCHES "^arm")
  set(FFI_TARGET ARM)
  set(FFI_TARGETDIR arm)
elseif(host_cpu MATCHES "^riscv")
  set(FFI_TARGET RISCV)
  set(FFI_TARGETDIR riscv)
elseif(host_cpu MATCHES "^s390")
  set(FFI_TARGET S390)
  set(FFI_TARGETDIR s390)
elseif(host_cpu MATCHES "^loongarch")
  if(ac_cv_sizeof_size_t EQUAL 4)
    set(FFI_TARGET LOONGARCH32)
  else()
    set(FFI_TARGET LOONGARCH64)
  endif()
  set(FFI_TARGETDIR loongarch)
endif()

# ---------------------------------------------------------------------------
# configure.host -- second case block: TARGET -> SOURCES
#
# Every name in SOURCES is relative to `src/${FFI_TARGETDIR}/`.
# ---------------------------------------------------------------------------

if(FFI_TARGET STREQUAL "X86_WIN64")
  if(MSVC)
    set(SOURCES ffiw64.c win64_intel.S)
  else()
    set(SOURCES ffiw64.c win64.S)
  endif()
elseif(FFI_TARGET STREQUAL "X86_64")
  if(TARGET_X32)
    set(SOURCES ffi64.c unix64.S)
  else()
    set(SOURCES ffi64.c unix64.S ffiw64.c win64.S)
  endif()
elseif(FFI_TARGET STREQUAL "X86" OR FFI_TARGET STREQUAL "X86_DARWIN"
    OR FFI_TARGET STREQUAL "X86_FREEBSD" OR FFI_TARGET STREQUAL "X86_WIN32")
  if(MSVC)
    set(SOURCES ffi.c sysv_intel.S)
  else()
    set(SOURCES ffi.c sysv.S)
  endif()
elseif(FFI_TARGET STREQUAL "ARM_WIN64")
  if(MSVC)
    set(SOURCES ffi.c win64_armasm.S)
  else()
    set(SOURCES ffi.c sysv.S)
  endif()
elseif(FFI_TARGET STREQUAL "ARM_WIN32")
  if(MSVC)
    set(SOURCES ffi.c sysv_msvc_arm32.S)
  else()
    set(SOURCES ffi.c sysv.S)
  endif()
elseif(FFI_TARGET STREQUAL "AARCH64")
  set(SOURCES ffi.c sysv.S)
elseif(FFI_TARGET STREQUAL "ARM")
  set(SOURCES ffi.c sysv.S)
elseif(FFI_TARGET STREQUAL "RISCV")
  set(SOURCES ffi.c sysv.S)
elseif(FFI_TARGET STREQUAL "S390")
  set(SOURCES ffi.c sysv.S)
elseif(FFI_TARGET STREQUAL "LOONGARCH32" OR FFI_TARGET STREQUAL "LOONGARCH64")
  set(SOURCES ffi.c sysv.S)
elseif(FFI_TARGET STREQUAL "wasm32" OR FFI_TARGET STREQUAL "wasm64")
  set(SOURCES ffi.c)
endif()

# `if test -z "${SOURCES}"; then UNSUPPORTED=1; fi` -- and configure.ac turns
# that into AC_MSG_ERROR.
if("${SOURCES}" STREQUAL "")
  message(FATAL_ERROR
    "libffi has not been ported to ${host_cpu}-${host_os}: configure.host "
    "selects no TARGET/SOURCES for it (the upstream build fails the same way).")
endif()

# ---------------------------------------------------------------------------
# configure.ac -- the remaining probes, all of which only feed fficonfig.h
# ---------------------------------------------------------------------------

set(SIZEOF_DOUBLE 8)
set(STDC_HEADERS 1)
set(HAVE_MEMCPY 1)
set(HAVE_ALLOCA 1)

# `if test "x$GCC" = "xyes"` guards the assembler/ELF probes below.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  set(libffi_gnu_toolchain TRUE)
else()
  set(libffi_gnu_toolchain FALSE)
endif()

if(libffi_gnu_toolchain)
  # `GCC_AS_CFI_PSEUDO_OP`: the .S files spell CFI directives through
  # ffi_cfi.h only when the assembler understands them.
  set(HAVE_AS_CFI_PSEUDO_OP 1)
  # `libffi_cv_ro_eh_frame` and the EH_FRAME_FLAGS AC_DEFINE pair.
  set(HAVE_RO_EH_FRAME 1)
  set(EH_FRAME_FLAGS "\"a\"")
endif()

# `case "$TARGET" in X86*)` -- `libffi_cv_as_x86_pcrel`.
if(libffi_gnu_toolchain AND FFI_TARGET MATCHES "^X86")
  set(HAVE_AS_X86_PCREL 1)
endif()

# `if test x$TARGET = xX86_64` -- `libffi_cv_as_x86_64_unwind_section_type`.
if(libffi_gnu_toolchain AND FFI_TARGET STREQUAL "X86_64")
  set(HAVE_AS_X86_64_UNWIND_SECTION_TYPE 1)
endif()

# `FFI_EXEC_TRAMPOLINE_TABLE` and `FFI_MMAP_EXEC_WRIT`, from the `case "$target"`
# block that follows LT_SYS_SYMBOL_USCORE.
if(host_os STREQUAL "darwin" OR host_os STREQUAL "ios")
  if(host_cpu MATCHES "^arm" OR host_cpu MATCHES "^(aarch64|arm64)$")
    set(FFI_EXEC_TRAMPOLINE_TABLE 1)
  endif()
  set(FFI_MMAP_EXEC_WRIT 1)
elseif(host_os MATCHES "^(dragonfly|freebsd|kfreebsd|openbsd)$")
  set(FFI_MMAP_EXEC_WRIT 1)
elseif(host_os STREQUAL "sunos" OR host_os STREQUAL "solaris")
  set(FFI_MMAP_EXEC_WRIT 1)
elseif(host_os STREQUAL "android")
  set(FFI_MMAP_EXEC_WRIT 1)
endif()
# Note: plain Linux and Windows are *not* in that list.  closures.c decides for
# itself there -- the `__linux__ && !__ANDROID__` arm and the
# `__CYGWIN__ || _WIN32 || __OS2__` arm both define FFI_MMAP_EXEC_WRIT -- so
# defining it here would be a second, redundant source of truth.

# `--enable-exec-static-tramp`, on by default, then restricted by target.
set(FFI_EXEC_STATIC_TRAMP 0)
if(host_os STREQUAL "cygwin" OR host_os STREQUAL "msys")
  if(libffi_gnu_toolchain)
    set(FFI_EXEC_STATIC_TRAMP 1)
  endif()
elseif(host_os STREQUAL "linux")
  set(FFI_EXEC_STATIC_TRAMP 1)
endif()

# `AC_CHECK_SIZEOF(long double)` + HAVE_LONG_DOUBLE, reproducing the nested test
# in configure.ac: a long double that is no wider than a double is not a
# distinct type as far as FFI_TYPE_LONGDOUBLE is concerned.
set(HAVE_LONG_DOUBLE 0)
if(SIZEOF_LONG_DOUBLE AND NOT SIZEOF_LONG_DOUBLE EQUAL 0)
  if(HAVE_LONG_DOUBLE_VARIANT)
    set(HAVE_LONG_DOUBLE 1)
  elseif(NOT SIZEOF_DOUBLE EQUAL SIZEOF_LONG_DOUBLE)
    set(HAVE_LONG_DOUBLE 1)
  endif()
endif()

# `AC_CHECK_FUNCS(memfd_create)` / `AC_CHECK_HEADERS(sys/memfd.h)`, both only
# reachable from the POSIX arm of closures.c.
if(NOT WIN32)
  check_function_exists(memfd_create HAVE_MEMFD_CREATE)
  check_include_file(sys/memfd.h HAVE_SYS_MEMFD_H)
endif()

# The `AC_CHECK_HEADERS` / `AC_CHECK_FUNCS` results autoheader records.  WIN32
# is the one platform where a sizeable part of them is absent, and it is also
# the platform this tree builds for, so the two arms are spelled out rather than
# probed one by one.
if(WIN32)
  set(HAVE_INTTYPES_H 1)
  set(HAVE_MEMORY_H 1)
  set(HAVE_STDINT_H 1)
  set(HAVE_STDLIB_H 1)
  set(HAVE_STRING_H 1)
  set(HAVE_SYS_STAT_H 1)
  set(HAVE_SYS_TYPES_H 1)
  # <alloca.h>, <strings.h>, <unistd.h>, <dlfcn.h> and <sys/mman.h> either do
  # not exist under MSVC or are not the headers the code means.
else()
  set(HAVE_INTTYPES_H 1)
  set(HAVE_MEMORY_H 1)
  set(HAVE_STDINT_H 1)
  set(HAVE_STDLIB_H 1)
  set(HAVE_STRING_H 1)
  set(HAVE_STRINGS_H 1)
  set(HAVE_SYS_STAT_H 1)
  set(HAVE_SYS_TYPES_H 1)
  set(HAVE_UNISTD_H 1)
  set(HAVE_DLFCN_H 1)
  set(HAVE_SYS_MMAN_H 1)
  set(HAVE_ALLOCA_H 1)
  set(HAVE_MKOSTEMP 1)
  set(HAVE_MMAP 1)
  set(HAVE_MMAP_ANON 1)
  set(HAVE_MMAP_DEV_ZERO 1)
  set(HAVE_MMAP_FILE 1)
endif()

# ---------------------------------------------------------------------------
# fficonfig.h -- AC_CONFIG_HEADERS([fficonfig.h])
# ---------------------------------------------------------------------------

set(PACKAGE_NAME "libffi")
set(PACKAGE_BUGREPORT "http://github.com/libffi/libffi/issues")
set(PACKAGE_URL "")

set(LIBFFI_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${LIBFFI_INCLUDE_DIR}")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/fficonfig.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/fficonfig.h"
  @ONLY)

# ---------------------------------------------------------------------------
# include/ffi.h -- AC_CONFIG_FILES(include/ffi.h)
#
# The `@TARGET@` / `@FFI_EXEC_TRAMPOLINE_TABLE@` / `@HAVE_LONG_DOUBLE@` slots
# land on `#define` and `#if` lines, so each one has to be a token: an unset
# variable would leave an `#if` with no expression (or an `#ifndef` with no
# identifier).  `TARGET` is spelled with upstream's name here and only here --
# the template says `@TARGET@` -- while the dispatch above uses `FFI_TARGET`,
# because a bare `if(TARGET ...)` is CMake's own "does this target exist" test.
# ---------------------------------------------------------------------------

set(TARGET "${FFI_TARGET}")
set(VERSION "${FFI_VERSION_STRING}")
if(NOT FFI_EXEC_TRAMPOLINE_TABLE)
  set(FFI_EXEC_TRAMPOLINE_TABLE 0)
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/ffi.h.in"
  "${LIBFFI_INCLUDE_DIR}/ffi.h"
  @ONLY)

# ---------------------------------------------------------------------------
# include/ffitarget.h -- the copy configure.ac does after AC_OUTPUT
#
# "Copy this file instead of using AC_CONFIG_LINK in order to support compiling
# with MSVC, which won't understand cygwin style symlinks."  Same reason here:
# the header is taken from the target directory rather than referenced there, so
# `-Iinclude` alone resolves it.
# ---------------------------------------------------------------------------

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/src/${FFI_TARGETDIR}/ffitarget.h"
  "${LIBFFI_INCLUDE_DIR}/ffitarget.h"
  COPYONLY)

# ---------------------------------------------------------------------------
# Makefile.am -- libffi_la_SOURCES and TARGET_OBJ
# ---------------------------------------------------------------------------

set(LIBFFI_SOURCES
  src/prep_cif.c
  src/types.c
  src/raw_api.c
  src/java_raw_api.c
  src/closures.c
  src/tramp.c
)

# `if FFI_DEBUG  libffi_la_SOURCES += src/debug.c`
if(FFI_ENABLE_DEBUG)
  list(APPEND LIBFFI_SOURCES src/debug.c)
endif()

# The `TARGET_OBJ` loop at the bottom of configure.ac, which prefixes every
# entry of SOURCES with `src/$TARGETDIR/`.  `.S` entries are separated out
# because they cannot be handed to the C compiler as-is (see below).
set(LIBFFI_TARGET_ASM "")
foreach(entry IN LISTS SOURCES)
  if(entry MATCHES "\\.S$")
    list(APPEND LIBFFI_TARGET_ASM "${entry}")
  else()
    list(APPEND LIBFFI_SOURCES "src/${FFI_TARGETDIR}/${entry}")
  endif()
endforeach()

# ---------------------------------------------------------------------------
# The assembly sources
#
# `.S` is C-preprocessed assembly: it #includes <fficonfig.h> and <ffi.h> and
# spells the symbol decoration (`C()`, `PLT()`, `FFI_HIDDEN()`) and the CFI
# directives through macros, so a preprocessor has to run before any assembler
# sees it.  How that happens depends on the assembler:
#
#   * GNU as (MinGW, ELF, Mach-O) reads `.S` itself and runs the preprocessor on
#     the way in, so the file is compiled directly -- Makefile.am's
#     `AM_CCASFLAGS = '$(AM_CPPFLAGS)'` is what feeds it the include path.
#   * MASM (ml64, via MSVC) cannot.  msvcc.sh preprocesses with `cl -EP` and
#     assembles the result with ml64, which is what PreprocessAsm.cmake does
#     here; the `.asm` it writes is then an ordinary MASM source for the target.
# ---------------------------------------------------------------------------

set(LIBFFI_GENERATED_HEADERS
  "${CMAKE_CURRENT_BINARY_DIR}/fficonfig.h"
  "${LIBFFI_INCLUDE_DIR}/ffi.h"
  "${LIBFFI_INCLUDE_DIR}/ffitarget.h")

# `AM_CPPFLAGS = -I. -I$(top_srcdir)/include -Iinclude -I$(top_srcdir)/src`
set(LIBFFI_INCLUDE_DIRS
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${LIBFFI_INCLUDE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/${FFI_TARGETDIR}")

set(LIBFFI_ASSEMBLED "")
if(NOT "${LIBFFI_TARGET_ASM}" STREQUAL "")
  if(MSVC)
    enable_language(ASM_MASM)
    # `|`-separated, because a `;` would be re-split by CMake's `-D` parsing
    # before PreprocessAsm.cmake ever sees the value.
    string(REPLACE ";" "|" asm_includes "${LIBFFI_INCLUDE_DIRS}")
    foreach(entry IN LISTS LIBFFI_TARGET_ASM)
      get_filename_component(stem "${entry}" NAME_WE)
      set(asm_out "${CMAKE_CURRENT_BINARY_DIR}/${stem}.asm")
      add_custom_command(
        OUTPUT "${asm_out}"
        COMMAND "${CMAKE_COMMAND}"
                "-DCL=${CMAKE_C_COMPILER}"
                "-DSRC=${CMAKE_CURRENT_SOURCE_DIR}/src/${FFI_TARGETDIR}/${entry}"
                "-DOUT=${asm_out}"
                "-DINCLUDES=${asm_includes}"
                "-DDEFINES=FFI_STATIC_BUILD"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreprocessAsm.cmake"
        DEPENDS
          "${CMAKE_CURRENT_SOURCE_DIR}/src/${FFI_TARGETDIR}/${entry}"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PreprocessAsm.cmake"
          ${LIBFFI_GENERATED_HEADERS}
        COMMENT "Preprocessing ${entry} for MASM"
        VERBATIM)
      list(APPEND LIBFFI_ASSEMBLED "${asm_out}")
    endforeach()
  else()
    enable_language(ASM)
    foreach(entry IN LISTS LIBFFI_TARGET_ASM)
      list(APPEND LIBFFI_ASSEMBLED
        "${CMAKE_CURRENT_SOURCE_DIR}/src/${FFI_TARGETDIR}/${entry}")
    endforeach()
  endif()
endif()

# ---------------------------------------------------------------------------
# The library
#
# Upstream builds both `libffi.la` and `libffi_convenience.la` from the same
# object list; the convenience archive is what an in-tree consumer links, and it
# is the one this translation produces.
# ---------------------------------------------------------------------------

add_library(libffi STATIC ${LIBFFI_SOURCES} ${LIBFFI_ASSEMBLED})

# FFI_STATIC_BUILD is what ffi.h.in checks before it reaches for
# __declspec(dllimport): without it every consumer of the static library would
# import symbols that are not in a DLL.  PUBLIC, because the macro is part of
# the header contract and every translation unit that includes <ffi.h> needs it.
target_compile_definitions(libffi PUBLIC FFI_STATIC_BUILD)

target_include_directories(libffi
  PUBLIC
    $<BUILD_INTERFACE:${LIBFFI_INCLUDE_DIR}>
  PRIVATE
    ${LIBFFI_INCLUDE_DIRS})

# `-DFFI_DEBUG` goes on the command line rather than into fficonfig.h so that,
# when building with MSVC, the debug CRT is picked up too (Makefile.am).
if(FFI_ENABLE_DEBUG)
  target_compile_definitions(libffi PRIVATE FFI_DEBUG)
endif()

# `--disable-structs` / `--disable-raw-api`.
if(NOT FFI_ENABLE_STRUCTS)
  target_compile_definitions(libffi PRIVATE FFI_NO_STRUCTS)
endif()
if(NOT FFI_ENABLE_RAW_API)
  target_compile_definitions(libffi PRIVATE FFI_NO_RAW_API)
endif()

# The legacy C in this tree predates MSVC's stricter defaults; keep the warnings
# that matter and drop the ones the upstream build never sees.
# (`_CRT_SECURE_NO_WARNINGS` and the runtime library come from the top-level
# CMakeLists.txt, which sets them for every subdirectory.)
if(MSVC)
  target_compile_options(libffi PRIVATE /W3)
endif()

set_target_properties(libffi PROPERTIES OUTPUT_NAME libffi)

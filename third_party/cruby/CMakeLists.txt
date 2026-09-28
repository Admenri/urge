cmake_minimum_required(VERSION 3.30)

project(CRUBY_193 LANGUAGES C)

include(cmake/GenerateConfig.cmake)

set(RUBY_SRCDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(RUBY_BINDIR ${CMAKE_CURRENT_BINARY_DIR})

# Core VM objects (mirrors common.mk COMMONOBJS)
set(COMMONOBJS
  array.c
  bignum.c
  class.c
  compar.c
  complex.c
  dir.c
  dln_find.c
  enum.c
  enumerator.c
  error.c
  eval.c
  load.c
  proc.c
  file.c
  gc.c
  hash.c
  inits.c
  io.c
  marshal.c
  math.c
  node.c
  numeric.c
  object.c
  pack.c
  parse.c
  process.c
  random.c
  range.c
  rational.c
  re.c
  regcomp.c
  regenc.c
  regerror.c
  regexec.c
  regparse.c
  regsyntax.c
  ruby.c
  safe.c
  signal.c
  sprintf.c
  st.c
  strftime.c
  string.c
  struct.c
  time.c
  transcode.c
  util.c
  variable.c
  compile.c
  debug.c
  iseq.c
  vm.c
  vm_dump.c
  thread.c
  cont.c
)

# Missing-function replacements.  addr2line.c provides the ELF backtrace
# helper and #errors when compiled without USE_ELF, so it is only built on
# non-Windows (ELF) targets.  Each missing/*.c fallback is only built for a
# function the target does not provide; the HAVE_<...> variables are set by
# cmake/GenerateConfig.cmake from the same probes that emit config.h (mirrors
# autoconf's LIBOBJS selection).
if(WIN32)
  set(MISSINGOBJS)
else()
  set(MISSINGOBJS addr2line.c)
endif()
if(NOT HAVE_STRLCPY)
  list(APPEND MISSINGOBJS missing/strlcpy.c)
endif()
if(NOT HAVE_STRLCAT)
  list(APPEND MISSINGOBJS missing/strlcat.c)
endif()
if(NOT HAVE_FFS)
  list(APPEND MISSINGOBJS missing/ffs.c)
endif()
if(NOT HAVE_SETPROCTITLE)
  list(APPEND MISSINGOBJS missing/setproctitle.c)
endif()
if(NOT HAVE_ACOSH)
  list(APPEND MISSINGOBJS missing/acosh.c)
endif()
if(NOT HAVE_ALLOCA)
  list(APPEND MISSINGOBJS missing/alloca.c)
endif()
if(NOT HAVE_CBRT)
  list(APPEND MISSINGOBJS missing/cbrt.c)
endif()
if(NOT HAVE_DUP2)
  list(APPEND MISSINGOBJS missing/dup2.c)
endif()
if(NOT HAVE_ERF)
  list(APPEND MISSINGOBJS missing/erf.c)
endif()
if(NOT HAVE_FINITE)
  list(APPEND MISSINGOBJS missing/finite.c)
endif()
if(NOT HAVE_FLOCK)
  list(APPEND MISSINGOBJS missing/flock.c)
endif()
if(NOT HAVE_HYPOT)
  list(APPEND MISSINGOBJS missing/hypot.c)
endif()
if(NOT HAVE_ISINF)
  list(APPEND MISSINGOBJS missing/isinf.c)
endif()
if(NOT HAVE_ISNAN)
  list(APPEND MISSINGOBJS missing/isnan.c)
endif()
if(NOT HAVE_LGAMMA_R)
  list(APPEND MISSINGOBJS missing/lgamma_r.c)
endif()
if(NOT HAVE_MEMCMP)
  list(APPEND MISSINGOBJS missing/memcmp.c)
endif()
if(NOT HAVE_MEMMOVE)
  list(APPEND MISSINGOBJS missing/memmove.c)
endif()
if(NOT HAVE_SIGNBIT)
  list(APPEND MISSINGOBJS missing/signbit.c)
endif()
if(NOT HAVE_STRCHR)
  list(APPEND MISSINGOBJS missing/strchr.c)
endif()
if(NOT HAVE_STRERROR)
  list(APPEND MISSINGOBJS missing/strerror.c)
endif()
if(NOT HAVE_STRSTR)
  list(APPEND MISSINGOBJS missing/strstr.c)
endif()
if(NOT HAVE_STRTOL)
  list(APPEND MISSINGOBJS missing/strtol.c)
endif()
# nl_langinfo fallback, only when <langinfo.h> is absent.
if(NOT HAVE_LANGINFO_H)
  list(APPEND MISSINGOBJS missing/langinfo.c)
endif()
# crypt lives in libcrypt on modern glibc; when it is not found in any library
# the bundled missing/crypt.c implementation is used instead.
if(RUBY_NEEDS_CRYPT)
  list(APPEND MISSINGOBJS missing/crypt.c)
endif()
# Windows needs the win32 portability layer (win32/win32.c + win32/file.c),
# exactly like the official win32 build's MISSING list.
if(WIN32)
  list(APPEND MISSINGOBJS win32/win32.c win32/file.c)
endif()

# Builtin encodings (enc/Makefile.in BUILTIN_ENCS) + newline transcoder,
# plus the encoding/transcoder *name* databases (encdb.h / transdb.h, both
# pre-generated in the tree) that declare every Encoding::* constant.
# A few more encodings/transcoders are statically linked as well (the official
# build ships them as loadable enc/*.so modules, which this build does not
# produce): UTF-16/32 are required by the Windows console writer and GBK is the
# Chinese (CP936) locale encoding.
set(ENCOBJS
  enc/ascii.c
  enc/us_ascii.c
  enc/utf_8.c
  enc/unicode.c
  enc/encdb.c
  enc/trans/transdb.c
  enc/utf_16le.c
  enc/utf_16be.c
  enc/utf_32le.c
  enc/utf_32be.c
  enc/gbk.c
  enc/trans/utf_16_32.c
  enc/trans/single_byte.c
  enc/trans/gbk.c
  enc/trans/chinese.c
  newline.c
)

# Init stubs and minimal prelude
set(INITOBJS
  dmyext.c
  miniprelude.c
)

# Every source compiled into ruby
# = EXPORTOBJS (dln/encoding/version + COMMONOBJS) + MISSING + ENC + INIT + main
set(RUBY_SOURCES
  ${COMMONOBJS}
  encoding.c
  version.c
  dln.c
  ${MISSINGOBJS}
  ${ENCOBJS}
  ${INITOBJS}
  main.c
)

# ---- Interpreter static library (mirrors the official libruby-static) ----
# All interpreter objects except main.c.  main() belongs to an executable, so
# it stays out of the library: other programs can link ruby-static and provide
# their own entry point (calling ruby_sysinit/ruby_init/ruby_run_node).
set(RUBY_STATIC_SOURCES ${RUBY_SOURCES})
list(REMOVE_ITEM RUBY_STATIC_SOURCES main.c)

add_library(ruby-static STATIC ${RUBY_STATIC_SOURCES})

# PUBLIC so that any consumer (ruby, or a program embedding the interpreter)
# automatically gets the generated config.h and the ruby headers too.
target_include_directories(ruby-static PUBLIC
  ${RUBY_BINDIR}/include
  ${RUBY_SRCDIR}/include
  ${RUBY_SRCDIR}
  ${RUBY_SRCDIR}/missing
)

# RUBY_EXPORT is defined by the official autoconf build on POSIX too; it just
# makes RUBY_EXTERN fall back to plain `extern` (relevant for extension ABI).
target_compile_definitions(ruby-static PRIVATE RUBY_EXPORT)

# RUBY_DEBUG_ENV debug flags
target_compile_definitions(ruby-static PRIVATE RUBY_DEBUG_ENV)

# Make ONLY the extra encoding sources generate Init_<encoding>() (which call
# rb_enc_register) instead of plain oniguruma tables, so they can be registered
# at startup (dmyext.c).  The core encodings (ascii/utf_8/unicode/...) must NOT
# get this flag: it makes their OnigEncoding tables `static`, which would break
# encoding.c's references to OnigEncodingUTF_8/US_ASCII/ASCII.  enc/gbk.c is also
# excluded: its transcoder sibling (enc/trans/gbk.c) already owns Init_gbk, so
# the GBK encoding table is registered explicitly in dmyext.c instead.
set_source_files_properties(
  enc/utf_16le.c enc/utf_16be.c enc/utf_32le.c enc/utf_32be.c
  PROPERTIES COMPILE_DEFINITIONS "ONIG_ENC_REGISTER=rb_enc_register")

if(MSVC)
  # Same feature macros the win32 build uses (win32/Makefile.sub COMMON_MACROS
  # + ARCHDEFS); _CRT_*_NO_WARNINGS keep the legacy C89 code quiet.
  target_compile_definitions(ruby-static PRIVATE
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0601
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  # Ruby 1.9.3 (2011) predates modern GCC/Clang defaults; use the original
  # gnu89 dialect and silence the newer strictness so the legacy code builds.
  target_compile_options(ruby-static PRIVATE
    -std=gnu89
    -fno-strict-aliasing
    -Wno-implicit-function-declaration
    -Wno-int-conversion
    -Wno-incompatible-pointer-types
    -Wno-discarded-qualifiers
    -Wno-implicit-int
    -Wno-deprecated-declarations
    -Wno-unused-but-set-variable
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-maybe-uninitialized
    -Wno-address
    -Wno-return-type
  )
endif()

# The `ruby` driver executable: just main() linked against the interpreter
# library (same layout as the official build: ruby = main.o + libruby-static).
add_executable(ruby main.c)
target_link_libraries(ruby PRIVATE ruby-static)

# Windows links the CRT plus these system libraries (see win32/Makefile.sub
# LIBS): ws2_32 (sockets), imagehlp (StackWalk backtrace), shlwapi/advapi32/
# shell32/user32 (console/shell helpers), oldnames (POSIX-name compat).
# Everything else is linked only when the configure probes in
# cmake/GenerateConfig.cmake found it on the target.
#
# These are attached to the static library: a static library is not linked at
# build time, so CMake forwards its (even PRIVATE) link dependencies to every
# consumer -- ruby and any program embedding the interpreter.
if(WIN32)
  target_link_libraries(ruby-static PRIVATE ws2_32 imagehlp shlwapi advapi32 shell32 user32 oldnames)
else()
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(ruby-static PRIVATE Threads::Threads m)
  if(RUBY_HAVE_LIBDL)
    target_link_libraries(ruby-static PRIVATE dl)
  endif()
  if(RUBY_HAVE_LIBRT)
    target_link_libraries(ruby-static PRIVATE rt)
  endif()
  if(RUBY_HAVE_LIBCRYPT)
    target_link_libraries(ruby-static PRIVATE crypt)
  endif()
endif()

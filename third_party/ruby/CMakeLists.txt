# ---------------------------------------------------------------------------
# Ruby 4.0 -- CMake build system
#
# This is a from-scratch replacement for Ruby's GNU make based build system
# (common.mk / defs/gmake.mk / template/Makefile.in / enc/Makefile.in).
#
# Design goals (see CMAKE.md):
#   * no `make`, no `configure`, no shell scripts are required to build;
#   * platform capabilities are detected by CMake feature tests instead of
#     hard-coded `#ifdef __linux__` style switches, so the tree can be built
#     on an unknown POSIX system that only provides a C library;
#   * the interpreter is bootstrapped in two stages, exactly like the
#     original build: miniruby (no builtin binary, no extensions) is used to
#     generate builtin_binary.rbbin, then the real ruby is linked;
#   * bundled gems and C extensions are not part of this build (core only).
#
# Layout:
#   cmake/RubySources.cmake    -- source file lists (mirrors common.mk)
#   cmake/RubyUnix.cmake       -- POSIX feature detection -> ruby/config.h
#   cmake/RubyMSVC.cmake       -- MSVC / Windows configuration
#   cmake/RubyGenerators.cmake -- generated headers and build-time scripts
#   cmake/config.h.in          -- POSIX config.h template
#   cmake/config_win32.h.in    -- MSVC config.h template
#   cmake/rbconfig.rb.in       -- RbConfig template
#   cmake/verconf.h.in         -- compiled-in load path template
#   cmake/tools/cgi/escape.rb  -- pure-Ruby shim so ERB works without exts
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.20)

project(Ruby
  VERSION 4.0.0
  DESCRIPTION "Ruby programming language"
  HOMEPAGE_URL "https://www.ruby-lang.org/"
  LANGUAGES C)

# This build system was written for a standalone checkout, where the tree root
# is also the top-level project.  When it is embedded through
# `add_subdirectory(third_party/ruby)` that is no longer true: CMAKE_SOURCE_DIR
# and CMAKE_BINARY_DIR then name the *host* project, and every source path the
# build system derives from them lands outside the Ruby tree.  The two
# variables below are what the rest of the build uses instead; standalone they
# hold exactly what the CMAKE_* ones would, so both layouts behave the same.
set(RUBY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(RUBY_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")

if(MSVC)
  # Ruby's Windows port reaches into the CRT's file-descriptor table
  # (`__pioinfo`, see win32/win32.c) to associate sockets with fds and to
  # emulate fcntl.  That table lives in ucrtbase.dll, and the interpreter finds
  # it by pattern-scanning `_isatty` *inside that DLL* -- so the CRT Ruby runs
  # on has to be the DLL one.  With the static CRT the scan resolves the DLL's
  # table while `_open` allocates fds from the image's own table: the two never
  # agree, and the first `File.open` dies in `rb_maygvl_fd_fix_cloexec` with
  # EBADF.  The host project already builds the DLL runtime for this reason;
  # say so here rather than let a future /MT switch surface as a runtime crash.
  if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  elseif(NOT CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
    message(FATAL_ERROR
      "Ruby 4.0 requires the DLL CRT (CMAKE_MSVC_RUNTIME_LIBRARY must end in "
      "'DLL'); the static CRT breaks its file-descriptor handling. "
      "Current value: ${CMAKE_MSVC_RUNTIME_LIBRARY}")
  endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(RUBY_ENABLE_YJIT "Build YJIT (Rust based JIT)" OFF)
option(RUBY_ENABLE_ZJIT "Build ZJIT (Rust based JIT)" OFF)
option(RUBY_USE_GMP "Use GMP for Bignum when available" OFF)
option(RUBY_BUILD_SMOKE_TEST "Register the single smoke test with CTest" ON)
option(RUBY_INSTALL_RULES "Generate install() rules" ON)
option(RUBY_DISABLE_RUBYGEMS
  "Do not load RubyGems at startup (required for a core-only build)" ON)

set(RUBY_BASE_NAME "ruby" CACHE STRING "Base name of the interpreter binary")
set(RUBY_LIB_VERSION "${PROJECT_VERSION}" CACHE STRING "Ruby library version (lib/ruby/<version>)")
set(RUBY_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE PATH "Install prefix compiled into the interpreter")

if(RUBY_ENABLE_YJIT OR RUBY_ENABLE_ZJIT)
  message(FATAL_ERROR
    "YJIT/ZJIT are not supported by the CMake build. "
    "Configure with -DRUBY_ENABLE_YJIT=OFF -DRUBY_ENABLE_ZJIT=OFF.")
endif()

# Ruby sources are C99 with a couple of C11 conveniences.
#
# MSVC is deliberately left in its legacy C mode, exactly like the nmake build
# in win32/Makefile.sub.  With /std:c11 MSVC treats enum bitfields as signed
# and sign-extends them on read, which corrupts Ruby's 4-bit method-type
# bitfield (VM_METHOD_TYPE_OPTIMIZED reads back as -7).
if(MSVC)
  set(CMAKE_C_STANDARD 90)
else()
  set(CMAKE_C_STANDARD 11)
endif()
set(CMAKE_C_STANDARD_REQUIRED OFF)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# Platform / architecture naming (used for RbConfig and include paths)
# ---------------------------------------------------------------------------
include(RubyPlatform)

# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------
include(RubySources)

# ---------------------------------------------------------------------------
# Platform configuration: generates <build>/include/ruby/config.h
# ---------------------------------------------------------------------------
if(WIN32)
  # Shared Windows pieces (win32/ layer, Win32 threads, coroutine backend).
  include(RubyWindows)
endif()

if(MSVC)
  include(RubyMSVC)
else()
  include(RubyUnix)
endif()

# Coroutine backend and missing/ replacements are chosen by the platform
# module; add them to the common object set.
list(APPEND RUBY_CORE_SOURCES ${RUBY_PLATFORM_SOURCES})

if(MSVC)
  set(RUBY_EXTRA_DEFINITIONS ${RUBY_MSVC_DEFINITIONS})
else()
  set(RUBY_EXTRA_DEFINITIONS "")
endif()

# ---------------------------------------------------------------------------
# Generated sources (rbconfig.rb, verconf.h, builtin_binary.rbbin)
# ---------------------------------------------------------------------------
include(RubyGenerators)

# ---------------------------------------------------------------------------
# Common compiler settings
# ---------------------------------------------------------------------------
set(RUBY_INCLUDE_DIRS
  "${RUBY_BINARY_DIR}/include"   # ruby/config.h
  "${RUBY_BINARY_DIR}"           # verconf.h, builtin_binary.rbbin, rbconfig.rb
  "${RUBY_SOURCE_DIR}"
  "${RUBY_SOURCE_DIR}/include"
  "${RUBY_SOURCE_DIR}/prism"
  "${RUBY_SOURCE_DIR}/prism/util"
  "${RUBY_SOURCE_DIR}/enc/unicode/17.0.0"  # casefold.h, name2ctype.h
)

# Objects shared by miniruby and ruby (COMMONOBJS in common.mk).
add_library(rubycore OBJECT ${RUBY_CORE_SOURCES})
target_include_directories(rubycore PUBLIC ${RUBY_INCLUDE_DIRS})
target_compile_definitions(rubycore PRIVATE RUBY_EXPORT ${RUBY_EXTRA_DEFINITIONS})

# Built-in encodings are linked into the interpreter, so they must define the
# classic OnigEncoding* symbols (encoding.c refers to them directly).  The
# -DONIG_ENC_REGISTER=... used by the original build is only for the shared
# encoding objects, which this build does not create.

# The coroutine assembly needs the platform's symbol prefix (see
# template/Makefile.in: PREFIXED_SYMBOL(name) is name, or _name on Mach-O).
# It is injected as a header because CMake cannot pass definitions that
# contain parentheses.
if(RUBY_COROUTINE_SRC MATCHES "\\.S$")
  target_compile_options(rubycore PRIVATE
    "$<$<COMPILE_LANGUAGE:ASM>:-include${RUBY_SOURCE_DIR}/cmake/coroutine_prefix.h>")
endif()

ruby_apply_compile_options(rubycore)

# ---------------------------------------------------------------------------
# miniruby -- bootstrap interpreter.
#
# It contains no builtin binary and no extension loading, and provides its own
# Init_enc/Init_ext (miniinit.c).  It is used to generate builtin_binary.rbbin.
#
# On the web its garbage collector finds the objects a C frame is holding by
# scanning the machine stack and the wasm locals -- `emscripten_scan_stack` /
# `emscripten_scan_registers` -- and those are Asyncify operations, so the tool
# needs `-sASYNCIFY`.  That is why the module uses JS-based exceptions rather
# than wasm ones; see the top-level CMakeLists.txt.
#
# EXCLUDE_FROM_ALL because it is a build tool, not an artefact of the library:
# it exists only to generate builtin_binary.rbbin, and only the build that has
# to generate one needs it.  With RUBY_BUILTIN_BINARY_PREBUILT set -- which is
# how a cross build works, miniruby being a program for the target -- nothing
# depends on the output, so nothing builds it.  A normal build still gets it,
# because the rbbin does depend on it.
# ---------------------------------------------------------------------------
add_executable(miniruby EXCLUDE_FROM_ALL
  $<TARGET_OBJECTS:rubycore>
  "${RUBY_SOURCE_DIR}/main.c"
  "${RUBY_SOURCE_DIR}/miniinit.c"
  "${RUBY_SOURCE_DIR}/dmydln.c"   # ARCHMINIOBJS: no dynamic loading in miniruby
)
target_include_directories(miniruby PRIVATE ${RUBY_INCLUDE_DIRS})
target_compile_definitions(miniruby PRIVATE RUBY_EXPORT ${RUBY_EXTRA_DEFINITIONS})
ruby_apply_compile_options(miniruby)
ruby_link_libraries(miniruby)

if(EMSCRIPTEN)
  # miniruby is a build tool, not part of the game: it is run under node to
  # generate builtin_binary.rbbin.  NODERAWFS is what lets it read the source
  # tree and write its output without the whole of it being preloaded into the
  # module, and the heap has to be able to grow -- parsing the builtin Ruby
  # files does not fit in the default 16 MiB.  The pre-js shim is for the
  # NODERAWFS `fstat` that traps on the three stdio streams, see the file.
  #
  # ASYNCIFY is not optional either: the conservative GC finds the objects a C
  # frame is holding by scanning the stack and the wasm locals, which is
  # `emscripten_scan_stack` / `emscripten_scan_registers`, which are Asyncify
  # operations.  Without it every collection aborts with "Please compile your
  # program with async support".  The directory drops `-fwasm-exceptions` above,
  # which cannot be mixed with Asyncify.
  target_link_options(miniruby PRIVATE
    "-sNODERAWFS=1"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=134217728"
    # Prism compiles a file by recursing over its nodes, and the builtin Ruby
    # files nest deep enough that the 64 KiB default stack is exhausted halfway
    # through -- and a wasm stack overflow does not trap, it walks off the end
    # and writes over the heap, which surfaces later as a divide by zero inside
    # the allocator.  The engine sets the same 16 MiB for the same reason.
    "-sSTACK_SIZE=16777216"
    "-sASYNCIFY"
    "-sASYNCIFY_STACK_SIZE=8388608"
    "--pre-js=${RUBY_SOURCE_DIR}/wasm/miniruby_pre.js")
endif()

# ---------------------------------------------------------------------------
# libruby-static -- the interpreter without main(), plus the builtin binary.
# ---------------------------------------------------------------------------
add_library(ruby-static STATIC
  $<TARGET_OBJECTS:rubycore>
  "${RUBY_SOURCE_DIR}/dln.c"
  "${RUBY_SOURCE_DIR}/localeinit.c"
  "${RUBY_SOURCE_DIR}/loadpath.c"
  "${RUBY_SOURCE_DIR}/builtin.c"
  # `ruby.obj` references Init_enc/Init_ext even when the host never calls
  # `ruby_options()`, because they are part of the start-up path it compiles
  # in.  The standalone executable used to carry the two stubs on its own, so
  # anything that linked `ruby-static` without also linking `ruby` -- an
  # embedding host is exactly that -- failed with LNK2019.  They belong to the
  # interpreter, not to its host, so they live in the library.
  "${RUBY_SOURCE_DIR}/dmyext.c"
  "${RUBY_SOURCE_DIR}/cmake/encinit_builtin.c"
)
set_target_properties(ruby-static PROPERTIES OUTPUT_NAME "${RUBY_BASE_NAME}-static")
target_include_directories(ruby-static PUBLIC ${RUBY_INCLUDE_DIRS})
target_compile_definitions(ruby-static PRIVATE RUBY_EXPORT ${RUBY_EXTRA_DEFINITIONS})
# `RUBY_EXPORT` is what the interpreter's own translation units see: on Windows
# it makes RUBY_EXTERN a plain `extern`, so the symbols are defined rather than
# imported.  Everything that *consumes* this library needs the same view of
# those declarations -- without it MSVC reads `extern __declspec(dllimport)`
# from ruby.h and emits an `__imp_rb_cObject`-style reference for every global
# the binding touches, which the linker then satisfies from the static archive
# instead of a DLL (LNK4217 / LNK4286, one per symbol per object).  `RUBY_STATIC`
# is the consumer-side spelling of the same thing: see
# include/ruby/internal/dllexport.h.
target_compile_definitions(ruby-static PUBLIC RUBY_STATIC)
ruby_apply_compile_options(ruby-static)
ruby_link_libraries(ruby-static)

# builtin.c does `#include "builtin_binary.rbbin"`, a generated file.
ruby_generate_builtin_binary()
set_source_files_properties("${RUBY_SOURCE_DIR}/builtin.c" PROPERTIES
  OBJECT_DEPENDS "${RUBY_BUILTIN_BINARY_RBBIN}")

# ---------------------------------------------------------------------------
# ruby -- the interpreter.
#
# Init_enc/Init_ext come from ruby-static now (see above), so the executable is
# just the entry point.
# ---------------------------------------------------------------------------
add_executable(ruby
  "${RUBY_SOURCE_DIR}/main.c"
)
target_include_directories(ruby PRIVATE ${RUBY_INCLUDE_DIRS})
target_compile_definitions(ruby PRIVATE RUBY_EXPORT ${RUBY_EXTRA_DEFINITIONS})
ruby_apply_compile_options(ruby)
target_link_libraries(ruby PRIVATE ruby-static)
ruby_link_libraries(ruby)
add_dependencies(ruby ruby-static)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
if(RUBY_INSTALL_RULES)
  include(GNUInstallDirs)
  install(TARGETS ruby RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
  install(FILES "${RUBY_BINARY_DIR}/rbconfig.rb"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/ruby/${RUBY_LIB_VERSION}/${RUBY_ARCH}")
  install(DIRECTORY "${RUBY_SOURCE_DIR}/lib/"
          DESTINATION "${CMAKE_INSTALL_LIBDIR}/ruby/${RUBY_LIB_VERSION}"
          FILES_MATCHING PATTERN "*.rb")
  install(DIRECTORY "${RUBY_SOURCE_DIR}/include/"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ruby-${RUBY_LIB_VERSION}")
  install(FILES "${RUBY_BINARY_DIR}/include/ruby/config.h"
          DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ruby-${RUBY_LIB_VERSION}/${RUBY_ARCH}/ruby")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(RUBY_BUILD_SMOKE_TEST)
  enable_testing()
  add_test(NAME ruby_smoke
           COMMAND $<TARGET_FILE:ruby> -I "${RUBY_SOURCE_DIR}/lib" "${RUBY_SOURCE_DIR}/smoke_test.rb")
  set_tests_properties(ruby_smoke PROPERTIES TIMEOUT 120)
endif()

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "Ruby ${PROJECT_VERSION} -- CMake build")
message(STATUS "  system          : ${CMAKE_SYSTEM_NAME} (${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "  arch            : ${RUBY_ARCH}")
message(STATUS "  compiler        : ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "  build type      : ${CMAKE_BUILD_TYPE}")
message(STATUS "  thread model    : ${RUBY_THREAD_MODEL}")
message(STATUS "  coroutine       : ${RUBY_COROUTINE_SRC}")
message(STATUS "  install prefix  : ${RUBY_PREFIX}")
message(STATUS "  YJIT / ZJIT     : disabled")
message(STATUS "")

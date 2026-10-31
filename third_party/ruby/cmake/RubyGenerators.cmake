# ---------------------------------------------------------------------------
# RubyGenerators.cmake -- files generated at configure / build time
#
#   rbconfig.rb           runtime configuration (RbConfig)
#   verconf.h             compiled-in load path (loadpath.c)
#   builtin_binary.rbbin  pre-compiled builtin Ruby code (builtin.c)
#
# Everything else the original build generates (parse.c, miniprelude.c,
# *.rbinc, encindex.h, unicode tables, enc/trans/*.c) is already part of the
# source tree, so no Ruby script is needed to configure the build.
# ---------------------------------------------------------------------------

# --- values shared by the generated files ---------------------------------
if(WIN32)
  set(RUBY_EXEEXT ".exe")
  set(RUBY_DLEXT "so")
  set(RUBY_SOEXT "dll")
elseif(APPLE)
  set(RUBY_EXEEXT "")
  set(RUBY_DLEXT "bundle")
  set(RUBY_SOEXT "dylib")
else()
  set(RUBY_EXEEXT "")
  set(RUBY_DLEXT "so")
  set(RUBY_SOEXT "so")
endif()

set(RUBY_PROGRAM_VERSION "${PROJECT_VERSION}")
set(RUBY_LIB_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(RUBY_LIB_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(RUBY_LIB_VERSION_TEENY "${PROJECT_VERSION_PATCH}")

string(TOLOWER "${CMAKE_SYSTEM_NAME}" RUBY_HOST_OS)
if(RUBY_HOST_OS STREQUAL "windows")
  set(RUBY_HOST_OS "mingw32")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" RUBY_HOST_CPU)
if(WIN32)
  set(RUBY_PATH_SEPARATOR ";")
else()
  set(RUBY_PATH_SEPARATOR ":")
endif()

set(RUBY_CC "${CMAKE_C_COMPILER}")
set(RUBY_AR "${CMAKE_AR}")
set(RUBY_RANLIB "${CMAKE_RANLIB}")
if(MSVC)
  if(CMAKE_MSVC_RUNTIME_LIBRARY MATCHES "DLL")
    set(RUBY_CFLAGS "/O2 /MD")
  else()
    set(RUBY_CFLAGS "/O2 /MT")
  endif()
  set(RUBY_LDFLAGS "-incremental:no -debug -opt:ref -opt:icf")
else()
  set(RUBY_CFLAGS "-fno-strict-overflow -fvisibility=hidden")
  set(RUBY_LDFLAGS "")
endif()
string(REPLACE ";" " " RUBY_LINK_LIBS_STR "${RUBY_LINK_LIBS}")
string(REPLACE ";" " " RUBY_CONFIGURE_ARGS_STR "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}")

# --- verconf.h ------------------------------------------------------------
# loadpath.c uses these to build the default $LOAD_PATH.
set(RUBY_LIB_PREFIX "${RUBY_PREFIX}/lib/ruby")
configure_file("${CMAKE_CURRENT_LIST_DIR}/verconf.h.in"
               "${RUBY_BINARY_DIR}/verconf.h"
               @ONLY)

# --- rbconfig.rb ----------------------------------------------------------
configure_file("${CMAKE_CURRENT_LIST_DIR}/rbconfig.rb.in"
               "${RUBY_BINARY_DIR}/rbconfig.rb"
               @ONLY)

# --- encdb.h --------------------------------------------------------------
# Generated once from the built-in encoding sources (enc/ascii.c,
# enc/us_ascii.c, enc/unicode.c, enc/utf_8.c).  It only lists encoding names,
# aliases and replicas, so it does not depend on the target platform; keeping
# a copy in cmake/ means the build does not need a Ruby interpreter to
# configure itself.
configure_file("${CMAKE_CURRENT_LIST_DIR}/encdb.h"
               "${RUBY_BINARY_DIR}/encdb.h"
               COPYONLY)

# --- builtin_binary.rbbin -------------------------------------------------
set(RUBY_BUILTIN_BINARY_RBBIN "${RUBY_BINARY_DIR}/builtin_binary.rbbin")

# A cross build cannot run the miniruby it just built: that one is a program for
# the *target*, so a Windows host is handed an aarch64 ELF it has no way to
# execute.  RUBY_BUILTIN_BINARY_PREBUILT names a file a host build already
# produced, and the build copies it instead of generating one.
#
# What the file contains is the compiled form of the builtin Ruby sources
# (array.rb, kernel.rb, ...), which depends on the interpreter's word size and
# byte order but not on its operating system -- so a 64-bit little-endian host's
# file is exactly what a 64-bit little-endian target needs.  A 32-bit target
# would need its own; that is one of the reasons the Android port is 64-bit only
# for now.
set(RUBY_BUILTIN_BINARY_PREBUILT "" CACHE FILEPATH
  "Pre-generated builtin_binary.rbbin for a cross build, where miniruby is a program for the target; the tree keeps one at third_party/ruby/prebuilt/builtin_binary-lp64.rbbin")

# miniruby is a native executable everywhere except on the web, where the only
# thing the toolchain produces is a JavaScript module.  It is still a build
# tool, so it runs on the build machine -- under node, which the Emscripten
# toolchain already needs.
set(RUBY_MINIRUBY_RUNNER "")
if(EMSCRIPTEN)
  find_program(RUBY_JS_RUNNER NAMES node nodejs)
  if(NOT RUBY_JS_RUNNER)
    message(FATAL_ERROR
      "The Emscripten build generates builtin_binary.rbbin by running miniruby "
      "under node, and no node executable was found.")
  endif()
  set(RUBY_MINIRUBY_RUNNER "${RUBY_JS_RUNNER}")
endif()

# Ruby's own path handling is POSIX: "J:/x" does not start with a slash, so it
# is a *relative* path to the interpreter and gets joined onto the working
# directory.  NODERAWFS resolves a drive-less path against the current drive,
# which is exactly where it came from, so the build machine's drive letter is
# dropped from every path miniruby is handed.
function(ruby_host_tool_path out path)
  if(EMSCRIPTEN)
    string(REGEX REPLACE "^[A-Za-z]:" "" _path "${path}")
    string(REPLACE "\\" "/" _path "${_path}")
    set(${out} "${_path}" PARENT_SCOPE)
  else()
    set(${out} "${path}" PARENT_SCOPE)
  endif()
endfunction()

function(ruby_generate_builtin_binary)
  if(RUBY_BUILTIN_BINARY_PREBUILT)
    if(NOT EXISTS "${RUBY_BUILTIN_BINARY_PREBUILT}")
      message(FATAL_ERROR
        "RUBY_BUILTIN_BINARY_PREBUILT is '${RUBY_BUILTIN_BINARY_PREBUILT}', "
        "which does not exist. Generate it with a host build first "
        "(cmake --build --preset win64-vendored-ruby --target miniruby, then run "
        "miniruby on third_party/ruby/tool/generic_erb.rb), or use the copy the "
        "tree keeps at third_party/ruby/prebuilt/builtin_binary.rbbin.")
    endif()

    add_custom_command(
      OUTPUT "${RUBY_BUILTIN_BINARY_RBBIN}"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "${RUBY_BUILTIN_BINARY_PREBUILT}" "${RUBY_BUILTIN_BINARY_RBBIN}"
      DEPENDS "${RUBY_BUILTIN_BINARY_PREBUILT}"
      COMMENT "Using the pre-generated builtin_binary.rbbin"
      VERBATIM)
    return()
  endif()

  ruby_host_tool_path(_lib "${RUBY_SOURCE_DIR}/lib")
  ruby_host_tool_path(_build "${RUBY_BINARY_DIR}")
  ruby_host_tool_path(_tool "${RUBY_SOURCE_DIR}/tool/generic_erb.rb")
  ruby_host_tool_path(_template "${RUBY_SOURCE_DIR}/template/builtin_binary.rbbin.tmpl")
  ruby_host_tool_path(_output "${RUBY_BUILTIN_BINARY_RBBIN}")

  add_custom_command(
    OUTPUT "${RUBY_BUILTIN_BINARY_RBBIN}"
    COMMAND ${RUBY_MINIRUBY_RUNNER} $<TARGET_FILE:miniruby>
            -I "${_lib}"
            -I "${_build}"
            "${_tool}"
            -o "${_output}"
            "${_template}"
    DEPENDS miniruby
            "${RUBY_SOURCE_DIR}/tool/generic_erb.rb"
            "${RUBY_SOURCE_DIR}/template/builtin_binary.rbbin.tmpl"
            ${RUBY_BUILTIN_RB_SRCS}
    COMMENT "Generating builtin_binary.rbbin with miniruby"
    VERBATIM)
endfunction()

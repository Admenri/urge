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
  set(RUBY_CFLAGS "/O2 /MD")
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

function(ruby_generate_builtin_binary)
  add_custom_command(
    OUTPUT "${RUBY_BUILTIN_BINARY_RBBIN}"
    COMMAND $<TARGET_FILE:miniruby>
            -I "${RUBY_SOURCE_DIR}/lib"
            -I "${RUBY_BINARY_DIR}"
            "${RUBY_SOURCE_DIR}/tool/generic_erb.rb"
            -o "${RUBY_BUILTIN_BINARY_RBBIN}"
            "${RUBY_SOURCE_DIR}/template/builtin_binary.rbbin.tmpl"
    DEPENDS miniruby
            "${RUBY_SOURCE_DIR}/tool/generic_erb.rb"
            "${RUBY_SOURCE_DIR}/template/builtin_binary.rbbin.tmpl"
            ${RUBY_BUILTIN_RB_SRCS}
    COMMENT "Generating builtin_binary.rbbin with miniruby"
    VERBATIM)
endfunction()

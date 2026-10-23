# ---------------------------------------------------------------------------
# RubyMSVC.cmake -- configuration for MSVC (mswin) builds
#
# The macro set is the same as the one win32/Makefile.sub generated with
# nmake; the conditional parts are resolved from CMake variables.
# ---------------------------------------------------------------------------

if(CMAKE_SIZEOF_VOIDP EQUAL 8)
  set(RUBY_MSVC_64BIT 1)
else()
  set(RUBY_MSVC_64BIT 0)
endif()

set(RUBY_MISC_DEFINES "")
if(RUBY_DISABLE_RUBYGEMS)
  string(APPEND RUBY_MISC_DEFINES "#define DISABLE_RUBYGEMS 1\n")
endif()

file(MAKE_DIRECTORY "${RUBY_BINARY_DIR}/include/ruby")
configure_file("${CMAKE_CURRENT_LIST_DIR}/config_win32.h.in"
               "${RUBY_BINARY_DIR}/include/ruby/config.h"
               @ONLY)

configure_file("${CMAKE_CURRENT_LIST_DIR}/dummy_probes.h"
               "${RUBY_BINARY_DIR}/probes.h"
               COPYONLY)

# Additional sources that MSVC needs but the POSIX build does not.
# missing/crypt.c provides crypt_r(), which string.c falls back to when the
# system has no crypt(3); missing/des_tables.c is #included by it.
list(APPEND RUBY_PLATFORM_SOURCES
  "${RUBY_SOURCE_DIR}/missing/crypt.c"
  "${RUBY_SOURCE_DIR}/missing/ffs.c"
  "${RUBY_SOURCE_DIR}/missing/langinfo.c"
  "${RUBY_SOURCE_DIR}/missing/lgamma_r.c"
  "${RUBY_SOURCE_DIR}/missing/strlcat.c"
  "${RUBY_SOURCE_DIR}/missing/strlcpy.c"
  "${RUBY_SOURCE_DIR}/missing/setproctitle.c"
  "${RUBY_SOURCE_DIR}/missing/explicit_bzero.c")

set(RUBY_LINK_LIBS
  user32 advapi32 shell32 ws2_32 iphlpapi imagehlp shlwapi bcrypt)

# MSVC has no <unistd.h> style configuration; everything comes from config.h.
set(RUBY_MSVC_DEFINITIONS
  WIN32_LEAN_AND_MEAN
  _CRT_SECURE_NO_WARNINGS
  _CRT_NONSTDC_NO_DEPRECATE
  RUBY_MSVC)

message(STATUS "config.h written to ${RUBY_BINARY_DIR}/include/ruby/config.h (MSVC)")

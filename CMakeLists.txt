cmake_minimum_required(VERSION 3.30)

project(URGE LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
include_directories(${PROJECT_SOURCE_DIR})

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Video playback is optional.  dav1d and libwebm are native C libraries with no
# Emscripten story, so the platform decides whether the engine is built at all.
# The default follows the platform, but an explicit -DURGE_ENABLE_VIDEO=... on
# the command line still wins.
#
# The Ruby side is not conditional: `Video` stays exported either way, and every
# call raises while the engine is off, so a script sees one API on every
# platform -- only the depth behind it differs.  See core/video.h.
if(EMSCRIPTEN)
  set(URGE_ENABLE_VIDEO_DEFAULT OFF)
else()
  set(URGE_ENABLE_VIDEO_DEFAULT ON)
endif()
option(URGE_ENABLE_VIDEO
  "Build the AV1/Vorbis video playback engine" ${URGE_ENABLE_VIDEO_DEFAULT})

# The network subsystem is on everywhere, including the web: libcurl carries
# the native side and Emscripten ships its own fetch/websocket, so neither
# platform needs a dependency the other lacks.  The switch exists so a build
# can drop the network code -- and libcurl with it -- the way the video switch
# drops dav1d.  `Network` stays exported either way and raises while it is off,
# see core/network.h.
option(URGE_ENABLE_NETWORK "Build the WebSocket and HTTP client" ON)

# ---------------------------------------------------------------------------
# The script interpreter.
#
# `bind/external` is an optional drop-in holding a *prebuilt* CRuby: headers
# under include/, an import library under lib/ and the DLLs under bin/.  When
# it is there the engine links that interpreter instead of compiling the
# bundled CRuby 1.9.3 in third_party/cruby -- and, because that drop-in is a
# CRuby built with YJIT, the host turns the JIT on (see bind/cruby_main.cc).
#
# The directory is git-ignored on purpose: which interpreter a build uses is a
# local decision, not a property of the tree.  Its absence is the default and
# means "compile third_party/cruby".  Nothing downstream has to know which
# case it is in, because both answer to the same target name, `ruby-static`.
#
# The layout is discovered rather than hard-coded, so dropping in a different
# CRuby needs no edit here -- the versioned include directory and the DLL name
# both change between releases.  The one file that does have to be kept in step
# is bind/ruby-msvc/ruby/config.h; see the comment at the top of it.
# ---------------------------------------------------------------------------
set(URGE_EXTERNAL_RUBY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bind/external")
if(EXISTS "${URGE_EXTERNAL_RUBY_DIR}/include")
  file(GLOB URGE_RUBY_INCLUDE_DIRS LIST_DIRECTORIES TRUE
    "${URGE_EXTERNAL_RUBY_DIR}/include/ruby-*")
  file(GLOB URGE_RUBY_IMPORT_LIBS "${URGE_EXTERNAL_RUBY_DIR}/lib/*.dll.a")
  file(GLOB URGE_RUBY_DLLS "${URGE_EXTERNAL_RUBY_DIR}/bin/*.dll")

  list(LENGTH URGE_RUBY_INCLUDE_DIRS URGE_RUBY_INCLUDE_COUNT)
  list(LENGTH URGE_RUBY_IMPORT_LIBS URGE_RUBY_IMPORT_COUNT)
  if(NOT URGE_RUBY_INCLUDE_COUNT EQUAL 1 OR NOT URGE_RUBY_IMPORT_COUNT EQUAL 1)
    message(FATAL_ERROR
      "bind/external is present but does not look like a CRuby drop-in: "
      "expected exactly one include/ruby-* directory and one lib/*.dll.a "
      "import library, found ${URGE_RUBY_INCLUDE_COUNT} and "
      "${URGE_RUBY_IMPORT_COUNT}.")
  endif()

  list(GET URGE_RUBY_INCLUDE_DIRS 0 URGE_RUBY_INCLUDE_DIR)
  list(GET URGE_RUBY_IMPORT_LIBS 0 URGE_RUBY_IMPORT_LIB)
  message(STATUS "URGE: interpreter = the CRuby in bind/external")
else()
  message(STATUS "URGE: interpreter = the bundled CRuby 1.9.3")
  unset(URGE_EXTERNAL_RUBY_DIR)
endif()

# ---------------------------------------------------------------------------
# The encrypted asset container.
#
# `admenri/` is a git-ignored drop-in holding the PhysicsFS archiver for the
# `.acs` container, the packer that writes one, and the container key.  Its
# presence is the whole switch: a clone of the open-source tree has no
# `admenri/`, so none of it is compiled, `core/archive.cc` answers "not
# available", and no `.acs` file is ever opened.  Nothing else in the build
# knows which case it is in -- see core/archive.h.
#
# The check is on the archiver source rather than on the directory, so an
# empty or half-copied drop-in is treated as absent instead of failing the
# build halfway through.
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/admenri/acs_archiver.cc")
  set(URGE_ENABLE_ARCHIVE_ENCRYPTION ON)
  message(STATUS "URGE: asset containers = encrypted (admenri/ drop-in found)")
else()
  set(URGE_ENABLE_ARCHIVE_ENCRYPTION OFF)
  message(STATUS "URGE: asset containers = plain (no admenri/ drop-in)")
endif()

option(URGE_BUILD_ACS_PACK "Build the .acs container packer" ON)

add_subdirectory(third_party)

add_subdirectory(core)

# The binding glue is generated from core/*.h, so it has to be brought up to
# date *before* bind/CMakeLists.txt globs it.  `include` runs here, in the
# configure step, and re-runs the two generators only when the headers (or a
# generator) actually changed -- see bind/gen_binding.cmake.  It has to be
# `include`, not `add_subdirectory`: a subdirectory gets its own variable and
# policy scope, and the `file(GLOB)` below has to see the sources this writes.
include(bind/gen_binding.cmake)

add_subdirectory(bind)
add_subdirectory(app)

# The engine, its bindings and the host are UTF-8 from end to end: the sources,
# the script runtime (the default external/internal encoding is set to UTF-8 in
# bind/cruby_main.cc), SDL and the game data are all UTF-8.  Tell MSVC so.
#
# It matters twice.  First, the execution character set decides how the
# compiler reads non-ASCII literals in the sources -- the default window title
# `(*^▽^*)` in core/config.h is written in UTF-8, and without this MSVC would
# read it through the system ANSI code page and store mojibake.  Second, and
# this is what actually bites: MSVC's <format> validates a *format string*
# against the execution character set, so on a non-UTF-8 system code page (936
# here) any UTF-8 text handed to std::format is rejected with
#   "Invalid encoded character in format string."
# See core/exception.h and core/logger.h, whose runtime messages hit exactly
# that.
#
# Applied to the three targets of this project only: third_party is upstream
# code whose own encoding is its own affair.
if(MSVC)
  target_compile_options(urge-core PRIVATE /utf-8)
  target_compile_options(urge-binding PRIVATE /utf-8)
  target_compile_options(Game PRIVATE /utf-8)
endif()

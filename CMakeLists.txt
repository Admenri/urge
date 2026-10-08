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

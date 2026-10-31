cmake_minimum_required(VERSION 3.30)

project(URGE LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
include_directories(${PROJECT_SOURCE_DIR})

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  # The static CRT is the whole point of this setting: the executable carries
  # its C runtime instead of importing vcruntime140.dll and the api-ms-win-crt
  # API sets.  Ruby needs the CRT's file-descriptor table -- its Windows port
  # reaches into `__pioinfo` (win32/win32.c) to associate sockets with fds and
  # to emulate fcntl.  With the DLL CRT that table belongs to ucrtbase.dll and
  # Ruby finds it by scanning `_isatty` in that DLL; with the static CRT the
  # table is the image's own and libucrt.lib defines `__pioinfo` outright, so
  # win32/win32.c takes the other branch.  Every target in the tree shares this
  # runtime, or allocations and file descriptors would cross two heaps.
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  # cl puts /EHsc in its default C++ flags.  The /EHs-c- this project hands its
  # own targets further down would then sit next to it on the same command
  # line, and cl reports the contradiction:
  #   cl : Command line warning D9025 : overriding '/EHs' with '/EHs-'
  # Drop the default here, where the root scope still reaches every target;
  # third_party puts it back for the upstream projects it builds.
  string(REGEX REPLACE "(^| )/EHsc( |$)" "\\1" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

if(EMSCRIPTEN)
  # The engine is single threaded, and the browser has no threads unless the
  # module is built with shared memory and atomics -- which nothing here does.
  # `OS_EMSCRIPTEN` is what the frame loop and the window code branch on.
  add_compile_definitions(OS_EMSCRIPTEN)

  # Nothing here asks for `-fexceptions` or `-fwasm-exceptions`: the engine has
  # no C++ `try`/`throw` left (see the `/EHs-c-` block at the bottom of this
  # file), so there is nothing for a C++ unwinder to do.  What still unwinds is
  # CRuby's `longjmp`, which is a separate mechanism and is untouched by this:
  # with wasm exceptions off it defaults to the JavaScript longjmp, which is
  # what Asyncify was designed against.  Asking for `SUPPORT_LONGJMP=wasm`
  # would be rejected outright -- Emscripten's wasm longjmp is built on wasm
  # exceptions, and the two cannot be mixed ("Parts of the program that mix
  # ASYNCIFY and exceptions will not compile").  Ruby does not need it either:
  # its own setjmp is the Asyncify-based one in wasm/setjmp.c.
endif()

if(ANDROID)
  # Android has no executable entry point of its own: the APK's Java side loads
  # `libmain.so` and calls `SDL_main` in it, which is either this file's `main`
  # (URGE_RUNNER_MODEL=poll) or SDL's callback entry point (fiber).
  # `OS_ANDROID` is what the engine branches on -- `core/system/filesystem.cc`
  # already carries the PhysicsFS hand-off under it, and `Graphics` uses it to
  # leave the surface's size to the system.
  add_compile_definitions(OS_ANDROID)
endif()

# Video playback is optional.  dav1d and libwebm are native C libraries with no
# Emscripten story, so the web build is the only one without the engine; every
# other platform builds it by default.  An explicit -DURGE_ENABLE_VIDEO=... on
# the command line still wins.
#
# The Ruby side is not conditional: `Video` stays exported either way, and every
# call raises while the engine is off, so a script sees one API on every
# platform -- only the depth behind it differs.  See core/video/video.h.
if(EMSCRIPTEN)
  set(URGE_ENABLE_VIDEO_DEFAULT OFF)
else()
  set(URGE_ENABLE_VIDEO_DEFAULT ON)
endif()
option(URGE_ENABLE_VIDEO
  "Build the AV1/Vorbis video playback engine" ${URGE_ENABLE_VIDEO_DEFAULT})

# The network subsystem is on everywhere, including the web: libcurl carries
# the native side and Emscripten ships its own fetch/websocket, so neither
# platform needs a dependency the other lacks.  The switch exists so a build can
# drop the network code -- and libcurl with it -- the way the video switch drops
# dav1d.  `Network` stays exported either way and raises while it is off, see
# core/network/network.h.
#
# Native builds need a TLS backend for libcurl.  Windows uses Schannel, the
# in-box Windows stack; Android, whose NDK carries no OpenSSL, uses the
# vendored mbedTLS against the system CA store; other desktop platforms link
# the system OpenSSL.  The web build has no libcurl at all.
option(URGE_ENABLE_NETWORK
  "Build the WebSocket and HTTP client" ON)

# The static CRT, the static C++ library, SDL and wgpu import a few functions
# that only exist from Windows 8 on (WaitOnAddress, ProcessPrng, CreateFile2,
# ...).  Windows 7 refuses to start the process because those imports cannot
# be resolved, even when the code behind them is never reached.  This switch
# links app/platform/win7_compat.cc, which redirects the import slots to
# in-image wrappers that forward to the real API when it exists and implement
# a fallback otherwise.  OFF keeps the import table exactly as the toolchain
# produced it.
option(URGE_WIN7_COMPAT
  "Link Windows 7 compatibility hooks for imports that need Windows 8+" OFF)
if(URGE_WIN7_COMPAT AND NOT MSVC)
  message(FATAL_ERROR "URGE_WIN7_COMPAT uses MSVC linker hooks and needs MSVC.")
endif()
message(STATUS "URGE: Windows 7 compatibility hooks = ${URGE_WIN7_COMPAT}")

# ---------------------------------------------------------------------------
# The script interpreter.
#
# There is exactly one: the CRuby in third_party/ruby, compiled from source
# by third_party/CMakeLists.txt and linked as `ruby-static`.  Nothing outside
# the tree is consulted, so a build never depends on which interpreter happens
# to be installed on the machine.
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# The encrypted asset container.
#
# `admenri/` is a git-ignored drop-in holding the PhysicsFS archiver for the
# `.acs` container, the packer that writes one, and the container key.  Its
# presence is the whole switch: a clone of the open-source tree has no
# `admenri/`, so none of it is compiled, `core/system/archive.cc` answers "not
# available", and no `.acs` file is ever opened.  Nothing else in the build
# knows which case it is in -- see core/system/archive.h.
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

# ---------------------------------------------------------------------------
# Who owns the frame loop.
#
#   poll   the engine does: `main()` runs the script, and every
#          `Graphics.update` is a whole frame -- it drains SDL's queue, draws,
#          presents and then sleeps out the rest of the game's frame slot
#          (core/graphics/frame_limiter.h).  This is the shape RGSS itself has,
#          and what any platform that lets a thread block wants.
#
#   fiber  SDL does, through its callback API: one tick is one `SDL_AppIterate`,
#          and the script runs on a stack of its own so it can survive from one
#          tick to the next, handing the thread back at every Present
#          (bind/script_loop.h).
#
# The browser is the reason `fiber` still exists: a page cannot block, so the
# engine must give the thread back to the animation frame every time, and SDL's
# callback loop is exactly that.  Everywhere else the engine may keep the
# thread, so it does.
if(NOT URGE_RUNNER_MODEL)
  if(EMSCRIPTEN)
    set(URGE_RUNNER_MODEL_DEFAULT fiber)
  else()
    set(URGE_RUNNER_MODEL_DEFAULT poll)
  endif()
else()
  set(URGE_RUNNER_MODEL_DEFAULT "${URGE_RUNNER_MODEL}")
endif()

set(URGE_RUNNER_MODEL "${URGE_RUNNER_MODEL_DEFAULT}" CACHE STRING
  "Frame loop owner: poll (engine runs main and sleeps) or fiber (SDL callbacks)")
set_property(CACHE URGE_RUNNER_MODEL PROPERTY STRINGS poll fiber)

if(NOT URGE_RUNNER_MODEL MATCHES "^(poll|fiber)$")
  message(FATAL_ERROR
    "URGE_RUNNER_MODEL is '${URGE_RUNNER_MODEL}'; it has to be poll or fiber.")
endif()

if(EMSCRIPTEN AND URGE_RUNNER_MODEL STREQUAL "poll")
  message(FATAL_ERROR
    "URGE_RUNNER_MODEL=poll cannot be used on the web: a page has to give the "
    "thread back to the browser at every frame, so the loop has to be SDL's "
    "callback loop.  Use fiber.")
endif()

message(STATUS "URGE: frame loop = ${URGE_RUNNER_MODEL}")
if(URGE_RUNNER_MODEL STREQUAL "poll")
  add_compile_definitions(URGE_RUNNER_POLL=1)
else()
  add_compile_definitions(URGE_RUNNER_FIBER=1)
endif()

add_subdirectory(third_party)

add_subdirectory(core)

# The binding glue is generated from core/**/*.h, so it has to be brought up to
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
# `(*^▽^*)` in core/system/config.h is written in UTF-8, and without this MSVC would
# read it through the system ANSI code page and store mojibake.  Second, and
# this is what actually bites: MSVC's <format> validates a *format string*
# against the execution character set, so on a non-UTF-8 system code page (936
# here) any UTF-8 text handed to std::format is rejected with
#   "Invalid encoded character in format string."
# See core/common/exception.h and core/common/logger.h, whose runtime messages hit exactly
# that.
#
# Applied to the three targets of this project only: third_party is upstream
# code whose own encoding is its own affair.
if(MSVC)
  target_compile_options(urge-core PRIVATE /utf-8)
  target_compile_options(urge-binding PRIVATE /utf-8)
  target_compile_options(Game PRIVATE /utf-8)
endif()

# The engine has no C++ exceptions.  A failure is an `urge::ExceptionState` the
# caller passes in and inspects afterwards (core/common/exception.h), so no
# declaration in core/, bind/ or app/ may throw or catch -- and the three
# targets are built so that it cannot happen by accident:
#
#   /EHs-c-             no unwind semantics at all
#   /we4530             a `try` block is an *error*.  Without this, MSVC only
#                       warns (C4530: "exception handler used, but unwind
#                       semantics are not enabled") and compiles the handler
#                       anyway, which is a catch that never catches.
#   _HAS_EXCEPTIONS=0   the STL takes its non-throwing paths instead of
#                       `throw`ing `bad_alloc` / `out_of_range` out of a build
#                       with no unwinder.  It aborts instead, which is the only
#                       thing left to do; the point is that the library stops
#                       compiling `throw` statements we cannot catch.
#
# Clang (the Emscripten build) spells the same thing `-fno-exceptions`; the
# `-fexceptions` this used to need existed for the try/catch bridge in the
# glue, which is gone.  libc++ reads the flag itself -- `__has_feature(
# cxx_exceptions)` is what it branches on -- so no define is needed there;
# `_HAS_EXCEPTIONS` is the MSVC STL's spelling of the same switch.
#
# third_party is upstream code: it keeps its own settings, and none of it is
# C++ that throws on the paths this engine takes.
if(MSVC)
  foreach(target urge-core urge-binding Game)
    target_compile_options(${target} PRIVATE /EHs-c- /we4530)
    target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
  endforeach()
else()
  foreach(target urge-core urge-binding Game)
    target_compile_options(${target} PRIVATE -fno-exceptions)
  endforeach()
endif()

# Profile-guided optimization is wired up at the end of app/CMakeLists.txt
# rather than here: it hangs a step off the Game target, and CMake only allows
# that in the directory that created it.  See cmake/pgo.cmake.

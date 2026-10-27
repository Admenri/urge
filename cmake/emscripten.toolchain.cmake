# ---------------------------------------------------------------------------
# Emscripten: find the SDK, then hand over to the toolchain file it ships.
#
# A preset cannot name the SDK itself.  `CMAKE_TOOLCHAIN_FILE` is read before
# any of this project's cache variables exist, and `$EMSDK` is an environment
# variable the user's shell either has or does not -- so the same preset has to
# work both for someone who ran emsdk_env and for someone who only installed
# the SDK somewhere.  This wrapper resolves it and says what it found, which is
# what a machine without the SDK gets instead of a missing-file error from deep
# inside CMake.
#
# Resolution order:
#   -DURGE_EMSDK=...   an explicit choice always wins
#   $EMSDK             what emsdk_env sets
#   $EMSCRIPTEN        also set by emsdk_env; the SDK root is two levels up
#
#   cmake --preset emscripten-wasm32
# ---------------------------------------------------------------------------

set(_urge_emsdk "")

if(URGE_EMSDK)
  set(_urge_emsdk "${URGE_EMSDK}")
elseif(DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "")
  set(_urge_emsdk "$ENV{EMSDK}")
elseif(DEFINED ENV{EMSCRIPTEN} AND NOT "$ENV{EMSCRIPTEN}" STREQUAL "")
  get_filename_component(_urge_emsdk "$ENV{EMSCRIPTEN}" DIRECTORY)
  get_filename_component(_urge_emsdk "${_urge_emsdk}" DIRECTORY)
endif()

set(_urge_emscripten_toolchain
  "${_urge_emsdk}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")

if(_urge_emsdk STREQUAL "" OR NOT EXISTS "${_urge_emscripten_toolchain}")
  message(FATAL_ERROR
    "URGE: no Emscripten SDK found (looked at $EMSDK and $EMSCRIPTEN).\n"
    "Install one with emsdk, run its emsdk_env script (or export EMSDK=<path>), "
    "or pass -DURGE_EMSDK=<path>.")
endif()

message(STATUS "URGE: Emscripten SDK = ${_urge_emsdk}")

include("${_urge_emscripten_toolchain}")

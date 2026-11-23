# ---------------------------------------------------------------------------
# emdawnwebgpu -- the WebGPU implementation the web build talks to.
#
# On Emscripten the engine does not go through wgpu-native.  Emscripten ships a
# port that implements the standard <webgpu/webgpu.h> on top of the browser's
# WebGPU, maintained in Dawn: same C API, no Rust in the middle, no wasm
# library to link.  `--use-port=emdawnwebgpu` (compile and link) is what pulls
# it in; this file's job is to make sure it is on disk *before* the build asks
# for its header.
#
# That timing matters.  third_party/webgpu-cpp generates the C++ wrapper the
# engine is written against, and it parses the C header at configure time -- so
# the port has to be installed at configure time too, not at first compile.
# `embuilder` is the documented way to do that ahead of the build, and it is
# sitting next to emcc.
#
# The download is a pinned Dawn release, cached by Emscripten and reused by
# every build from then on.
# ---------------------------------------------------------------------------

# Ask the selected compiler's configuration rather than assuming $EMSDK or
# the default cache location. EM_CACHE may move both the port and its headers.
get_filename_component(_urge_emscripten_dir "${CMAKE_C_COMPILER}" DIRECTORY)
execute_process(
  COMMAND "${_urge_emscripten_dir}/em-config" CACHE
  RESULT_VARIABLE _urge_cache_result
  OUTPUT_VARIABLE _urge_emscripten_cache
  ERROR_VARIABLE _urge_cache_error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _urge_cache_result EQUAL 0 OR _urge_emscripten_cache STREQUAL "")
  message(FATAL_ERROR
    "Could not locate the selected Emscripten cache:\n${_urge_cache_error}")
endif()

set(URGE_EMDAWNWEBGPU_ROOT
  "${_urge_emscripten_cache}/ports/emdawnwebgpu/emdawnwebgpu_pkg")
set(URGE_EMDAWNWEBGPU_INCLUDE "${URGE_EMDAWNWEBGPU_ROOT}/webgpu/include")

# Always let the active SDK validate its pinned port version. An existing
# header can belong to a different SDK restored from an Actions cache.
message(STATUS "URGE: preparing the selected SDK's emdawnwebgpu port")
execute_process(
  COMMAND "${_urge_emscripten_dir}/embuilder" build emdawnwebgpu
  RESULT_VARIABLE _urge_emdawn_result
  OUTPUT_VARIABLE _urge_emdawn_output
  ERROR_VARIABLE _urge_emdawn_error)
if(NOT _urge_emdawn_result EQUAL 0 OR
   NOT EXISTS "${URGE_EMDAWNWEBGPU_INCLUDE}/webgpu/webgpu.h")
  message(FATAL_ERROR
    "Could not install the emdawnwebgpu port (${_urge_emdawn_result}):\n"
    "${_urge_emdawn_output}\n${_urge_emdawn_error}")
endif()

file(SHA256 "${URGE_EMDAWNWEBGPU_INCLUDE}/webgpu/webgpu.h"
     _urge_emdawn_header_hash)
message(STATUS "URGE: emdawnwebgpu headers in '${URGE_EMDAWNWEBGPU_INCLUDE}'")
message(STATUS "URGE: emdawnwebgpu header SHA256=${_urge_emdawn_header_hash}")

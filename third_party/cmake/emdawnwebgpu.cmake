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

set(URGE_EMDAWNWEBGPU_ROOT
  "$ENV{EMSDK}/upstream/emscripten/cache/ports/emdawnwebgpu/emdawnwebgpu_pkg")
set(URGE_EMDAWNWEBGPU_INCLUDE "${URGE_EMDAWNWEBGPU_ROOT}/webgpu/include")

if(NOT EXISTS "${URGE_EMDAWNWEBGPU_INCLUDE}/webgpu/webgpu.h")
  if(NOT DEFINED ENV{EMSDK})
    message(FATAL_ERROR
      "EMSDK is not set, so the emdawnwebgpu port cannot be located.  Source "
      "emsdk_env (or export EMSDK) before configuring the web build.")
  endif()

  get_filename_component(_urge_emscripten_dir "${CMAKE_C_COMPILER}" DIRECTORY)
  message(STATUS "URGE: installing the emdawnwebgpu port (first use)")
  execute_process(
    COMMAND "${_urge_emscripten_dir}/embuilder" build emdawnwebgpu
    RESULT_VARIABLE _urge_emdawn_result
    OUTPUT_VARIABLE _urge_emdawn_output
    ERROR_VARIABLE _urge_emdawn_output)

  if(NOT _urge_emdawn_result EQUAL 0 OR
     NOT EXISTS "${URGE_EMDAWNWEBGPU_INCLUDE}/webgpu/webgpu.h")
    message(FATAL_ERROR
      "Could not install the emdawnwebgpu port (${_urge_emdawn_result}):\n"
      "${_urge_emdawn_output}")
  endif()
endif()

message(STATUS "URGE: emdawnwebgpu headers in '${URGE_EMDAWNWEBGPU_INCLUDE}'")

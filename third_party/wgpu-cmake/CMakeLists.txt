cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Emscripten: emdawnwebgpu, not wgpu-native.
#
# The web build used to cross-compile wgpu-native for wasm32 with cargo and link
# a Rust static library into the module.  It does not any more: Emscripten ships
# a port, maintained in Dawn, that implements the very same <webgpu/webgpu.h> on
# top of the browser's WebGPU.  The render layer's C API calls therefore go
# straight to the browser with nothing in between -- no cargo, no Rust, no
# second WebGPU implementation to keep in step.
#
# `--use-port` has to be passed to both the compile and the link, as the port
# requires.  `cpp_bindings=false` keeps Dawn's own C++ wrapper out of the
# include path: the engine is written against the wrapper
# third_party/webgpu-cpp generates from the same header, and two `namespace
# wgpu` definitions in one build is one too many.
#
# The port itself is installed at configure time by
# third_party/cmake/emdawnwebgpu.cmake, because the C++ wrapper is generated
# from its header before anything is compiled.
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  add_library(webgpu_wgpu INTERFACE)
  target_compile_options(webgpu_wgpu INTERFACE
    "--use-port=emdawnwebgpu:cpp_bindings=false")
  target_link_options(webgpu_wgpu INTERFACE
    "--use-port=emdawnwebgpu:cpp_bindings=false")
  return()
endif()

include(FetchContent)

FetchContent_Declare(
  Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG v0.6
)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(MANIFEST_PATH wgpu-native/Cargo.toml)

if(WIN32)
  set(OS_LIBRARIES d3dcompiler ws2_32 userenv bcrypt ntdll opengl32 Propsys RuntimeObject)
else()
  message(FATAL_ERROR unsupported platform)
endif()

add_library(webgpu_wgpu INTERFACE)
target_link_libraries(webgpu_wgpu INTERFACE wgpu_native ${OS_LIBRARIES})

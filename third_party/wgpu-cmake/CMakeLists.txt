cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Emscripten: emdawnwebgpu, not wgpu-native.
#
# Emscripten ships a port, maintained in Dawn, that implements <webgpu/webgpu.h>
# on top of the browser's WebGPU, so the render layer's C API calls go straight
# to the browser -- no cargo, no Rust, no second implementation to keep in step.
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

if(ANDROID)
  # Android renders through Vulkan (and can fall back to GLES); DX12, Metal and
  # the GLSL/SPIR-V front ends are dead weight in a cross build, and naga's
  # parsers are the slowest thing in the crate.
  corrosion_import_crate(MANIFEST_PATH wgpu-native/Cargo.toml
                         FEATURES wgsl vulkan)

  if(CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    set(_urge_android_triple "aarch64-linux-android")
  elseif(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
    set(_urge_android_triple "x86_64-linux-android")
  else()
    message(FATAL_ERROR
      "URGE: no Rust target for the Android ABI '${CMAKE_ANDROID_ARCH_ABI}'; "
      "only arm64-v8a and x86_64 are wired up.")
  endif()

  get_filename_component(_urge_ndk_bin "${CMAKE_C_COMPILER}" DIRECTORY)
  get_filename_component(_urge_ndk_sysroot
    "${_urge_ndk_bin}/../sysroot" ABSOLUTE)

  # The API level belongs to the triple: it is what selects the matching
  # `bits/` headers inside the sysroot.  `CMAKE_SYSTEM_VERSION` is not the API
  # level here, so it comes out of ANDROID_PLATFORM ("android-29") instead.
  string(REPLACE "android-" "" _urge_android_api "${ANDROID_PLATFORM}")
  set(_urge_android_target "${_urge_android_triple}${_urge_android_api}")

  # The NDK's clang is the linker cargo has to use -- but only the *bare*
  # `clang.exe`, because the per-target wrappers (`aarch64-linux-android29-clang`)
  # ship as .cmd/.sh scripts on Windows and cargo cannot spawn those.  The bare
  # one defaults to the host target, which is why the triple has to be handed to
  # the link step too: without it clang picks the COFF linker and rejects every
  # ELF flag rustc passes.
  corrosion_set_linker(wgpu_native "${CMAKE_C_COMPILER}")
  corrosion_set_env_vars(wgpu_native
    "RUSTFLAGS=-Clink-arg=--target=${_urge_android_target}")

  # wgpu-native generates its C API bindings with bindgen at build time, and
  # bindgen's libclang knows nothing about the NDK: without a target and a
  # sysroot it fails on the first `<stdint.h>` in webgpu.h.  The compiler
  # builtins (`stddef.h`, `stdarg.h`, ...) live next to the clang that owns
  # them rather than in the sysroot, and libclang does not always find that
  # directory when it was loaded from a DLL path of our choosing, so it is
  # named outright.
  file(GLOB _urge_clang_resource_dirs "${_urge_ndk_bin}/../lib/clang/*")
  set(_urge_clang_resource "")
  if(_urge_clang_resource_dirs)
    list(SORT _urge_clang_resource_dirs)
    list(GET _urge_clang_resource_dirs -1 _urge_clang_resource_dir)
    set(_urge_clang_resource " -resource-dir=${_urge_clang_resource_dir}")
  endif()

  # bindgen opens libclang with a bare `LoadLibraryExW(..., 0)`, which resolves
  # the DLL's own imports through the process search path only -- the directory
  # the DLL sits in is never consulted.  The NDK's libclang imports
  # libwinpthread-1.dll from that very directory, so without the NDK bin on
  # PATH the load fails with ERROR_MOD_NOT_FOUND and bindgen reports
  # "Unable to find libclang".
  #
  # `cmake -E env` takes one argument per assignment, and corrosion expands the
  # assignments as a CMake list, so plain semicolons inside PATH would split it
  # into many arguments -- the loader would then only see the first directory.
  # Escaped separators survive the list.  The property is written directly
  # because `corrosion_set_env_vars` expands the value again on its way into
  # `set_property`, which would unescape them.
  set(_urge_cargo_path "${_urge_ndk_bin};$ENV{PATH}")
  string(REPLACE ";" "\\;" _urge_cargo_path "${_urge_cargo_path}")
  set_property(TARGET wgpu_native APPEND PROPERTY CORROSION_ENVIRONMENT_VARIABLES
    "PATH=${_urge_cargo_path}")

  corrosion_set_env_vars(wgpu_native
    "LIBCLANG_PATH=${_urge_ndk_bin}"
    "BINDGEN_EXTRA_CLANG_ARGS=--target=${_urge_android_target} --sysroot=${_urge_ndk_sysroot}${_urge_clang_resource}")

  set(OS_LIBRARIES android log)
elseif(WIN32)
  corrosion_import_crate(MANIFEST_PATH wgpu-native/Cargo.toml)
  set(OS_LIBRARIES d3dcompiler ws2_32 userenv bcrypt ntdll opengl32 Propsys RuntimeObject)
else()
  message(FATAL_ERROR unsupported platform)
endif()

add_library(webgpu_wgpu INTERFACE)
target_link_libraries(webgpu_wgpu INTERFACE wgpu_native ${OS_LIBRARIES})

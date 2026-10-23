cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# Emscripten.
#
# wgpu-native builds for `wasm32-unknown-emscripten`, where the only backend is
# wgpu's GLES one on top of WebGL 2 -- so the desktop backends are compiled out
# and only `wgsl` (the shading language the engine writes) and `gles` are on.
#
# Corrosion is not used here: it drives cargo for the host target, while this is
# a cross build that produces a static library for em++ to link into the module
# (an Emscripten module needs an entry point, so the `cdylib` half of the crate
# cannot be linked on its own).  The archive is imported as a plain target and
# the flags the backend needs travel with `webgpu_wgpu`.
#
# `EMSDK` has to be in the environment: bindgen lays the bindings out for wasm32
# with the Emscripten sysroot, and without it every structure holding a pointer
# would be generated at host width.
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  find_program(URGE_CARGO NAMES cargo)
  if(NOT URGE_CARGO)
    message(FATAL_ERROR
      "cargo was not found on PATH.  wgpu-native is built from source, and the "
      "Emscripten build compiles it for wasm32.")
  endif()

  set(URGE_WGPU_TARGET wasm32-unknown-emscripten)
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(URGE_WGPU_PROFILE_DIR debug)
    set(URGE_WGPU_PROFILE_FLAG)
  else()
    set(URGE_WGPU_PROFILE_DIR release)
    set(URGE_WGPU_PROFILE_FLAG --release)
  endif()

  set(URGE_WGPU_LIBRARY
    "${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native/target/${URGE_WGPU_TARGET}/${URGE_WGPU_PROFILE_DIR}/libwgpu_native.a")

  add_custom_command(
    OUTPUT "${URGE_WGPU_LIBRARY}"
    COMMAND "${URGE_CARGO}" rustc --lib --crate-type staticlib
            --target "${URGE_WGPU_TARGET}" ${URGE_WGPU_PROFILE_FLAG}
            --no-default-features --features wgsl,gles
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/wgpu-native"
    COMMENT "Building wgpu-native for ${URGE_WGPU_TARGET} (GLES / WebGL 2)"
    VERBATIM)

  # `ALL` so the archive exists before anything links against it: an imported
  # target is not part of the build graph, and the custom command's OUTPUT is
  # only reached through this target.
  add_custom_target(wgpu-native-emscripten ALL DEPENDS "${URGE_WGPU_LIBRARY}")

  add_library(wgpu_native STATIC IMPORTED GLOBAL)
  set_target_properties(wgpu_native PROPERTIES
    IMPORTED_LOCATION "${URGE_WGPU_LIBRARY}")

  add_library(webgpu_wgpu INTERFACE)
  target_link_libraries(webgpu_wgpu INTERFACE wgpu_native)
  add_dependencies(webgpu_wgpu wgpu-native-emscripten)

  # Emscripten exposes WebGL 2 and nothing older or newer, and wgpu-hal reaches
  # every GL entry point through `eglGetProcAddress`, which the runtime only
  # links in when this is set.
  target_link_options(webgpu_wgpu INTERFACE
    "-sMIN_WEBGL_VERSION=2"
    "-sMAX_WEBGL_VERSION=2"
    "-sGL_ENABLE_GET_PROC_ADDRESS")

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

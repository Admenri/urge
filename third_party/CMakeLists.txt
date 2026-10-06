cmake_minimum_required(VERSION 3.30)

set(BUILD_SHARED_LIBS OFF)
set(SDL_STATIC ON)
set(SDLIMAGE_VENDORED OFF)
set(YAML_CPP_BUILD_TOOLS OFF)

# SDL_ttf is built into the engine as a static library that carries its own
# FreeType, HarfBuzz and PlutoSVG copies (the "vendored" layout, which is also
# what the upstream project defaults to on MSVC). Samples and install rules are
# not wanted, and a warning is not an error in a bundled copy.
set(SDLTTF_VENDORED ON)
set(SDLTTF_SAMPLES OFF)
set(SDLTTF_INSTALL OFF)
set(SDLTTF_WERROR OFF)

set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_GLSLANG_JS OFF)
set(GLSLANG_TESTS OFF)
set(GLSLANG_ENABLE_INSTALL OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
set(ENABLE_SPIRV ON)

# miniaudio renders into a buffer the engine hands to SDL, so it must not open
# a sound card of its own: SDL owns the device and the format negotiation.
set(MINIAUDIO_NO_DEVICEIO ON)
set(MINIAUDIO_DEBUG_OUTPUT OFF)

# The Tint SPIR-V reader builds its IR with SPIRV-Tools, which glslang carries
# in its External directory. Only the static library is wanted here.
set(SPIRV_SKIP_TESTS ON)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV_WERROR OFF)
set(SPIRV_TOOLS_BUILD_STATIC ON)
set(SKIP_SPIRV_TOOLS_INSTALL ON)

add_subdirectory(glm)
add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(physfs)
add_subdirectory(miniaudio)
add_subdirectory(webgpu-cpp)
add_subdirectory(imgui-docking)
add_subdirectory(wgpu-cmake)
add_subdirectory(glslang)
add_subdirectory(spirv-reflect)

add_subdirectory(cruby)
add_subdirectory(zlib)

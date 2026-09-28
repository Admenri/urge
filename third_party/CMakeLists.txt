cmake_minimum_required(VERSION 3.30)

set(BUILD_SHARED_LIBS OFF)
set(SDL_STATIC ON)
set(SDLIMAGE_VENDORED OFF)
set(YAML_CPP_BUILD_TOOLS OFF)

set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_GLSLANG_JS OFF)
set(GLSLANG_TESTS OFF)
set(GLSLANG_ENABLE_INSTALL OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
set(ENABLE_SPIRV ON)

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
add_subdirectory(physfs)
add_subdirectory(webgpu-cpp)
add_subdirectory(wgpu-cmake)
add_subdirectory(glslang)
add_subdirectory(spirv-reflect)

add_subdirectory(cruby)
add_subdirectory(zlib)

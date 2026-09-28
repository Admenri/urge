cmake_minimum_required(VERSION 3.30)

add_library(urge-core STATIC)
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
target_sources(urge-core PRIVATE ${CORE_SOURCES})

target_link_libraries(urge-core PUBLIC glm SDL3::SDL3-static SDL3_image::SDL3_image-static physfs-static)
target_link_libraries(urge-core PUBLIC webgpu-cpp-header webgpu_wgpu)

# The engine shaders are GLSL: glslang compiles them, spirv-reflect reads the
# layout of them and the Tint subset writes the WGSL the device is asked for
target_link_libraries(urge-core PUBLIC glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits spirv-reflect)

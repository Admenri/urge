cmake_minimum_required(VERSION 3.30)

add_library(urge-core STATIC)
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
target_sources(urge-core PRIVATE ${CORE_SOURCES})

target_link_libraries(urge-core PUBLIC glm SDL3::SDL3-static SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image-static physfs-static)
target_link_libraries(urge-core PUBLIC webgpu-cpp-header glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits spirv-reflect)

target_link_libraries(urge-core PUBLIC webgpu_wgpu)
target_compile_definitions(urge-core PUBLIC WEBGPU_BACKEND_WGPU=1)

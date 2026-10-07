cmake_minimum_required(VERSION 3.30)

add_library(urge-core STATIC)
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# `video.cc` always compiles: it is the `Video` class itself, which stays
# exported to Ruby and turns into the stub that raises while the subsystem is
# off.  What the switch removes is the engine behind it -- the demuxer, the
# decoder and the player -- and with them the only path to a dav1d header.
if(NOT URGE_ENABLE_VIDEO)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/video_[a-z_]+\\.cc$")
endif()

target_sources(urge-core PRIVATE ${CORE_SOURCES})

if(URGE_ENABLE_VIDEO)
  target_compile_definitions(urge-core PUBLIC URGE_ENABLE_VIDEO=1)
else()
  target_compile_definitions(urge-core PUBLIC URGE_ENABLE_VIDEO=0)
endif()

target_link_libraries(urge-core PUBLIC glm SDL3::SDL3-static SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image-static physfs-static)
target_link_libraries(urge-core PUBLIC webgpu-cpp-header glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits spirv-reflect)

target_link_libraries(urge-core PUBLIC webgpu_wgpu)
target_compile_definitions(urge-core PUBLIC WEBGPU_BACKEND_WGPU=1)

# The in-engine debug interface (core/imgui_manager.cc) lives in this library,
# so both the headers and the backend selector come from the ImGui target.
target_link_libraries(urge-core PUBLIC imgui-docking)

# The audio subsystem decodes with miniaudio and plays through SDL.
target_link_libraries(urge-core PUBLIC miniaudio)

# The video subsystem demuxes with libwebm and decodes AV1 with dav1d.  Neither
# target exists when the subsystem is off.
if(URGE_ENABLE_VIDEO)
  target_link_libraries(urge-core PUBLIC dav1d libwebm)
endif()

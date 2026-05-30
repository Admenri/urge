cmake_minimum_required(VERSION 3.25)

add_library(engine_renderer STATIC)

target_sources(engine_renderer
 PUBLIC
  device/render_device.cc
  device/render_device.h
  renderer_config.h
)

target_link_libraries(engine_renderer
 PUBLIC
  engine_base
)

target_link_libraries(engine_renderer
 PUBLIC
  webgpu-cpp-header
  webgpu_wgpu
)

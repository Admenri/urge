# renderer components
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# components source
#--------------------------------------------------------------------------------

set(ENGINE_RENDERER_SRC
  context/device_context.cc
  context/device_context.h
  device/render_device.cc
  device/render_device.h
  pipeline/builtin_wgsl.cc
  pipeline/builtin_wgsl.h
  pipeline/render_pipeline.cc
  pipeline/render_pipeline.h
  resource/render_buffer.cc
  resource/render_buffer.h
  vertex/vertex_layout.cc
  vertex/vertex_layout.h
)

add_library(engine_renderer STATIC ${ENGINE_RENDERER_SRC})
urge_setup_target_compile_feature(engine_renderer)
target_link_libraries(engine_renderer PUBLIC engine_base engine_ui dawn::webgpu_dawn)

# renderer components
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# components source
#--------------------------------------------------------------------------------

set(ENGINE_RENDERER_SRC
  device/render_device.cc
  device/render_device.h
  layout/vertex_layout.cc
  layout/vertex_layout.h
  layout/uniform_layout.cc
  layout/uniform_layout.h
  pipeline/builtin_hlsl.cc
  pipeline/builtin_hlsl.h
  pipeline/render_binding.cc
  pipeline/render_binding.h
  pipeline/render_pipeline.cc
  pipeline/render_pipeline.h
  resource/render_buffer.cc
  resource/render_buffer.h
  utils/texture_utils.cc
  utils/texture_utils.h
)

add_library(engine_renderer STATIC ${ENGINE_RENDERER_SRC})
target_link_libraries(engine_renderer
 PUBLIC
  engine_base
  engine_ui
  Diligent-BuildSettings
  Diligent-GraphicsTools
)

set(LIBRARY_TYPE static)
if (URGE_SHARED_RENDERER_LIBRARY)
  set(LIBRARY_TYPE shared)
endif ()

if (D3D11_SUPPORTED)
  target_link_libraries(engine_renderer PUBLIC Diligent-GraphicsEngineD3D11-${LIBRARY_TYPE})
endif ()
if (D3D12_SUPPORTED)
  target_link_libraries(engine_renderer PUBLIC Diligent-GraphicsEngineD3D12-${LIBRARY_TYPE})
endif ()
if (GL_SUPPORTED OR GLES_SUPPORTED)
  target_link_libraries(engine_renderer PUBLIC Diligent-GraphicsEngineOpenGL-${LIBRARY_TYPE})
endif ()
if (VULKAN_SUPPORTED)
  target_link_libraries(engine_renderer PUBLIC Diligent-GraphicsEngineVk-${LIBRARY_TYPE})
endif ()

if (MSVC)
  target_compile_options(engine_renderer PRIVATE /WX /wd4267)
else ()
  target_compile_options(engine_renderer PRIVATE -Werror)
endif ()

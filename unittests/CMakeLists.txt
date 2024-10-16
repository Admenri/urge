# Unittests
cmake_minimum_required(VERSION 3.20.1)

add_executable(renderer_unittests renderer_unittests.cc)
target_link_libraries(renderer_unittests PUBLIC
  engine_base
  engine_renderer
  engine_ui
  Diligent-GraphicsEngine
  Diligent-GraphicsEngineOpenGL-static
  Diligent-GraphicsEngineD3D12-static
  Diligent-TextureLoader
)

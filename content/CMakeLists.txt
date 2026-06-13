cmake_minimum_required(VERSION 3.25)

add_library(engine_content STATIC)

target_sources(engine_content
 PUBLIC
  app/runner.cc
  app/runner.h
  binding/external_binding.h
  common/disposable.h
  common/exception.h
  common/object.h
  common/utility.h
  common/vector.h
  gpu/gpu_command.cc
  gpu/gpu_command.h
  gpu/gpu_device.cc
  gpu/gpu_device.h
  gpu/gpu_resource.cc
  gpu/gpu_resource.h
  gpu/gpu.cc
  gpu/gpu.h
  profile/core_profile.cc
  profile/core_profile.h
  render/camera.cc
  render/camera.h
  render/graphics.cc
  render/graphics.h
  render/light.cc
  render/light.h
  render/renderer.cc
  render/renderer.h
  render/viewport.cc
  render/viewport.h
  resource/material.cc
  resource/material.h
  resource/mesh.cc
  resource/mesh.h
  scene/node_link.cc
  scene/node_link.h
  scene/node.cc
  scene/node.h
  scene/transform.cc
  scene/transform.h
  scene/world.cc
  scene/world.h
  content_config.h
)

target_link_libraries(engine_content
 PUBLIC
  engine_base
  engine_renderer
  engine_ui
  components_filesystem
  yaml-cpp::yaml-cpp
)

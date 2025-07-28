# content components
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# components source
#--------------------------------------------------------------------------------

file(GLOB RUNTIMEAPI_HEADERS "${PROJECT_SOURCE_DIR}/content/public/*.h")

set(ENGINE_CONTENT_SRC
  canvas/animation_impl.cc
  canvas/animation_impl.h  
  canvas/canvas_impl.cc
  canvas/canvas_impl.h
  canvas/canvas_scheduler.cc
  canvas/canvas_scheduler.h
  canvas/font_context.cc
  canvas/font_context.h
  canvas/font_impl.cc
  canvas/font_impl.h
  canvas/surface_impl.cc
  canvas/surface_impl.h
  common/color_impl.cc
  common/color_impl.h
  common/rect_impl.cc
  common/rect_impl.h
  common/table_impl.cc
  common/table_impl.h
  common/tone_impl.cc
  common/tone_impl.h
  common/value_observer.h
  context/disposable.cc
  context/disposable.h
  context/engine_object.h
  context/exception_state.cc
  context/exception_state.h
  context/execution_context.cc
  context/execution_context.h
  gpu/buffer_impl.cc
  gpu/buffer_impl.h
  gpu/command_list_impl.cc
  gpu/command_list_impl.h
  gpu/command_queue_impl.cc
  gpu/command_queue_impl.h
  gpu/device_context_impl.cc
  gpu/device_context_impl.h
  gpu/fence_impl.cc
  gpu/fence_impl.h
  gpu/pipeline_signature_impl.cc
  gpu/pipeline_signature_impl.h
  gpu/pipeline_state_impl.cc
  gpu/pipeline_state_impl.h
  gpu/query_impl.cc
  gpu/query_impl.h
  gpu/render_device_impl.cc
  gpu/render_device_impl.h
  gpu/resource_binding_impl.cc
  gpu/resource_binding_impl.h
  gpu/resource_mapping_impl.cc
  gpu/resource_mapping_impl.h
  gpu/resource_variable_impl.cc
  gpu/resource_variable_impl.h
  gpu/sampler_impl.cc
  gpu/sampler_impl.h
  gpu/shader_impl.cc
  gpu/shader_impl.h
  gpu/texture_impl.cc
  gpu/texture_impl.h
  input/keyboard_controller.cc
  input/keyboard_controller.h
  input/key_event.cc
  input/key_event.h
  input/mouse_controller.cc
  input/mouse_controller.h
  input/mouse_event.cc
  input/mouse_event.h
  input/text_input.cc
  input/text_input.h
  input/touch_event.cc
  input/touch_event.h
  io/iostream_impl.cc
  io/iostream_impl.h
  media/audio_impl.cc
  media/audio_impl.h
  misc/misc_system.cc
  misc/misc_system.h
  profile/command_ids.h
  profile/content_profile.cc
  profile/content_profile.h
  profile/i18n_profile.cc
  profile/i18n_profile.h
  render/drawable_controller.cc
  render/drawable_controller.h
  render/mesh_impl.cc
  render/mesh_impl.h
  render/plane_impl.cc
  render/plane_impl.h
  render/spinesprite_impl.cc
  render/spinesprite_impl.h
  render/sprite_batch.cc
  render/sprite_batch.h
  render/sprite_impl.cc
  render/sprite_impl.h
  render/tilemap_impl.cc
  render/tilemap_impl.h
  render/tilemap2_impl.cc
  render/tilemap2_impl.h
  render/tilequad.cc
  render/tilequad.h
  render/videodecoder_impl.cc
  render/videodecoder_impl.h
  render/window_impl.cc
  render/window_impl.h
  render/window2_impl.cc
  render/window2_impl.h
  screen/display_impl.cc
  screen/display_impl.h
  screen/renderscreen_impl.cc
  screen/renderscreen_impl.h
  screen/viewport_impl.cc
  screen/viewport_impl.h
  worker/content_runner.cc
  worker/content_runner.h
  worker/engine_binding.cc
  worker/engine_binding.h
  worker/event_controller.cc
  worker/event_controller.h
  content_config.h

  ${RUNTIMEAPI_HEADERS}
)

#--------------------------------------------------------------------------------
# Embeded program resource
#--------------------------------------------------------------------------------

execute_process(
  COMMAND "${PYTHON_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/buildtools/build_file_binary.py" "${PROJECT_SOURCE_DIR}/content/resource/embed.ttf" -o "${RGU_AUTOGEN_DIR}/content/resource/embed.ttf.bin"
  OUTPUT_QUIET
)

add_library(engine_content STATIC ${ENGINE_CONTENT_SRC})
target_link_libraries(engine_content
 PUBLIC
  engine_base
  engine_ui
  engine_renderer
  components_network
  components_filesystem
  components_fpslimiter
  components_audioservice
  components_spine2drenderer
  sdl_image_loader
  sdl_ttf_render
  ocornut_imgui
  av1player
)

if (MSVC)
  target_compile_options(engine_content PRIVATE /WX /wd4267 /wd4244)
else ()
  target_compile_options(engine_content PRIVATE -Werror)
endif ()

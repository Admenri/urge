# Content compositor components of RGU Kernel
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Content source of core
#--------------------------------------------------------------------------------

set(CORE_CONTENT_SRC
  binding/binding_engine.cc
  binding/binding_engine.h
  common/content_utils.h  
  common/tilequad.h
  common/tileutils.cc
  common/tileutils.h
  config/engine_config.cc
  config/engine_config.h
  public/bitmap.cc
  public/bitmap.h
  public/disposable.cc
  public/disposable.h
  public/drawable.cc
  public/drawable.h
  public/flashable.h
  public/font.cc
  public/font.h
  public/graphics.cc
  public/graphics.h
  public/input.cc
  public/input.h
  public/mouse.cc
  public/mouse.h
  public/serializable.cc
  public/serializable.h
  public/table.cc
  public/table.h
  public/touch.cc
  public/touch.h
  public/utility.cc
  public/utility.h
  worker/engine_worker.cc
  worker/engine_worker.h
  worker/worker_scheduler.cc
  worker/worker_scheduler.h
)

add_library(engine_content PUBLIC ${CORE_CONTENT_SRC})
target_link_libraries(engine_content PUBLIC
                      engine_base
                      engine_renderer
                      engine_ui
                      components_filesystem
                      components_fpslimiter
                      sdl_image_loader
                      sdl_ttf_render
                      )

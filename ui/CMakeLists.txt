# UI Library
cmake_minimum_required(VERSION 3.25)

#--------------------------------------------------------------------------------
# ui widget
#--------------------------------------------------------------------------------

add_library(engine_ui STATIC)

target_sources(engine_ui
 PUBLIC
  context/imgui_context.cc
  context/imgui_context.h
  widget/widget.cc
  widget/widget.h
)

target_link_libraries(engine_ui
 PUBLIC
  engine_base
  engine_renderer
  dear_imgui
)

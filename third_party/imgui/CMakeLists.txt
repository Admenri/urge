cmake_minimum_required(VERSION 3.25)

add_library(dear_imgui STATIC)

target_include_directories(dear_imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(dear_imgui
 PUBLIC
  backends/imgui_impl_sdl3.cpp
  backends/imgui_impl_wgpu.cpp
  misc/cpp/imgui_stdlib.cpp
  misc/freetype/imgui_freetype.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
  imgui.cpp
)

target_link_libraries(dear_imgui
 PUBLIC
  engine_base
  engine_renderer
  freetype
  plutosvg
)

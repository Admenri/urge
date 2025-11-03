# Main imgui
cmake_minimum_required(VERSION 3.20.0)

#--------------------------------------------------------------------------------
# Source of IMGUI
#--------------------------------------------------------------------------------

set(IMGUI_MAIN_SOURCE
  imgui.cpp
  imgui_demo.cpp
  imgui_draw.cpp
  imgui_tables.cpp
  imgui_widgets.cpp
)

set(IMGUI_BACKEND_SOURCE
  backends/imgui_impl_diligent.cc
  backends/imgui_impl_sdl3.cpp
)

set(IMGUI_MISC_SOURCE
  misc/cpp/imgui_stdlib.cpp
  misc/freetype/imgui_freetype.cpp
)

add_library(ocornut_imgui
  ${IMGUI_MAIN_SOURCE}
  ${IMGUI_BACKEND_SOURCE}
  ${IMGUI_MISC_SOURCE}
)

target_link_libraries(ocornut_imgui PUBLIC engine_renderer freetype)
target_include_directories(ocornut_imgui PRIVATE .)

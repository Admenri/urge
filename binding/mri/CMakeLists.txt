# Mri binding components
cmake_minimum_required(VERSION 3.18.0)

#--------------------------------------------------------------------------------
# Mri - Matz's Ruby Interpreter components
#--------------------------------------------------------------------------------

set(BINDING_MRI_SRC
  init_audio.cc
  init_audio.h  
  init_bitmap.cc
  init_bitmap.h
  init_corefile.cc
  init_corefile.h
  init_font.cc
  init_font.h
  init_graphics.cc
  init_graphics.h
  init_input.cc
  init_input.h
  init_plane.cc
  init_plane.h
  init_sprite.cc
  init_sprite.h
  init_table.cc
  init_table.h
  init_tilemap.cc
  init_tilemap.h
  init_tilemap2.cc
  init_tilemap2.h
  init_touch.cc
  init_touch.h
  init_utility.cc
  init_utility.h
  init_viewport.cc
  init_viewport.h
  init_window.cc
  init_window.h
  init_window2.cc
  init_window2.h

  init_rgu.cc
  init_rgu.h
  init_mouse.cc
  init_mouse.h

  mri_template.h
  mri_main.cc
  mri_main.h
  mri_util.cc
  mri_util.h

  zlib.c
)

add_library(binding_mri ${BINDING_MRI_SRC})
target_link_libraries(binding_mri PUBLIC engine_base engine_renderer engine_content engine_ui mri_ruby zlibstatic)

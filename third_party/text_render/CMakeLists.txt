# SDL_ttf vendor
cmake_minimum_required(VERSION 3.18.0)

add_library(sdl_ttf_render STATIC
  src/SDL_ttf.c
)

target_include_directories(sdl_ttf_render PUBLIC include)
target_link_libraries(sdl_ttf_render PRIVATE freetype)

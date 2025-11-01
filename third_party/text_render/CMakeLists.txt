# SDL_ttf vendor
cmake_minimum_required(VERSION 3.18.0)

add_library(sdl_ttf_render STATIC
  src/SDL_hashtable.c
  src/SDL_hashtable_ttf.c
  src/SDL_gpu_textengine.c
  src/SDL_renderer_textengine.c
  src/SDL_surface_textengine.c
  src/SDL_ttf.c
)

target_include_directories(sdl_ttf_render PUBLIC include)
target_link_libraries(sdl_ttf_render PRIVATE freetype harfbuzz)
target_compile_definitions(sdl_ttf_render PUBLIC TTF_USE_HARFBUZZ=1)

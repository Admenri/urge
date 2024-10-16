# SDL_image vendor
cmake_minimum_required(VERSION 3.18.0)

add_library(sdl_image_loader STATIC
  src/IMG.c
  src/IMG_WIC.c
  src/IMG_avif.c
  src/IMG_bmp.c
  src/IMG_gif.c
  src/IMG_jpg.c
  src/IMG_jxl.c
  src/IMG_lbm.c
  src/IMG_pcx.c
  src/IMG_png.c
  src/IMG_pnm.c
  src/IMG_qoi.c
  src/IMG_stb.c
  src/IMG_svg.c
  src/IMG_tga.c
  src/IMG_tif.c
  src/IMG_webp.c
  src/IMG_xcf.c
  src/IMG_xpm.c
  src/IMG_xv.c
)

target_include_directories(sdl_image_loader PUBLIC include)
target_compile_definitions(sdl_image_loader PRIVATE
  USE_STBIMAGE
  LOAD_PNG
  LOAD_JPG
)

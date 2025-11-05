cmake_minimum_required(VERSION 3.20.1)

add_library(rb_zlib STATIC zlib.c)
target_link_libraries(rb_zlib PUBLIC mri_ruby zlibstatic)

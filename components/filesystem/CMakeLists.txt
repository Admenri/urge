# Components - filesystem
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of filesystem
#--------------------------------------------------------------------------------

add_library(components_filesystem STATIC)

target_sources(components_filesystem
 PUBLIC
  io_service.cc
  io_service.h
)

target_link_libraries(components_filesystem
 PUBLIC
  engine_base
  physfs-static
)

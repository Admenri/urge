# Components - net stack for URGE
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of net stack
#--------------------------------------------------------------------------------

set(COMPONENTS_NETWORK_SRC
  network_service.cc
  network_service.h
)

add_library(components_network STATIC ${COMPONENTS_NETWORK_SRC})
target_link_libraries(components_network PUBLIC engine_base uv_a)

# Components - network
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of net stack
#--------------------------------------------------------------------------------

set(COMPONENTS_NETWORK_SRC
  public/network_service.h
  network_service_impl.cc
  network_service_impl.h
)

add_library(components_network STATIC ${COMPONENTS_NETWORK_SRC})
target_compile_definitions(components_network
 PUBLIC
  ASIO_STANDALONE=1
  WIN32_LEAN_AND_MEAN=1
  _WIN32_WINNT=0x601
)
target_link_libraries(components_network
 PUBLIC
  engine_base
  ssl
)

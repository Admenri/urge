# Components - Network Service
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of net stack
#--------------------------------------------------------------------------------

set(COMPONENTS_NETWORK_SRC
  public/network_service.h
  public/tcp_socket.h
  public/udp_socket.h
  network_service_impl.cc
  network_service_impl.h
  tcp_socket_impl.cc
  tcp_socket_impl.h
  udp_socket_impl.cc
  udp_socket_impl.h
)

add_library(components_network STATIC ${COMPONENTS_NETWORK_SRC})
target_link_libraries(components_network PUBLIC engine_base)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(components_network PRIVATE _WIN32_WINNT=0x0601)
endif ()

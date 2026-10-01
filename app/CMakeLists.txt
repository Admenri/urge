cmake_minimum_required(VERSION 3.30)

add_executable(Game app.cc)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(Game PUBLIC resources/app.rc)
  target_sources(Game PUBLIC platform/win32.cc)
endif()

target_link_libraries(Game PUBLIC urge-core urge-binding)

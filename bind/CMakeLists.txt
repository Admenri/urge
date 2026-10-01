cmake_minimum_required(VERSION 3.30)

add_library(urge-binding STATIC)

file(GLOB BINDING_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")
target_sources(urge-binding PRIVATE ${BINDING_SOURCES})

target_include_directories(urge-binding PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(urge-binding PUBLIC ruby-static urge-core zlibstatic)

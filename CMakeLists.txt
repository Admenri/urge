cmake_minimum_required(VERSION 3.30)

project(URGE LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
include_directories(${PROJECT_SOURCE_DIR})

if(MSVC)
  add_compile_options(/utf-8 /wd5033)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(third_party)

add_subdirectory(core)

# The binding glue is generated from core/*.h, so it has to be brought up to
# date *before* bind/CMakeLists.txt globs it.  `include` runs here, in the
# configure step, and re-runs the two generators only when the headers (or a
# generator) actually changed -- see bind/gen_binding.cmake.  It has to be
# `include`, not `add_subdirectory`: a subdirectory gets its own variable and
# policy scope, and the `file(GLOB)` below has to see the sources this writes.
include(bind/gen_binding.cmake)

add_subdirectory(bind)
add_subdirectory(app)

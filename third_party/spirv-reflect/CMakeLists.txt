cmake_minimum_required(VERSION 3.30)

# spirv-reflect is one C file which reflects a SPIR-V module.
add_library(spirv-reflect STATIC spirv_reflect.c)
target_include_directories(spirv-reflect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

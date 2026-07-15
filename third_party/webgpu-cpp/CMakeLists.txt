cmake_minimum_required(VERSION 3.21)

find_package(Python3 COMPONENTS Interpreter)

get_filename_component(WEBGPU_NATIVE_HEADER ${CMAKE_CURRENT_SOURCE_DIR} DIRECTORY)
set(WEBGPU_NATIVE_HEADER "${WEBGPU_NATIVE_HEADER}/webgpu-headers/webgpu.h")

set(WEBGPUCPP_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(WEBGPUCPP_AUTOGEN_DIR "${WEBGPUCPP_INCLUDE_DIR}/webgpu")
file(MAKE_DIRECTORY ${WEBGPUCPP_AUTOGEN_DIR})
file(COPY ${WEBGPU_NATIVE_HEADER} DESTINATION ${WEBGPUCPP_AUTOGEN_DIR})

message(STATUS "Generating webgpu cpp header from ${WEBGPU_NATIVE_HEADER}")
message(STATUS "Output directory: ${WEBGPUCPP_AUTOGEN_DIR}")

execute_process(
  COMMAND ${Python3_EXECUTABLE} -B 
  "${CMAKE_CURRENT_SOURCE_DIR}/autogen.py" 
  --header "${WEBGPU_NATIVE_HEADER}" 
  --output "webgpu_cpp.hpp"
  WORKING_DIRECTORY ${WEBGPUCPP_AUTOGEN_DIR}
  RESULT_VARIABLE SCRIPT_RESULT
)

if (NOT SCRIPT_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to generate webgpu cpp header: ${SCRIPT_RESULT}")
endif ()

add_library(webgpu-cpp-header INTERFACE)
target_include_directories(webgpu-cpp-header INTERFACE
  $<BUILD_INTERFACE:${WEBGPUCPP_INCLUDE_DIR}>
  $<BUILD_INTERFACE:${WEBGPUCPP_AUTOGEN_DIR}>
)

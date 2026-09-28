cmake_minimum_required(VERSION 3.21)

find_package(Python3 COMPONENTS Interpreter)

# Locate the C header next to this directory (third_party/webgpu-headers).
get_filename_component(_WEBGPU_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
set(WEBGPU_NATIVE_HEADER "${_WEBGPU_HEADER_DIR}/wgpu-cmake/wgpu-native/ffi/webgpu-headers/webgpu.h")

# Generated output lives in the build tree.
set(WEBGPUCPP_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(WEBGPUCPP_AUTOGEN_DIR "${WEBGPUCPP_INCLUDE_DIR}/webgpu")
file(MAKE_DIRECTORY "${WEBGPUCPP_AUTOGEN_DIR}")
file(COPY "${WEBGPU_NATIVE_HEADER}" DESTINATION "${WEBGPUCPP_AUTOGEN_DIR}")

message(STATUS "Generating webgpu cpp header from ${WEBGPU_NATIVE_HEADER}")
message(STATUS "Output directory: ${WEBGPUCPP_AUTOGEN_DIR}")

# Run the parser/renderer to produce webgpu_cpp.hpp (plus webgpu_cpp.hpp.json).
execute_process(
  COMMAND "${Python3_EXECUTABLE}" -B
          "${CMAKE_CURRENT_SOURCE_DIR}/autogen.py"
          --header "${WEBGPU_NATIVE_HEADER}"
          --output "webgpu_cpp.hpp"
  WORKING_DIRECTORY "${WEBGPUCPP_AUTOGEN_DIR}"
  RESULT_VARIABLE _SCRIPT_RESULT
)

if (NOT _SCRIPT_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to generate webgpu cpp header: ${_SCRIPT_RESULT}")
endif ()

add_library(webgpu-cpp-header INTERFACE)
target_include_directories(webgpu-cpp-header INTERFACE
  $<BUILD_INTERFACE:${WEBGPUCPP_INCLUDE_DIR}>
  # The native extensions of wgpu-native, wgpuSetLogCallback() among them, are
  # declared in ffi/wgpu.h and are not part of the standard header the wrapper
  # is generated from. ffi/ is exposed so that "wgpu.h" can be included, and
  # ffi/webgpu-headers/ so that the "webgpu.h" it includes resolves to the very
  # same header that was copied into the directory above.
  $<BUILD_INTERFACE:${_WEBGPU_HEADER_DIR}/wgpu-cmake/wgpu-native/ffi>
  $<BUILD_INTERFACE:${_WEBGPU_HEADER_DIR}/wgpu-cmake/wgpu-native/ffi/webgpu-headers>
)

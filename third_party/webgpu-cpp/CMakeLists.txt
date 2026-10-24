cmake_minimum_required(VERSION 3.21)

find_package(Python3 COMPONENTS Interpreter)

# Which C header the wrapper is generated from.
#
# The engine is written against the standard WebGPU C API, and the wrapper is
# generated from whichever implementation the build links: wgpu-native on the
# desktop, emdawnwebgpu on the web.  The two headers track the same
# webgpu-headers repository, so the generated API is the same -- what differs is
# who implements it behind the scenes.
if(EMSCRIPTEN)
  set(WEBGPU_NATIVE_HEADER "${URGE_EMDAWNWEBGPU_INCLUDE}/webgpu/webgpu.h")
  if(NOT EXISTS "${WEBGPU_NATIVE_HEADER}")
    message(FATAL_ERROR
      "emdawnwebgpu's webgpu.h is missing at '${WEBGPU_NATIVE_HEADER}'.  "
      "third_party/cmake/emdawnwebgpu.cmake should have installed the port.")
  endif()
else()
  # Locate the C header next to this directory (third_party/webgpu-headers).
  get_filename_component(_WEBGPU_HEADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
  set(WEBGPU_NATIVE_HEADER "${_WEBGPU_HEADER_DIR}/wgpu-cmake/wgpu-native/ffi/webgpu-headers/webgpu.h")
endif()

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
)

if(NOT EMSCRIPTEN)
  # The native extensions of wgpu-native, wgpuSetLogCallback() among them, are
  # declared in ffi/wgpu.h and are not part of the standard header the wrapper
  # is generated from. ffi/ is exposed so that "wgpu.h" can be included, and
  # ffi/webgpu-headers/ so that the "webgpu.h" it includes resolves to the very
  # same header that was copied into the directory above.
  #
  # None of that exists on the web: emdawnwebgpu implements the standard API
  # and nothing else, which is the whole point of the switch.
  target_include_directories(webgpu-cpp-header INTERFACE
    $<BUILD_INTERFACE:${_WEBGPU_HEADER_DIR}/wgpu-cmake/wgpu-native/ffi>
    $<BUILD_INTERFACE:${_WEBGPU_HEADER_DIR}/wgpu-cmake/wgpu-native/ffi/webgpu-headers>
  )
endif()

cmake_minimum_required(VERSION 3.25)

project(URGE LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/third_party)

#--------------------------------------------------------------------------------
# Auto-generated directory include path
#--------------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter)

if(Python3_FOUND)
  set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
  message(STATUS "Found Python3: ${Python3_EXECUTABLE}")
else()
  message(FATAL_ERROR "Python3 not found.")
endif()

set(URGE_AUTOGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
include_directories("${URGE_AUTOGEN_DIR}")

#--------------------------------------------------------------------------------
# Platform options
#--------------------------------------------------------------------------------

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
endif ()

#--------------------------------------------------------------------------------
# Other definitions
#--------------------------------------------------------------------------------

add_compile_definitions(GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)

#--------------------------------------------------------------------------------
# 3rd
#--------------------------------------------------------------------------------

set(BUILD_SHARED_LIBS OFF)
add_subdirectory(third_party/glm)
add_subdirectory(third_party/SDL)
add_subdirectory(third_party/SDL_ttf)
add_subdirectory(third_party/physfs)
add_subdirectory(third_party/imgui)
add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/yaml-cpp)
add_subdirectory(third_party/mimalloc)
add_subdirectory(third_party/magic_enum)
add_subdirectory(third_party/webgpu-cpp)
add_subdirectory(third_party/wgpu-cmake)

#--------------------------------------------------------------------------------
# Core
#--------------------------------------------------------------------------------

add_subdirectory(base)
add_subdirectory(components)
add_subdirectory(content)
add_subdirectory(renderer)
add_subdirectory(ui)
add_subdirectory(binding)
add_subdirectory(app)

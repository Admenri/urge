cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Project Setup
#--------------------------------------------------------------------------------

project(URGE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
include_directories("${PROJECT_SOURCE_DIR}")

#--------------------------------------------------------------------------------
# Build Options
#--------------------------------------------------------------------------------

option(URGE_SHARED_RENDERER_LIBRARY "Use Dynamic Renderer Library in Engine Building" OFF)

#--------------------------------------------------------------------------------
# Platform and compiler check
#--------------------------------------------------------------------------------

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)
  link_libraries(
    "shell32;ws2_32;iphlpapi;imagehlp;shlwapi;bcrypt"
    "setupapi;advapi32;uuid;version;oleaut32;ole32;imm32;winmm;user32;rpcrt4;usp10"
  )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  link_libraries(crypt)
endif ()

#--------------------------------------------------------------------------------
# Auto-generated directory include path
#--------------------------------------------------------------------------------

find_package(Python3 COMPONENTS Interpreter)

if (Python3_FOUND)
    set(PYTHON_EXECUTABLE ${Python3_EXECUTABLE})
    message(STATUS "Found Python3: ${Python3_EXECUTABLE}")
else ()
    message(FATAL_ERROR "Python3 not found.")
endif ()

set(RGU_AUTOGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/gen")
include_directories("${RGU_AUTOGEN_DIR}")

#-------------------------------------------------------------------------
# * third party libraries
#-------------------------------------------------------------------------

include_directories(base/third_party)
include_directories(third_party)

### Android compile configure
set(SDL_STATIC on)
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  set(SDL_STATIC off)
endif ()

## SDL
add_subdirectory(third_party/SDL)
include_directories(third_party/SDL/include)

## image loader (SDL_image)
add_subdirectory(third_party/image_loader)
include_directories(third_party/image_loader/include)

## text render (SDL_ttf)
add_subdirectory(third_party/freetype)
include_directories(third_party/freetype/include)
add_subdirectory(third_party/text_render)
include_directories(third_party/text_render/include)

## Diligent Engine
add_subdirectory(third_party/DiligentCore)
include_directories(third_party/DiligentCore)

## IMGUI
add_subdirectory(third_party/imgui)
include_directories(third_party/imgui)

## Physfs
set(PHYSFS_BUILD_STATIC ON)
set(PHYSFS_BUILD_SHARED OFF)
set(PHYSFS_BUILD_SHARED OFF)
set(PHYSFS_BUILD_TEST OFF)
set(PHYSFS_BUILD_DOCS OFF)
add_subdirectory(third_party/physfs)
include_directories(third_party/physfs/src)

## Vorbis OGG
add_subdirectory(third_party/ogg)
add_subdirectory(third_party/vorbis)

## Opus decoder
set(OP_DISABLE_HTTP ON)
set(OP_DISABLE_DOCS ON)
set(OP_DISABLE_EXAMPLES ON)
add_subdirectory(third_party/opus)
add_subdirectory(third_party/opusfile)

## Spine2D
add_subdirectory(third_party/spine2d)
include_directories(third_party/spine2d/spine-cpp/include)

## spdlog
add_subdirectory(third_party/spdlog)

## miniaudio
set(MINIAUDIO_NO_DEVICEIO ON)
add_subdirectory(third_party/miniaudio)

## DAV1D
add_subdirectory(third_party/dav1d)

## AOMPLAYER
add_subdirectory(third_party/av1player)

## Webm
add_subdirectory(third_party/webm)

## Zlib
add_subdirectory(third_party/zlib)

## Asio
include_directories(third_party/asio/include)

## Mimalloc
set(MI_OVERRIDE OFF)
set(MI_BUILD_TESTS OFF)
set(MI_BUILD_OBJECT OFF)
set(MI_BUILD_SHARED OFF)
set(MI_BUILD_STATIC ON)
add_subdirectory(third_party/mimalloc)

## Private crypto
set(ENABLE_ADMENRI_BUILD_VERSION False)
set(ADMENRI_BUILD_BUNDLE_DIR ${PROJECT_SOURCE_DIR}/admenri)
if (EXISTS ${ADMENRI_BUILD_BUNDLE_DIR})
  set(ENABLE_ADMENRI_BUILD_VERSION True)
  message(STATUS "!!!!!!!!Enable Admenri Official Build Bundle Settings!!!!!!!!")
  add_subdirectory(admenri)
endif ()

#-------------------------------------------------------------------------
# * sub compinents
#-------------------------------------------------------------------------

add_subdirectory(components/audioservice)
add_subdirectory(components/filesystem)
add_subdirectory(components/fpslimiter)
add_subdirectory(components/network)
add_subdirectory(components/spine2d)

#-------------------------------------------------------------------------
# * source
#-------------------------------------------------------------------------

add_subdirectory(base)
add_subdirectory(ui)
add_subdirectory(renderer)
add_subdirectory(content)
add_subdirectory(binding/mri)
add_subdirectory(app)

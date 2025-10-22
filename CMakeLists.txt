cmake_minimum_required(VERSION 3.21)

#--------------------------------------------------------------------------------
# Project Setup
#--------------------------------------------------------------------------------

project(URGE LANGUAGES C CXX ASM)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include_directories("${PROJECT_SOURCE_DIR}")

#--------------------------------------------------------------------------------
# Build Options
#--------------------------------------------------------------------------------

option(URGE_SHARED_RENDERER_LIBRARY "Use Dynamic Renderer Library in Engine Building" OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_compile_options(-pthread)
endif ()

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

## Diligent Engine
add_subdirectory(third_party/DiligentCore)
include_directories(third_party/DiligentCore)

## image loader (SDL_image)
add_subdirectory(third_party/image_loader)
include_directories(third_party/image_loader/include)

## text render (SDL_ttf)
set(FT_DISABLE_HARFBUZZ ON)
set(FT_DISABLE_PNG ON)
set(FT_DISABLE_ZLIB ON)
set(FT_DISABLE_BZIP2 ON)
set(FT_DISABLE_BROTLI ON)
add_subdirectory(third_party/freetype)
include_directories(third_party/freetype/include)
add_subdirectory(third_party/text_render)
include_directories(third_party/text_render/include)

## IMGUI
add_subdirectory(third_party/imgui)
include_directories(third_party/imgui)

## Physfs
add_subdirectory(third_party/physfs)
include_directories(third_party/physfs/src)

## spdlog
add_subdirectory(third_party/spdlog)

## miniaudio
set(MINIAUDIO_NO_DEVICEIO ON)
add_subdirectory(third_party/miniaudio)

## ASIO
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  include_directories(third_party/asio/include)
endif ()

## Zlib
set(ZLIB_BUILD_SHARED OFF)
add_subdirectory(third_party/zlib)

## Media
### Vorbis OGG
add_subdirectory(third_party/ogg)
add_subdirectory(third_party/vorbis)
### Opus decoder
set(OP_DISABLE_HTTP ON)
set(OP_DISABLE_DOCS ON)
set(OP_DISABLE_EXAMPLES ON)
add_subdirectory(third_party/opus)
add_subdirectory(third_party/opusfile)
### WebP
add_subdirectory(third_party/libwebp)
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  ### DAV1D
  add_subdirectory(third_party/dav1d)
  ### Webm
  add_subdirectory(third_party/webm)
  ### AOMPLAYER
  add_subdirectory(third_party/av1player)
endif()

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
add_subdirectory(components/version)

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  add_subdirectory(components/network)
endif ()

#-------------------------------------------------------------------------
# * source
#-------------------------------------------------------------------------

add_subdirectory(base)
add_subdirectory(ui)
add_subdirectory(renderer)
add_subdirectory(content)
add_subdirectory(binding/mri)
add_subdirectory(app)

cmake_minimum_required(VERSION 3.16)

option(
  CUBISM_CORE_CRL_MD
  "Use Cubism Core that is multithread-specific and DLL-specific version"
  OFF
)

# Add Cubism Core.
# Import as static library.
add_library(Live2DCubismCore STATIC IMPORTED)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  message(STATUS "[CubismCore] Enable Windows build.")
  # Detect architecture.
  if(CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:x64")
    set(ARCH x86_64)
  elseif(CMAKE_EXE_LINKER_FLAGS STREQUAL "/machine:X86")
    set(ARCH x86)
  else()
    message(FATAL_ERROR "[${APP_NAME}] Invalid linker flag ${CMAKE_EXE_LINKER_FLAGS}.")
  endif()
  
  # Detect compiler.
  if(MSVC_VERSION MATCHES 1900)
    # Visual Studio 2015
    set(COMPILER 140)
  elseif(MSVC_VERSION GREATER_EQUAL 1910 AND MSVC_VERSION LESS 1920)
    # Visual Studio 2017
    set(COMPILER 141)
  elseif(MSVC_VERSION GREATER_EQUAL 1920 AND MSVC_VERSION LESS 1930)
    # Visual Studio 2019
    set(COMPILER 142)
  elseif(MSVC_VERSION GREATER_EQUAL 1930 AND MSVC_VERSION LESS 1950)
    # Visual Studio 2022
    set(COMPILER 143)
  elseif(MSVC)
    message(FATAL_ERROR "[CubismCore] Unsupported Visual C++ compiler used.")
  else()
    message(FATAL_ERROR "[CubismCore] Unsupported compiler used.")
  endif()
  
  # Detect core crt.
  if(CORE_CRL_MD)
    set(CRT MD)
  else()
    set(CRT MT)
  endif()
  
  # Find library path.
  set(CORE_LIB_SUFFIX ${CMAKE_CURRENT_SOURCE_DIR}/Core/lib/windows/${ARCH}/${COMPILER})
  set_target_properties(Live2DCubismCore
    PROPERTIES
      IMPORTED_LOCATION_DEBUG
        ${CORE_LIB_SUFFIX}/Live2DCubismCore_${CRT}d.lib
      IMPORTED_LOCATION_RELEASE
        ${CORE_LIB_SUFFIX}/Live2DCubismCore_${CRT}.lib
      IMPORTED_LOCATION_MINRELEASE
        ${CORE_LIB_SUFFIX}/Live2DCubismCore_${CRT}.lib
      IMPORTED_LOCATION_RELWITHDEBINFO
        ${CORE_LIB_SUFFIX}/Live2DCubismCore_${CRT}.lib
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Core/include"
  )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Find library path.
  set_target_properties(Live2DCubismCore
    PROPERTIES
      IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/Core/lib/linux/x86_64/libLive2DCubismCore.a
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Core/include"
  )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  # Find library path.
  set_target_properties(Live2DCubismCore
    PROPERTIES
      IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/Core/lib/android/${ANDROID_ABI}/libLive2DCubismCore.a
      INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/Core/include"
  )
endif ()

add_library(CubismFramework STATIC)
set(LIB_NAME CubismFramework)
add_subdirectory(Framework/src)
target_link_libraries(CubismFramework
 PUBLIC
  Live2DCubismCore
  engine_renderer
)

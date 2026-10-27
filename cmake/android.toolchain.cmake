# ---------------------------------------------------------------------------
# Android: find the NDK, then hand over to the one Google ships.
#
# A preset cannot name the NDK itself.  `CMAKE_TOOLCHAIN_FILE` is read before
# any of this project's cache variables exist, and the SDK installs its NDKs
# side by side under a version directory, so "$env{ANDROID_NDK_HOME}/..." is
# empty on the machine of anyone who only ran the SDK Manager.  This wrapper
# resolves the path and says what it found, so a machine without an NDK fails
# with one readable line instead of a missing-file error from deep inside CMake.
#
# Resolution order:
#   -DURGE_ANDROID_NDK=...      an explicit choice always wins
#   $ANDROID_NDK_HOME           what the SDK Manager's own scripts set
#   $ANDROID_NDK_ROOT           the older spelling
#   $ANDROID_HOME/ndk/<newest>  the SDK layout
#   $ANDROID_SDK_ROOT/ndk/<newest>
#   %LOCALAPPDATA%/Android/Sdk/ndk/<newest>   (Windows default install)
#
#   cmake --preset android-arm64
# ---------------------------------------------------------------------------

set(_urge_ndk "")

if(URGE_ANDROID_NDK)
  set(_urge_ndk "${URGE_ANDROID_NDK}")
elseif(DEFINED ENV{ANDROID_NDK_HOME} AND NOT "$ENV{ANDROID_NDK_HOME}" STREQUAL "")
  set(_urge_ndk "$ENV{ANDROID_NDK_HOME}")
elseif(DEFINED ENV{ANDROID_NDK_ROOT} AND NOT "$ENV{ANDROID_NDK_ROOT}" STREQUAL "")
  set(_urge_ndk "$ENV{ANDROID_NDK_ROOT}")
endif()

if(_urge_ndk STREQUAL "")
  set(_urge_sdk "")
  foreach(_env ANDROID_HOME ANDROID_SDK_ROOT)
    if(DEFINED ENV{${_env}} AND NOT "$ENV{${_env}}" STREQUAL "")
      set(_urge_sdk "$ENV{${_env}}")
      break()
    endif()
  endforeach()
  if(_urge_sdk STREQUAL "" AND DEFINED ENV{LOCALAPPDATA})
    set(_urge_sdk "$ENV{LOCALAPPDATA}/Android/Sdk")
  endif()

  if(IS_DIRECTORY "${_urge_sdk}/ndk")
    file(GLOB _urge_ndks LIST_DIRECTORIES true "${_urge_sdk}/ndk/*")
    if(_urge_ndks)
      # Several revisions may be installed; the highest one is the one the SDK
      # Manager would hand to Gradle as well.
      list(SORT _urge_ndks)
      list(GET _urge_ndks -1 _urge_ndk)
    endif()
  endif()
endif()

if(_urge_ndk STREQUAL "" OR
   NOT EXISTS "${_urge_ndk}/build/cmake/android.toolchain.cmake")
  message(FATAL_ERROR
    "URGE: no Android NDK found (looked for $ANDROID_NDK_HOME, "
    "$ANDROID_NDK_ROOT, $ANDROID_HOME/ndk/*).\n"
    "Install one with the SDK Manager, or pass -DURGE_ANDROID_NDK=<path>.")
endif()

message(STATUS "URGE: Android NDK = ${_urge_ndk}")

include("${_urge_ndk}/build/cmake/android.toolchain.cmake")

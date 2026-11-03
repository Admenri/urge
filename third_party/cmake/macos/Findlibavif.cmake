# libavif intentionally does not install a CMake package for static builds.
# Its AOM dependency must travel with the imported archive.
find_path(libavif_INCLUDE_DIR avif/avif.h PATHS "${URGE_MACOS_DEPS_PREFIX}/include" NO_DEFAULT_PATH)
find_library(libavif_LIBRARY NAMES libavif.a PATHS "${URGE_MACOS_DEPS_PREFIX}/lib" NO_DEFAULT_PATH)
find_library(libavif_AOM_LIBRARY NAMES libaom.a PATHS "${URGE_MACOS_DEPS_PREFIX}/lib" NO_DEFAULT_PATH)
if(libavif_INCLUDE_DIR)
  foreach(part MAJOR MINOR PATCH)
    file(STRINGS "${libavif_INCLUDE_DIR}/avif/avif.h" line REGEX "^#define AVIF_VERSION_${part} ")
    string(REGEX MATCH "[0-9]+$" libavif_${part} "${line}")
  endforeach()
  set(libavif_VERSION "${libavif_MAJOR}.${libavif_MINOR}.${libavif_PATCH}")
endif()
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libavif REQUIRED_VARS libavif_INCLUDE_DIR libavif_LIBRARY libavif_AOM_LIBRARY VERSION_VAR libavif_VERSION)
if(libavif_FOUND AND NOT TARGET avif)
  add_library(avif STATIC IMPORTED)
  set_target_properties(avif PROPERTIES IMPORTED_LOCATION "${libavif_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${libavif_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${libavif_AOM_LIBRARY}")
endif()

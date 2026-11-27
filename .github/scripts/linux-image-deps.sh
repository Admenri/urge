#!/usr/bin/env bash
# CI-only image codecs for the glibc 2.28 builder. Source builds keep using
# their normal dependency discovery and do not invoke this script.
set -euo pipefail

if [[ $# -ne 3 ]]; then
  echo "Usage: $0 <repository> <build-directory> <install-prefix>" >&2
  exit 2
fi

repo=$(cd "$1" && pwd)
mkdir -p "$2" "$3"
work=$(cd "$2" && pwd)
prefix=$(cd "$3" && pwd)
zlib_source="$repo/third_party/zlib"
png_source="$repo/third_party/SDL_image/external/libpng"
jpeg_source="$repo/third_party/SDL_image/external/jpeg"
tiff_source="$repo/third_party/SDL_image/external/libtiff"
aom_source="$repo/third_party/SDL_image/external/aom"
avif_source="$repo/third_party/SDL_image/external/libavif"
parallel=${CMAKE_BUILD_PARALLEL_LEVEL:-3}

test -f "$zlib_source/CMakeLists.txt"
test -f "$png_source/CMakeLists.txt"
test -f "$jpeg_source/CMakeLists.txt"
test -f "$tiff_source/CMakeLists.txt"
test -f "$aom_source/CMakeLists.txt"
test -f "$avif_source/CMakeLists.txt"

# TIFF's JPEG and Deflate codecs must not introduce additional runtime library
# requirements. Build their pinned repository dependencies into the CI prefix.
cmake -S "$zlib_source" -B "$work/zlib" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_STATIC=ON \
  -DZLIB_BUILD_TESTING=OFF -DZLIB_INSTALL=ON
cmake --build "$work/zlib" --parallel "$parallel"
cmake --install "$work/zlib"

cmake -S "$jpeg_source" -B "$work/jpeg" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF
cmake --build "$work/jpeg" --parallel "$parallel"
cmake --install "$work/jpeg"

# Exercise the static-PNG path in CI. SDL_image must link this archive instead
# of trying to dlopen it; the main CI configure sets SDLIMAGE_PNG_SHARED=OFF.
cmake -S "$png_source" -B "$work/libpng" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DZLIB_INCLUDE_DIR="$prefix/include" -DZLIB_LIBRARY="$prefix/lib/libz.a" \
  -DZLIB_LIBRARY_RELEASE="$prefix/lib/libz.a" \
  -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_FRAMEWORK=OFF \
  -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_EXECUTABLES=OFF \
  -DSKIP_INSTALL_EXECUTABLES=ON -DSKIP_INSTALL_PROGRAMS=ON
cmake --build "$work/libpng" --parallel "$parallel"
cmake --install "$work/libpng"

# Keep TIFF's built-in codecs, JPEG (including old-style JPEG), and Deflate /
# PixarLog enabled. Optional external codecs are disabled explicitly so their
# presence in the builder cannot add unbundled shared-library dependencies.
cmake -S "$tiff_source" -B "$work/libtiff" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF -Dtiff-static=ON -Dtiff-install=ON \
  -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-contrib=OFF -Dtiff-docs=OFF \
  -Dtiff-cxx=OFF -Dtiff-opengl=OFF \
  -Dccitt=ON -Dpackbits=ON -Dlzw=ON -Dthunder=ON -Dnext=ON -Dlogluv=ON -Dmdi=ON \
  -Dzlib=ON -Dpixarlog=ON -Dlibdeflate=OFF \
  -DZLIB_INCLUDE_DIR="$prefix/include" -DZLIB_LIBRARY="$prefix/lib/libz.a" \
  -DZLIB_LIBRARY_RELEASE="$prefix/lib/libz.a" \
  -Djpeg=ON -Dold-jpeg=ON -Djpeg12=OFF -Djpeg-prefer-standard=ON \
  -DJPEG_INCLUDE_DIR="$prefix/include" -DJPEG_LIBRARY="$prefix/lib/libjpeg.a" \
  -DJPEG_LIBRARY_RELEASE="$prefix/lib/libjpeg.a" \
  -Djbig=OFF -Dlerc=OFF -Dlzma=OFF -Dzstd=OFF -Dwebp=OFF
cmake --build "$work/libtiff" --parallel "$parallel"
cmake --install "$work/libtiff"

# libtiff's installed package leaves static dependency discovery as a TODO.
# Give SDL_image a complete target in this CI prefix; local source builds keep
# using their usual finders. Absolute archive paths also avoid cached system
# JPEG/ZLIB targets silently selecting shared libraries.
package="$prefix/lib/cmake/tiff"
cat > "$package/tiff-config.cmake" <<'CMAKE'
get_filename_component(_urge_ci_image_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET TIFF::tiff)
  add_library(TIFF::tiff STATIC IMPORTED GLOBAL)
  set_target_properties(TIFF::tiff PROPERTIES
    IMPORTED_LOCATION "${_urge_ci_image_prefix}/lib/libtiff.a"
    INTERFACE_INCLUDE_DIRECTORIES "${_urge_ci_image_prefix}/include"
    INTERFACE_LINK_LIBRARIES "${_urge_ci_image_prefix}/lib/libjpeg.a;${_urge_ci_image_prefix}/lib/libz.a;m")
endif()
if(NOT TARGET TIFF::TIFF)
  add_library(TIFF::TIFF ALIAS TIFF::tiff)
endif()
set(TIFF_INCLUDE_DIR "${_urge_ci_image_prefix}/include")
set(TIFF_INCLUDE_DIRS "${TIFF_INCLUDE_DIR}")
set(TIFF_LIBRARIES TIFF::tiff)
unset(_urge_ci_image_prefix)
CMAKE

# EL8's libavif is older than SDL_image's required version. Use the pinned
# repository sources, including both halves of AOM's AV1 codec.
cmake -S "$aom_source" -B "$work/aom" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DBUILD_SHARED_LIBS=OFF \
  -DCONFIG_PIC=1 -DCONFIG_AV1_ENCODER=1 -DCONFIG_AV1_DECODER=1 \
  -DENABLE_NASM=ON -DENABLE_DOCS=OFF -DENABLE_EXAMPLES=OFF \
  -DENABLE_TESTS=OFF -DENABLE_TESTDATA=OFF -DENABLE_TOOLS=OFF
cmake --build "$work/aom" --parallel "$parallel"
cmake --install "$work/aom"

export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake -S "$avif_source" -B "$work/libavif" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$prefix" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$prefix" -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DAVIF_CODEC_AOM=SYSTEM -DAVIF_CODEC_AOM_ENCODE=ON -DAVIF_CODEC_AOM_DECODE=ON \
  -DAOM_LIBRARY="$prefix/lib/libaom.a" -DAOM_INCLUDE_DIR="$prefix/include" \
  -DAVIF_CODEC_DAV1D=OFF -DAVIF_CODEC_LIBGAV1=OFF -DAVIF_CODEC_RAV1E=OFF \
  -DAVIF_CODEC_SVT=OFF -DAVIF_CODEC_AVM=OFF \
  -DAVIF_LIBYUV=OFF -DAVIF_LIBSHARPYUV=OFF -DAVIF_LIBXML2=OFF \
  -DAVIF_ZLIBPNG=OFF -DAVIF_JPEG=OFF \
  -DAVIF_BUILD_APPS=OFF -DAVIF_BUILD_EXAMPLES=OFF -DAVIF_BUILD_TESTS=OFF \
  -DAVIF_ENABLE_GTEST=OFF -DAVIF_ENABLE_FUZZTEST=OFF
cmake --build "$work/libavif" --parallel "$parallel"
cmake --install "$work/libavif"

# The vendored libavif does not install a CMake package for static builds.
# Its SYSTEM AOM dependency also remains a separate archive. Make both
# dependencies explicit in this CI prefix instead of changing the project
# finders or accidentally selecting EL8's older libavif package.
package="$prefix/lib/cmake/libavif"
mkdir -p "$package"
cat > "$package/libavif-config.cmake" <<'CMAKE'
include(CMakeFindDependencyMacro)
find_dependency(Threads)
get_filename_component(_urge_ci_image_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET avif)
  add_library(avif STATIC IMPORTED GLOBAL)
  set_target_properties(avif PROPERTIES
    IMPORTED_LOCATION "${_urge_ci_image_prefix}/lib/libavif.a"
    INTERFACE_INCLUDE_DIRECTORIES "${_urge_ci_image_prefix}/include"
    INTERFACE_LINK_LIBRARIES "${_urge_ci_image_prefix}/lib/libaom.a;Threads::Threads;m")
endif()
unset(_urge_ci_image_prefix)
CMAKE

cat > "$work/libavif-package-version.cmake" <<'CMAKE'
include(CMakePackageConfigHelpers)
foreach(part MAJOR MINOR PATCH)
  file(STRINGS "${PREFIX}/include/avif/avif.h" line
    REGEX "^#define AVIF_VERSION_${part} [0-9]+$")
  string(REGEX MATCH "[0-9]+$" version_${part} "${line}")
endforeach()
write_basic_package_version_file(
  "${PREFIX}/lib/cmake/libavif/libavif-config-version.cmake"
  VERSION "${version_MAJOR}.${version_MINOR}.${version_PATCH}"
  COMPATIBILITY SameMajorVersion)
CMAKE
cmake "-DPREFIX=$prefix" -P "$work/libavif-package-version.cmake"

mkdir -p "$prefix/licenses"
install -m 644 "$zlib_source/LICENSE" "$prefix/licenses/zlib.txt"
install -m 644 "$png_source/LICENSE" "$prefix/licenses/libpng.txt"
install -m 644 "$jpeg_source/COPYING" "$prefix/licenses/jpeg.txt"
printf '%s\n' 'This software is based in part on the work of the Independent JPEG Group.' \
  > "$prefix/licenses/jpeg-notice.txt"
install -m 644 "$tiff_source/LICENSE.md" "$prefix/licenses/libtiff.txt"
install -m 644 "$avif_source/LICENSE" "$prefix/licenses/libavif.txt"
install -m 644 "$aom_source/LICENSE" "$prefix/licenses/aom.txt"
install -m 644 "$aom_source/PATENTS" "$prefix/licenses/aom-patents.txt"
for item in \
  'third_party/fastfeat/LICENSE:aom-fastfeat.txt' \
  'third_party/vector/LICENSE:aom-vector.txt' \
  'third_party/x86inc/LICENSE:aom-x86inc.txt' \
  'third_party/libyuv/LICENSE:aom-libyuv.txt' \
  'third_party/SVT-AV1/LICENSE.md:aom-svt-av1.txt' \
  'third_party/SVT-AV1/PATENTS.md:aom-svt-av1-patents.txt'; do
  install -m 644 "$aom_source/${item%%:*}" "$prefix/licenses/${item#*:}"
done

test -f "$prefix/lib/libz.a"
test -f "$prefix/lib/libjpeg.a"
test -f "$prefix/lib/libpng16.a"
test -f "$prefix/lib/libtiff.a"
test -f "$prefix/lib/libaom.a"
test -f "$prefix/lib/libavif.a"
printf 'CI image libraries installed in %s\n' "$prefix"

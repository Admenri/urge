cmake_minimum_required(VERSION 3.30)

set(BUILD_SHARED_LIBS OFF)

# The top-level project drops cl's default /EHsc so its own targets can pass
# /EHs-c- without the D9025 contradiction.  Everything under here is upstream
# code and keeps the default: a vendored C++ target that uses exceptions must
# still compile with unwind semantics.
if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# SDL is static everywhere except Android, where it has to be the shared
# library the APK ships.  SDL's Android layer registers every native method
# SDLActivity calls from `JNI_OnLoad` (src/core/android/SDL_android.c), and the
# JVM only runs that for a library that was loaded by name -- which is why the
# Java side loads `libSDL3.so` and then `libmain.so`.  A static copy linked into
# libmain.so would leave those methods unregistered, and would also give the
# engine a second set of SDL globals to disagree with.
if(ANDROID)
  set(SDL_STATIC OFF)
  # SDL defaults both to BUILD_SHARED_LIBS, which this tree keeps off, so the
  # shared library has to be asked for by name here.
  set(SDL_SHARED ON)
else()
  set(SDL_STATIC ON)
endif()
set(SDLIMAGE_VENDORED OFF)

# SDL_ttf is built into the engine as a static library that carries its own
# FreeType and PlutoSVG copies (the "vendored" layout, which is also
# what the upstream project defaults to on MSVC). Samples and install rules are
# not wanted, and a warning is not an error in a bundled copy.
set(SDLTTF_VENDORED ON)
set(SDLTTF_SAMPLES OFF)
set(SDLTTF_INSTALL OFF)
set(SDLTTF_WERROR OFF)

# HarfBuzz is what SDL_ttf shapes complex scripts with -- Arabic, Thai, the
# Indic family -- and it is by far the heaviest thing in the vendored set (its
# own copy alone is a hundred megabytes of source).  The engine renders CJK,
# Latin and Cyrillic, all of which FreeType's own layout handles, so the shaping
# library is not built; SDL_ttf falls back to plain FreeType without it.
set(SDLTTF_HARFBUZZ OFF)

# miniaudio renders into a buffer the engine hands to SDL, so it must not open
# a sound card of its own: SDL owns the device and the format negotiation.
set(MINIAUDIO_NO_DEVICEIO ON)
set(MINIAUDIO_DEBUG_OUTPUT OFF)

# ---------------------------------------------------------------------------
# The Xiph codecs: libogg, libvorbis, libopus and libopusfile.  They are
# submodules under third_party/miniaudio/external/, pinned to a release tag,
# and they are what decodes Ogg Vorbis and Ogg Opus for miniaudio now (see
# third_party/miniaudio) -- replacing the stb_vorbis copy the tree used to
# carry.
#
# They sit inside miniaudio's own `external/` directory because that library is
# their only consumer: the two decoder backends under miniaudio/decoders are the
# one place in the tree that opens them.  Keeping them next to the consumer is
# what makes it obvious that dropping Ogg support means dropping four
# submodules and nothing else.
#
# Their upstream CMake projects are used as they ship, so the submodules stay
# pristine.  Everything a bundled copy has no business doing is turned off:
# tests, command line tools, pkg-config files and CMake package exports.
# OPUS_STATIC_RUNTIME has to match the runtime the rest of the tree links
# against (see the MSVC block in the top-level CMakeLists.txt).  The tree is on
# the static CRT, so opus is too; the option only changes which runtime opus'
# own targets select.
#
# BUILD_TESTING is a cache entry rather than a plain variable because libogg
# opens with `cmake_minimum_required(VERSION 2.8.12)`, where `option()` still
# overwrites a normal variable (policy CMP0077 is OLD at that level) and
# `include(CTest)` would put the setting back to ON.
#
# CMAKE_POLICY_VERSION_MINIMUM is the other half of that.  CMake 4 refuses to
# configure a project that asks for less than 3.5, and libogg and libvorbis
# both still ask for 2.8.12; raising the floor here keeps the submodules
# untouched instead of patching their CMakeLists.  It only takes effect for
# projects whose own minimum is lower, so nothing else changes.
# ---------------------------------------------------------------------------
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(BUILD_TESTING OFF CACHE BOOL "Build the testing tree" FORCE)
set(OPUS_BUILD_TESTING OFF)
set(OPUS_BUILD_PROGRAMS OFF)
set(OPUS_STATIC_RUNTIME ON CACHE BOOL
  "Build opus with the static MSVC runtime" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
set(INSTALL_DOCS OFF)
set(INSTALL_PKG_CONFIG_MODULE OFF)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF)

# ---------------------------------------------------------------------------
# libwebp backs SDL_image's WEBP backend.  Only the libraries are wanted, not
# the cwebp/dwebp/vwebp tools or the helpers they link.
#
# It sits at SDL_image/external/libwebp -- the slot SDL_image itself reserves
# for a vendored WEBP -- but the tree still does not turn SDLIMAGE_VENDORED on,
# because that would pull in the rest of SDL_image's external/ tree (jpeg, png,
# tiff, jxl, avif) as well.  SDL_image therefore takes WEBP from a library the
# surrounding build already compiles: statically, since the engine is one
# executable, so SDLIMAGE_WEBP_SHARED is off and the lookup goes through
# find_package(webp), which third_party/cmake/Findwebp.cmake answers with the
# in-tree target.
# ---------------------------------------------------------------------------
set(WEBP_BUILD_ANIM_UTILS OFF)
set(WEBP_BUILD_CWEBP OFF)
set(WEBP_BUILD_DWEBP OFF)
set(WEBP_BUILD_GIF2WEBP OFF)
set(WEBP_BUILD_IMG2WEBP OFF)
set(WEBP_BUILD_VWEBP OFF)
set(WEBP_BUILD_WEBPINFO OFF)
set(WEBP_BUILD_WEBPMUX OFF)
set(WEBP_BUILD_EXTRAS OFF)
set(WEBP_BUILD_LIBWEBPMUX ON)
set(SDLIMAGE_WEBP_SHARED OFF)

# third_party/cmake holds the CMake the parent has to supply around the
# vendored projects: the finders that resolve Ogg and webp to the targets this
# tree builds rather than to installed copies of the libraries, and the build
# file for libopusfile, which ships none of its own.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# libcurl carries the HTTP and WebSocket client.  Only the static library is
# wanted, so the tool, the examples and the test suite are all off, and the
# protocols the engine never speaks (mail, news, telnet, and the rest) are
# compiled out too.  HTTP_ONLY is deliberately *not* used: it also disables
# WebSocket, which is the other half of what this dependency is for.
set(BUILD_CURL_EXE OFF)
set(BUILD_STATIC_LIBS ON)
set(BUILD_EXAMPLES OFF)
# libcurl selects its MSVC runtime itself, and its default is the DLL one --
# which would leave vcruntime140.dll in the executable no matter what the rest
# of the tree does.  Point it at the static runtime the tree links against.
set(CURL_STATIC_CRT ON CACHE BOOL
  "Build libcurl with the static MSVC runtime" FORCE)
set(CURL_DISABLE_INSTALL ON)
# The build-tree package libcurl writes (`libcurl-target.cmake`) is for
# external projects that consume the library; this one links the target
# directly and never reads it.  It also fails generation where a system zlib is
# found: FindZLIB then leaves the vendored zlib's `ZLIB::ZLIB` alias in place,
# which points at a target outside the export set.
set(CURL_ENABLE_EXPORT_TARGET OFF)
set(CURL_BUILD_EVERYTHING OFF)
set(CURL_WERROR OFF)
set(CURL_DISABLE_DICT ON)
set(CURL_DISABLE_FILE ON)
set(CURL_DISABLE_FTP ON)
set(CURL_DISABLE_GOPHER ON)
set(CURL_DISABLE_IMAP ON)
set(CURL_DISABLE_IPFS ON)
set(CURL_DISABLE_LDAP ON)
set(CURL_DISABLE_LDAPS ON)
set(CURL_DISABLE_MQTT ON)
set(CURL_DISABLE_POP3 ON)
set(CURL_DISABLE_RTSP ON)
set(CURL_DISABLE_SMB ON)
set(CURL_DISABLE_SMTP ON)
set(CURL_DISABLE_TELNET ON)
set(CURL_DISABLE_TFTP ON)
# libpsl only feeds the public-suffix list used for cookie domain matching,
# which the engine has no use for; dropping it removes a dependency.
set(CURL_USE_LIBPSL OFF)

# System TLS, as in NETWORK_DESIGN.md 5.2.  Neither backend is picked by
# default, and with none chosen libcurl is built without HTTPS or WSS at all,
# so one has to be named per platform.  Schannel is the Windows native stack
# and needs nothing installed; curl 8.x dropped its Secure Transport backend,
# so macOS falls back to OpenSSL like Linux does.
#
# Android is the exception: its NDK carries no OpenSSL, and the BoringSSL it
# does use internally exposes no linkable SSL API, so the TLS backend there is
# the mbedTLS vendored below.
if(WIN32)
  set(CURL_USE_SCHANNEL ON)
elseif(ANDROID)
  set(CURL_USE_MBEDTLS ON)
else()
  set(CURL_USE_OPENSSL ON)
endif()

add_subdirectory(glm)
add_subdirectory(SDL)

# libwebp lives inside SDL_image's own `external/` directory -- the slot
# upstream keeps for a vendored WEBP backend -- because the image library is
# its only consumer in this tree.  It still has to come up first: SDL_image's
# WEBP backend links the targets it defines.
add_subdirectory(SDL_image/external/libwebp)

add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(physfs)

# The Xiph codecs live inside miniaudio's `external/` directory: the audio
# engine's two decoder backends (decoders/libvorbis, decoders/libopus) are the
# only consumers, so they travel with it instead of sitting at the top of
# third_party/.  libogg comes first: libvorbis and libopusfile both link it.
add_subdirectory(miniaudio/external/ogg)
add_subdirectory(miniaudio/external/vorbis)
add_subdirectory(miniaudio/external/opus)

# libopusfile is the odd one out: upstream ships autotools only, so the build
# file is ours and lives in third_party/cmake/ rather than inside the submodule.
include(cmake/opusfile.cmake)

add_subdirectory(miniaudio)

# The web build's WebGPU implementation, before anything asks for its header:
# webgpu-cpp generates the C++ wrapper from it at configure time.
if(EMSCRIPTEN)
  include(cmake/emdawnwebgpu.cmake)
endif()

add_subdirectory(webgpu-cpp)
add_subdirectory(imgui)
add_subdirectory(wgpu-cmake)

# The interpreter.  It is always the CRuby in third_party/ruby -- there is no
# other source for `ruby-static` -- and this is the only place it is compiled.
add_subdirectory(ruby)

add_subdirectory(zlib)

# libffi exists in this tree for one consumer: the Win32API binding
# (bind/binding_win32api.cc), which uses it to call an arbitrary DLL export
# with a signature only known at runtime.  That feature is meaningless off
# Windows, so the dependency is only brought up there -- on any other platform
# Win32API has nothing to call and the binding compiles to an empty
# initialiser.
if(WIN32)
  add_subdirectory(libffi)
endif()

if(URGE_ENABLE_VIDEO)
  # dav1d compiles its thirteen `*_tmpl.c` sources once per bit depth, so
  # dropping the 16-bit build removes thirteen translation units -- about a
  # tenth of what dav1d costs.  It also drops 10- and 12-bit AV1, so it is a
  # switch rather than the default: a video that decodes today should not stop
  # decoding because someone wanted a slightly faster build.
  option(URGE_VIDEO_AV1_8BIT_ONLY
    "Build dav1d for 8-bit video only (drops 10/12-bit AV1)" OFF)
  if(URGE_VIDEO_AV1_8BIT_ONLY)
    set(DAV1D_BITDEPTHS 8 CACHE STRING "enabled bitdepths" FORCE)
  endif()

  add_subdirectory(dav1d)
  add_subdirectory(webm)
endif()

# mbedTLS is Android's TLS backend for libcurl, since the NDK has no OpenSSL to
# link against.  libcurl is the only consumer: its finder module looks for an
# installed copy otherwise, and it shadows this tree's CMAKE_MODULE_PATH, so
# the build-tree archives are handed to it through the input variables it
# documents.  Only the three static libraries are wanted: the command line
# tools, the test suite and the install rules are all off.
# `MBEDTLS_FATAL_WARNINGS` has to go too, or a warning from a newer NDK clang
# becomes a build failure in code this tree does not own.
if(URGE_ENABLE_NETWORK AND NOT EMSCRIPTEN AND ANDROID)
  set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
  set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
  set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
  set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)
  add_subdirectory(mbedtls)

  set(MBEDTLS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls/include"
    CACHE PATH "mbedTLS headers for libcurl" FORCE)
  set(_urge_mbedtls_libdir "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library")
  set(MBEDTLS_LIBRARY
    "${_urge_mbedtls_libdir}/${CMAKE_STATIC_LIBRARY_PREFIX}mbedtls${CMAKE_STATIC_LIBRARY_SUFFIX}"
    CACHE FILEPATH "mbedTLS library for libcurl" FORCE)
  set(MBEDX509_LIBRARY
    "${_urge_mbedtls_libdir}/${CMAKE_STATIC_LIBRARY_PREFIX}mbedx509${CMAKE_STATIC_LIBRARY_SUFFIX}"
    CACHE FILEPATH "mbedTLS X.509 library for libcurl" FORCE)
  set(MBEDCRYPTO_LIBRARY
    "${_urge_mbedtls_libdir}/${CMAKE_STATIC_LIBRARY_PREFIX}mbedcrypto${CMAKE_STATIC_LIBRARY_SUFFIX}"
    CACHE FILEPATH "mbedTLS crypto library for libcurl" FORCE)
  unset(_urge_mbedtls_libdir)
endif()

if(URGE_ENABLE_NETWORK AND NOT EMSCRIPTEN)
  add_subdirectory(curl)
endif()

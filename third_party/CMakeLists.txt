cmake_minimum_required(VERSION 3.30)

set(BUILD_SHARED_LIBS OFF)
set(SDL_STATIC ON)
set(SDLIMAGE_VENDORED OFF)
set(YAML_CPP_BUILD_TOOLS OFF)

# SDL_ttf is built into the engine as a static library that carries its own
# FreeType, HarfBuzz and PlutoSVG copies (the "vendored" layout, which is also
# what the upstream project defaults to on MSVC). Samples and install rules are
# not wanted, and a warning is not an error in a bundled copy.
set(SDLTTF_VENDORED ON)
set(SDLTTF_SAMPLES OFF)
set(SDLTTF_INSTALL OFF)
set(SDLTTF_WERROR OFF)

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
# against (see the MSVC block in the top-level CMakeLists.txt) -- the tree is
# on the DLL runtime because the script interpreter needs it, so opus is too.
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
set(OPUS_STATIC_RUNTIME OFF)
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
set(BUILD_SHARED_LIBS OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_TESTING OFF)
set(CURL_DISABLE_INSTALL ON)
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
if(WIN32)
  set(CURL_USE_SCHANNEL ON)
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
add_subdirectory(webgpu-cpp)
add_subdirectory(imgui)
add_subdirectory(wgpu-cmake)

# glslang and spirv-reflect used to sit here.  The renderer now speaks WGSL --
# the shading language both wgpu backends consume natively -- so no GLSL is
# compiled at run time any more and there is no SPIR-V to reflect over.  The
# reflection the pipelines need is read out of the WGSL source by
# core/wgsl_reflect.cc.

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

if(URGE_ENABLE_NETWORK AND NOT EMSCRIPTEN)
  add_subdirectory(curl)
endif()

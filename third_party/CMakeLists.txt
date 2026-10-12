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

set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_GLSLANG_JS OFF)
set(GLSLANG_TESTS OFF)
set(GLSLANG_ENABLE_INSTALL OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
set(ENABLE_SPIRV ON)

# miniaudio renders into a buffer the engine hands to SDL, so it must not open
# a sound card of its own: SDL owns the device and the format negotiation.
set(MINIAUDIO_NO_DEVICEIO ON)
set(MINIAUDIO_DEBUG_OUTPUT OFF)

# ---------------------------------------------------------------------------
# The Xiph codecs: libogg, libvorbis, libopus and libopusfile.  They are
# submodules under third_party/, pinned to a release tag, and they are what
# decodes Ogg Vorbis and Ogg Opus for miniaudio now (see
# third_party/miniaudio) -- replacing the stb_vorbis copy the tree used to
# carry.
#
# Their upstream CMake projects are used as they ship, so the submodules stay
# pristine.  Everything a bundled copy has no business doing is turned off:
# tests, command line tools, pkg-config files and CMake package exports.
# OPUS_STATIC_RUNTIME has to match the runtime the rest of the tree links
# against (see the MSVC block in the top-level CMakeLists.txt) -- without it
# opus is built against the DLL runtime and the final link fails.
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
set(OPUS_STATIC_RUNTIME ON)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
set(INSTALL_DOCS OFF)
set(INSTALL_PKG_CONFIG_MODULE OFF)
set(INSTALL_CMAKE_PACKAGE_MODULE OFF)

# ---------------------------------------------------------------------------
# libwebp backs SDL_image's WEBP backend.  Only the libraries are wanted, not
# the cwebp/dwebp/vwebp tools or the helpers they link.
#
# SDL_image takes WEBP either from a vendored copy of its own or from a library
# the surrounding build already compiles.  This tree does the latter, statically
# -- the engine is one executable -- so SDLIMAGE_WEBP_SHARED is off and the
# lookup goes through find_package(webp), which third_party/cmake/Findwebp.cmake
# answers with the in-tree target.
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

# The Tint SPIR-V reader builds its IR with SPIRV-Tools, which glslang carries
# in its External directory. Only the static library is wanted here.
set(SPIRV_SKIP_TESTS ON)
set(SPIRV_SKIP_EXECUTABLES ON)
set(SPIRV_WERROR OFF)
set(SPIRV_TOOLS_BUILD_STATIC ON)
set(SKIP_SPIRV_TOOLS_INSTALL ON)

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

# libwebp is what SDL_image's WEBP backend links against, so it has to be up
# before it.
add_subdirectory(libwebp)

add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(physfs)

# The Xiph codecs.  libogg comes first: libvorbis and libopusfile both link it.
add_subdirectory(ogg)
add_subdirectory(vorbis)
add_subdirectory(opus)

# libopusfile is the odd one out: upstream ships autotools only, so the build
# file is ours and lives in third_party/cmake/ rather than inside the submodule.
include(cmake/opusfile.cmake)

add_subdirectory(miniaudio)
add_subdirectory(webgpu-cpp)
add_subdirectory(imgui-docking)
add_subdirectory(wgpu-cmake)
add_subdirectory(glslang)
add_subdirectory(spirv-reflect)

# The interpreter is either compiled here or taken from bind/external, and the
# top-level CMakeLists.txt has already made that choice: it leaves
# URGE_EXTERNAL_RUBY_DIR unset when there is no drop-in, and this directory
# then supplies `ruby-static` itself.  Either way the target name is the same,
# so nothing downstream has to know which case it is in.
if(NOT URGE_EXTERNAL_RUBY_DIR)
  add_subdirectory(cruby)
endif()

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
  add_subdirectory(dav1d)
  add_subdirectory(webm)
endif()

if(URGE_ENABLE_NETWORK)
  add_subdirectory(curl)
endif()

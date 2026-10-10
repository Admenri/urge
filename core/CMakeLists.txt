cmake_minimum_required(VERSION 3.30)

add_library(urge-core STATIC)
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# `video.cc` always compiles: it is the `Video` class itself, which stays
# exported to Ruby and turns into the stub that raises while the subsystem is
# off.  What the switch removes is the engine behind it -- the demuxer, the
# decoder and the player -- and with them the only path to a dav1d header.
if(NOT URGE_ENABLE_VIDEO)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/video_[a-z_]+\\.cc$")
endif()

# `network.cc` is the front end and always compiles, like `video.cc`.  The two
# backends are mutually exclusive: `network_curl.cc` is the native one and
# `network_web.cc` the Emscripten one, and each defines `CreateNetworkBackend`
# for its platform alone -- so a build that kept both would fail to link.
if(NOT URGE_ENABLE_NETWORK)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/network_(curl|web)\\.cc$")
elseif(EMSCRIPTEN)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/network_curl\\.cc$")
else()
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/network_web\\.cc$")
endif()

# `archive.cc` is the stub `urge::archive` for a build with no `admenri/`
# drop-in; when the drop-in is there, its own `archive.cc` takes the place of
# this one.  Exactly one of the two defines the symbols in core/archive.h, so
# the stub is dropped from the glob rather than guarded from the inside.
if(URGE_ENABLE_ARCHIVE_ENCRYPTION)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/archive\\.cc$")
endif()

target_sources(urge-core PRIVATE ${CORE_SOURCES})

# The container: the archiver, its key, and Monocypher underneath.  The
# include directory is private -- nothing above `urge::archive` may reach for
# the format or the key, and the open-source build has neither.
if(URGE_ENABLE_ARCHIVE_ENCRYPTION)
  target_sources(urge-core PRIVATE
    "${PROJECT_SOURCE_DIR}/admenri/monocypher.c"
    "${PROJECT_SOURCE_DIR}/admenri/archive.cc"
    "${PROJECT_SOURCE_DIR}/admenri/acs_archiver.cc")
  target_include_directories(urge-core PRIVATE "${PROJECT_SOURCE_DIR}/admenri")
  target_link_libraries(urge-core PUBLIC zlibstatic)
endif()

if(URGE_ENABLE_VIDEO)
  target_compile_definitions(urge-core PUBLIC URGE_ENABLE_VIDEO=1)
else()
  target_compile_definitions(urge-core PUBLIC URGE_ENABLE_VIDEO=0)
endif()

if(URGE_ENABLE_NETWORK)
  target_compile_definitions(urge-core PUBLIC URGE_ENABLE_NETWORK=1)
else()
  target_compile_definitions(urge-core PUBLIC URGE_ENABLE_NETWORK=0)
endif()

target_link_libraries(urge-core PUBLIC glm SDL3::SDL3-static SDL3_ttf::SDL3_ttf SDL3_image::SDL3_image-static physfs-static)
target_link_libraries(urge-core PUBLIC webgpu-cpp-header glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits spirv-reflect)

target_link_libraries(urge-core PUBLIC webgpu_wgpu)
target_compile_definitions(urge-core PUBLIC WEBGPU_BACKEND_WGPU=1)

# The in-engine debug interface (core/imgui_manager.cc) lives in this library,
# so both the headers and the backend selector come from the ImGui target.
target_link_libraries(urge-core PUBLIC imgui-docking)

# The audio subsystem decodes with miniaudio and plays through SDL.
target_link_libraries(urge-core PUBLIC miniaudio)

# The video subsystem demuxes with libwebm and decodes AV1 with dav1d.  Neither
# target exists when the subsystem is off.
if(URGE_ENABLE_VIDEO)
  target_link_libraries(urge-core PUBLIC dav1d libwebm)
endif()

# The network subsystem's native backend is libcurl, which also carries the
# WebSocket transport.  The target is absent when the subsystem is off, and on
# the web the backend is Emscripten's own fetch/websocket instead -- those
# libraries belong on the final link, so they are added in app/.
if(URGE_ENABLE_NETWORK AND NOT EMSCRIPTEN)
  target_link_libraries(urge-core PUBLIC libcurl)
endif()

# A headless harness that drives the fetch and WebSocket backends against the
# Python server in test/network_server.py.  Kept behind a switch so a normal
# build never carries it.
if(URGE_BUILD_NETWORK_TESTS AND URGE_ENABLE_NETWORK AND NOT EMSCRIPTEN)
  add_executable(urge-network-test "${CMAKE_CURRENT_SOURCE_DIR}/../test/network_test.cc")
  target_link_libraries(urge-network-test PRIVATE urge-core)
endif()

# The packer that writes the containers.  A build-time tool rather than part of
# the engine: it links Monocypher and zlib directly and never touches urge-core,
# so it can be built on its own with
#   cmake --build build --target urge-acs-pack
if(URGE_ENABLE_ARCHIVE_ENCRYPTION AND URGE_BUILD_ACS_PACK)
  add_executable(urge-acs-pack
    "${PROJECT_SOURCE_DIR}/admenri/acs_pack.cc"
    "${PROJECT_SOURCE_DIR}/admenri/monocypher.c")
  target_include_directories(urge-acs-pack PRIVATE "${PROJECT_SOURCE_DIR}/admenri")
  target_link_libraries(urge-acs-pack PRIVATE zlibstatic)
endif()

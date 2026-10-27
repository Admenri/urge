cmake_minimum_required(VERSION 3.30)

add_library(urge-core STATIC)
file(GLOB_RECURSE CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cc")

# `video.cc` is the `Video` class itself: it stays exported to Ruby and turns
# into the stub that raises while the subsystem is off.  The switch removes the
# engine behind it -- demuxer, decoder and player -- and with them the only path
# to a dav1d header.
if(NOT URGE_ENABLE_VIDEO)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/video_[a-z_]+\\.cc$")
endif()

# `network.cc` is the front end and always compiles, like `video.cc`.  The two
# backends are mutually exclusive -- each defines `CreateNetworkBackend` for its
# own platform -- so a build that kept both would fail to link.
if(NOT URGE_ENABLE_NETWORK)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/network_(curl|web)\\.cc$")
elseif(EMSCRIPTEN)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/network_curl\\.cc$")
else()
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/network_web\\.cc$")
endif()

# `archive.cc` is the stub `urge::archive` for a build with no `admenri/`
# drop-in; the drop-in brings its own, and exactly one of the two may define the
# symbols in core/system/archive.h.
if(URGE_ENABLE_ARCHIVE_ENCRYPTION)
  list(FILTER CORE_SOURCES EXCLUDE REGEX "/archive\\.cc$")
endif()

target_sources(urge-core PRIVATE ${CORE_SOURCES})

if(URGE_ENABLE_ARCHIVE_ENCRYPTION)
  target_sources(urge-core PRIVATE
    "${PROJECT_SOURCE_DIR}/admenri/monocypher.c"
    "${PROJECT_SOURCE_DIR}/admenri/archive.cc"
    "${PROJECT_SOURCE_DIR}/admenri/acs_archiver.cc")
  target_include_directories(urge-core PRIVATE "${PROJECT_SOURCE_DIR}/admenri")
  target_link_libraries(urge-core PUBLIC zlibstatic)
endif()

target_compile_definitions(urge-core PUBLIC
  URGE_ENABLE_VIDEO=$<BOOL:${URGE_ENABLE_VIDEO}>
  URGE_ENABLE_NETWORK=$<BOOL:${URGE_ENABLE_NETWORK}>)

# On Android the one SDL in the process is the shared library the Java glue
# loaded; everywhere else it is the static copy.
if(ANDROID)
  set(URGE_SDL_TARGET SDL3::SDL3)
else()
  set(URGE_SDL_TARGET SDL3::SDL3-static)
endif()

# The shaders are WGSL, which both wgpu backends parse themselves: no GLSL front
# end, no SPIR-V reflector, and nothing reads the shaders at build time.
target_link_libraries(urge-core PUBLIC
  glm
  ${URGE_SDL_TARGET}
  SDL3_ttf::SDL3_ttf
  SDL3_image::SDL3_image-static
  physfs-static
  webgpu-cpp-header
  webgpu_wgpu
  imgui-docking
  miniaudio)

# wgpu-native's own extensions -- `wgpuDevicePoll`, `WGPUInstanceExtras`,
# `wgpu.h` -- which the web build does not have: there the same C API is
# implemented by emdawnwebgpu.
if(NOT EMSCRIPTEN)
  target_compile_definitions(urge-core PUBLIC WEBGPU_BACKEND_WGPU=1)
endif()

if(URGE_ENABLE_VIDEO)
  target_link_libraries(urge-core PUBLIC dav1d libwebm)
endif()

# The native backend is libcurl, which also carries the WebSocket transport.  On
# the web the backend is Emscripten's own fetch/websocket, and those libraries
# belong on the final link, so they are added in app/.
if(URGE_ENABLE_NETWORK AND NOT EMSCRIPTEN)
  target_link_libraries(urge-core PUBLIC libcurl)
endif()

# The packer that writes the containers: a build-time tool that links Monocypher
# and zlib directly and never touches urge-core.
if(URGE_ENABLE_ARCHIVE_ENCRYPTION AND URGE_BUILD_ACS_PACK)
  add_executable(urge-acs-pack
    "${PROJECT_SOURCE_DIR}/admenri/acs_pack.cc"
    "${PROJECT_SOURCE_DIR}/admenri/monocypher.c")
  target_include_directories(urge-acs-pack PRIVATE "${PROJECT_SOURCE_DIR}/admenri")
  target_link_libraries(urge-acs-pack PRIVATE zlibstatic)
endif()

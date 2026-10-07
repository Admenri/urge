cmake_minimum_required(VERSION 3.30)

add_executable(Game app.cc)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(Game PUBLIC resources/app.rc)
  target_sources(Game PUBLIC platform/win32.cc)
endif()

target_link_libraries(Game PUBLIC urge-core urge-binding)

# On the web the engine's network backend is the browser's own.  The WebSocket
# library is opt-in and has to reach the final link, which is here rather than
# in the static core library.
if(EMSCRIPTEN AND URGE_ENABLE_NETWORK)
  target_link_options(Game PUBLIC "-sFETCH=1")
  target_link_libraries(Game PUBLIC "-lwebsocket.js")
endif()

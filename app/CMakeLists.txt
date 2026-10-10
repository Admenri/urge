cmake_minimum_required(VERSION 3.30)

add_executable(Game app.cc)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(Game PUBLIC resources/app.rc)
  target_sources(Game PUBLIC platform/win32.cc)
endif()

target_link_libraries(Game PUBLIC urge-core urge-binding)

# The interpreter from bind/external is a DLL -- and so is the libgmp it loads
# -- so both have to sit next to the executable.  Copying them as part of the
# build keeps the output directory self-contained instead of relying on the
# loader finding bind/external from the working directory.
if(URGE_EXTERNAL_RUBY_DIR)
  add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${URGE_RUBY_DLLS} "$<TARGET_FILE_DIR:Game>"
    COMMENT "Copying the external CRuby runtime next to the executable")
endif()

# On the web the engine's network backend is the browser's own.  The WebSocket
# library is opt-in and has to reach the final link, which is here rather than
# in the static core library.
if(EMSCRIPTEN AND URGE_ENABLE_NETWORK)
  target_link_options(Game PUBLIC "-sFETCH=1")
  target_link_libraries(Game PUBLIC "-lwebsocket.js")
endif()

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

# ---------------------------------------------------------------------------
# The web build.
#
# `main()` is an ordinary blocking function here: the game's frame loop lives in
# Ruby (`loop { Graphics.update }`), so it cannot be turned inside out into
# `emscripten_set_main_loop` callbacks.  Instead each frame ends with an
# `emscripten_sleep(0)`, which Asyncify turns into "unwind the whole C stack,
# let the browser paint and handle input, then rewind and continue" -- the loop
# keeps its shape and the page stays responsive.  The same Asyncify pass is what
# makes CRuby's fibers work (see third_party/ruby/wasm/README.md), so the
# instrumentation is there anyway.
#
# `ASYNCIFY_STACK_SIZE` is the buffer the unwound frames are saved into.  The
# whole stack is saved on every frame, and the Ruby VM's own frames are wide, so
# the 4 KiB default is far too small.
#
# The game itself is preloaded into the module's in-memory filesystem: PhysFS
# then reads it like any other directory, with no JS bridge in the way.
# `URGE_WEB_ASSETS` names the directory to pack (its contents land at `/`).
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  set(URGE_WEB_ASSETS "" CACHE PATH
    "Game directory to preload into the module's filesystem (contents land at /)")

  target_link_options(Game PRIVATE
    "-sASYNCIFY"
    "-sASYNCIFY_STACK_SIZE=4194304"
    "-sSTACK_SIZE=16777216"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/urge_index.html")

  if(URGE_WEB_ASSETS)
    if(NOT IS_DIRECTORY "${URGE_WEB_ASSETS}")
      message(FATAL_ERROR
        "URGE_WEB_ASSETS is '${URGE_WEB_ASSETS}', which is not a directory.")
    endif()
    target_link_options(Game PRIVATE "--preload-file=${URGE_WEB_ASSETS}@/")
    message(STATUS "URGE: preloading '${URGE_WEB_ASSETS}' into the web build")
  endif()

  set_target_properties(Game PROPERTIES SUFFIX ".html")
endif()

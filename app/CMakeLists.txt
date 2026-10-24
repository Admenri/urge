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
# The game is not packed into the module.  A game is tens or hundreds of
# megabytes and `--preload-file` would make the page wait for all of it before
# the first frame, so instead the directory is published beside the module as
# `assets/` and indexed by `build_mapping.py`:
#
#     <game dir>  --build_mapping.py-->  async_mapping.js
#     <game dir>  --copy------------->   assets/
#
# `urge-helper.js` reads that index, and `core/system/filesystem.cc` asks it
# for the real name of every file before opening it -- which is what gives the
# browser the directory enumeration (and so the case-insensitive, extension
# optional names) that RGSS expects.  Files marked `preload` are fetched before
# the game starts; the rest are fetched the first time they are asked for.
#
# `URGE_WEB_EMBED_ASSETS` goes back to a single `Game.data`, for a build that
# has to be one file.  The index is still generated, and the bridge notices the
# files are already in the filesystem.
# ---------------------------------------------------------------------------
if(EMSCRIPTEN)
  set(URGE_WEB_ASSETS "" CACHE PATH
    "Game directory served beside the module as assets/ (contents land at /)")
  set(URGE_WEB_PRELOAD "*.ini,*.acs,Data/Scripts.*" CACHE STRING
    "Glob patterns the web build loads before the first frame")
  option(URGE_WEB_EMBED_ASSETS
    "Pack the game into Game.data instead of loading it on demand" OFF)

  target_link_options(Game PRIVATE
    "-sASYNCIFY"
    "-sASYNCIFY_STACK_SIZE=4194304"
    # The asset bridge blocks the engine on a download; Asyncify has to know the
    # import can sleep, or the wait would be a busy loop that never yields.
    # The animation frame wait is the same kind of import: it is what puts the
    # frame inside a requestAnimationFrame callback so the surface can present.
    "-sASYNCIFY_IMPORTS=emjs_load_file_async,urge_wait_animation_frame"
    "-sSTACK_SIZE=16777216"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    # The filesystem is only reached from JavaScript here -- the bridge writes
    # the files in -- and the linker cannot see a use it never compiles.
    "-sFORCE_FILESYSTEM=1"
    # The link half of the JavaScript exception model.  `-fexceptions` (set for
    # C++ at the top level) only marks the frames that may throw; this is what
    # makes the linker emit `invoke_*` wrappers so a `catch` actually runs.  It
    # is set here rather than globally because it is only meaningful on a C++
    # link, and the ruby executables are C.
    "-fexceptions"
    "-sDISABLE_EXCEPTION_CATCHING=0"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/urge_index.html")

  add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/urge-helper.js"
            "${CMAKE_CURRENT_BINARY_DIR}/urge-helper.js"
    COMMENT "URGE: copying the web runtime glue")

  set(URGE_WEB_MAPPING "${CMAKE_CURRENT_BINARY_DIR}/async_mapping.js")

  if(URGE_WEB_ASSETS)
    if(NOT IS_DIRECTORY "${URGE_WEB_ASSETS}")
      message(FATAL_ERROR
        "URGE_WEB_ASSETS is '${URGE_WEB_ASSETS}', which is not a directory.")
    endif()

    find_program(URGE_PYTHON NAMES python python3 py)
    if(NOT URGE_PYTHON)
      message(FATAL_ERROR
        "The web build indexes its assets with app/build_mapping.py, which "
        "needs a Python interpreter on PATH.")
    endif()

    # Re-run on every build: the input is a whole directory, which CMake
    # cannot watch.  The script rewrites the file only when its content
    # changed, so a build that touched nothing still touches nothing.
    add_custom_target(GameWebAssets ALL
      COMMAND "${URGE_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/build_mapping.py"
              --input "${URGE_WEB_ASSETS}"
              --output "${URGE_WEB_MAPPING}"
              --preload "${URGE_WEB_PRELOAD}"
      COMMENT "URGE: indexing '${URGE_WEB_ASSETS}' for the web build"
      VERBATIM)
    add_dependencies(Game GameWebAssets)

    if(URGE_WEB_EMBED_ASSETS)
      target_link_options(Game PRIVATE "--preload-file=${URGE_WEB_ASSETS}@/")
    else()
      add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${URGE_WEB_ASSETS}" "${CMAKE_CURRENT_BINARY_DIR}/assets"
        COMMENT "URGE: publishing the game directory beside the module")
    endif()

    message(STATUS "URGE: web assets from '${URGE_WEB_ASSETS}'")
  else()
    file(WRITE "${URGE_WEB_MAPPING}" "const asyncMappingData = {};\n")
  endif()

  set_target_properties(Game PROPERTIES SUFFIX ".html")
endif()

cmake_minimum_required(VERSION 3.30)

# Android has no executable of its own: the APK's Java side loads a shared
# library and looks up `SDL_main` in it, which `SDL_main_impl.h` defines for a
# callback-loop app (`SDL_MAIN_USE_CALLBACKS` in app.cc).  So the same source
# becomes `libmain.so` there and `Game.exe` everywhere else.
if(ANDROID)
  add_library(Game SHARED app.cc)
  set_target_properties(Game PROPERTIES OUTPUT_NAME main)
else()
  add_executable(Game app.cc)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(Game PUBLIC resources/app.rc)
  target_sources(Game PUBLIC platform/win32.cc)
endif()

if(ANDROID)
  target_sources(Game PUBLIC platform/android.cc)
endif()

target_link_libraries(Game PUBLIC urge-core urge-binding)

# ---------------------------------------------------------------------------
# The Android packaging step.
#
# The native side is built here, by the preset, and Gradle only packages what
# this build produced.  Both libraries have to reach jniLibs: SDL3 because its
# `JNI_OnLoad` is what registers the native methods SDLActivity calls, and
# libmain.so because that is where `SDL_main` lives.
# ---------------------------------------------------------------------------
if(ANDROID)
  set(URGE_ANDROID_PROJECT "${PROJECT_SOURCE_DIR}/android" CACHE PATH
    "Gradle project that packages the Android build")
  set(_urge_jnilibs
    "${URGE_ANDROID_PROJECT}/app/src/main/jniLibs/${ANDROID_ABI}")

  # A RelWithDebInfo build carries its debug info, and a hundred megabytes of
  # DWARF in a shared library is a download nobody wants.  The copy that goes
  # into the APK is stripped; the one in the build tree keeps everything, so a
  # native crash can still be read.
  get_filename_component(_urge_toolchain_bin "${CMAKE_C_COMPILER}" DIRECTORY)
  find_program(URGE_LLVM_STRIP NAMES llvm-strip llvm-strip.exe
    HINTS "${_urge_toolchain_bin}")
  if(NOT URGE_LLVM_STRIP)
    message(WARNING
      "URGE: llvm-strip was not found next to the compiler; the APK will "
      "carry unstripped libraries.")
  endif()

  add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_urge_jnilibs}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Game>" "${_urge_jnilibs}/libmain.so"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3-shared>" "${_urge_jnilibs}/libSDL3.so"
    COMMAND ${URGE_LLVM_STRIP} --strip-unneeded
            "${_urge_jnilibs}/libmain.so" "${_urge_jnilibs}/libSDL3.so"
    COMMENT "URGE: copying the Android libraries into ${_urge_jnilibs}"
    VERBATIM)
endif()

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
# The frame loop is SDL's callback loop here, exactly as it is on the desktop:
# `SDL_AppIterate` is called once per animation frame, and the frame runs on a
# Ruby Fiber that hands the thread back to it at the end.  Nothing waits for an
# animation frame -- the tick already is one -- so the module never suspends
# itself to go looking for the next one.
#
# Asyncify is still linked, for two things that genuinely have to suspend: the
# asset bridge, which blocks the engine on a download, and the interpreter's
# conservative collector, which finds the objects a C frame holds through
# `emscripten_scan_stack` / `emscripten_scan_registers` -- both of them
# Asyncify operations.  `ASYNCIFY_STACK_SIZE` is the buffer the unwound frames
# are saved into, and the Ruby VM's own frames are wide, so the 4 KiB default is
# far too small.
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
    # Nothing else here sleeps any more: the frame no longer goes looking for an
    # animation frame, because the host's tick already is one.
    "-sASYNCIFY_IMPORTS=emjs_load_file_async"
    "-sSTACK_SIZE=16777216"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sINITIAL_MEMORY=67108864"
    # The filesystem is only reached from JavaScript here -- the bridge writes
    # the files in -- and the linker cannot see a use it never compiles.
    "-sFORCE_FILESYSTEM=1"
    # No `-fexceptions` / `-sDISABLE_EXCEPTION_CATCHING=0` here: the engine has
    # no C++ `try`/`throw` left, so there is no handler for the linker to emit
    # an `invoke_*` wrapper around.  CRuby's `longjmp` is a separate mechanism
    # and still works, see the Emscripten block in the top level CMakeLists.
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/urge_index.html")

  # The shell page and the runtime glue are inputs to the link, not sources, so
  # nothing would notice them changing: `--shell-file` is an argument of the
  # link command and ninja does not read inside it.  `LINK_DEPENDS` is the
  # CMake way of saying "relink when this file changes", and the glue needs it
  # too because it is only copied by the POST_BUILD step below.
  set_property(TARGET Game APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/urge_index.html"
    "${CMAKE_CURRENT_SOURCE_DIR}/urge-helper.js")

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

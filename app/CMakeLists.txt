# Main entry
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Player entry source
#--------------------------------------------------------------------------------

set(URGE_DEP_LIBS
  engine_base
  engine_content
  engine_renderer
  engine_ui

  binding_mri
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_compile_definitions(NOMINMAX)

  add_executable(Game WIN32 urge_main.cc resources/app.rc)
  target_link_libraries(Game PUBLIC ${URGE_DEP_LIBS})
  if (URGE_SHARED_RENDERER_LIBRARY)
    copy_required_dlls(Game)
  endif ()
  set(APP_TARGET Game)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(Game urge_main.cc)
  target_link_libraries(Game PUBLIC ${URGE_DEP_LIBS})
  set(APP_TARGET Game)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  add_library(main SHARED urge_main.cc)
  target_link_libraries(main PUBLIC ${URGE_DEP_LIBS})
  set(APP_TARGET main)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
  # Emscripten with "index.html" for executable file
  add_executable(index urge_main.cc)
  target_link_libraries(index PUBLIC ${URGE_DEP_LIBS})
  
  # Emscripten resource package
  set(EMSCRIPTEN_SHELL_TEMPLATE "${CMAKE_CURRENT_SOURCE_DIR}/urge_index.html")
  # Make link flags
  set(EMSCRIPTEN_LINK_FLAGS "--bind -sASYNCIFY=1 -sFORCE_FILESYSTEM=1 -sWASM=1 -sENVIRONMENT=web,worker -sALLOW_MEMORY_GROWTH=1 -sAUDIO_WORKLET=1 -sWASM_WORKERS")
  # Setup logging verbose
  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -sASSERTIONS=0")
  else ()
    set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -sASSERTIONS=2")
  endif ()
  # Stack size
  set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} -sSTACK_SIZE=16MB -sASYNCIFY_STACK_SIZE=16MB -sTOTAL_MEMORY=128MB -sMAXIMUM_MEMORY=1073741824")
  # Shell
  set(EMSCRIPTEN_LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS} --shell-file ${EMSCRIPTEN_SHELL_TEMPLATE}")

  # Setup index.html
  set(APP_TARGET index)
  set_target_properties(index PROPERTIES
    SUFFIX ".html"
    LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}"
  )
endif ()

if (ENABLE_ADMENRI_BUILD_VERSION)
  target_link_libraries(${APP_TARGET} PRIVATE admenri_arb)
  target_compile_definitions(${APP_TARGET} PRIVATE HAVE_ARB_ENCRYPTO_SUPPORT=1)
endif ()

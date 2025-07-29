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
endif ()

if (ENABLE_ADMENRI_BUILD_VERSION)
  target_link_libraries(${APP_TARGET} PRIVATE admenri_arb)
  target_compile_definitions(${APP_TARGET} PRIVATE HAVE_ARB_ENCRYPTO_SUPPORT=1)
endif ()

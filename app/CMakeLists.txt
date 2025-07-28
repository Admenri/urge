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

  add_executable(urge_engine WIN32 urge_main.cc resources/app.rc)
  target_link_libraries(urge_engine PUBLIC ${URGE_DEP_LIBS})
  if (URGE_SHARED_RENDERER_LIBRARY)
    copy_required_dlls(urge_engine)
  endif ()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(urge_engine urge_main.cc)
  target_link_libraries(urge_engine PUBLIC ${URGE_DEP_LIBS})
elseif (CMAKE_SYSTEM_NAME STREQUAL "Android")
  add_library(main SHARED urge_main.cc)
  target_link_libraries(main PUBLIC ${URGE_DEP_LIBS})
endif ()

# unittests binding components
cmake_minimum_required(VERSION 3.18.0)

#--------------------------------------------------------------------------------
# unittests cpp binding
#--------------------------------------------------------------------------------

set(BINDING_UNITTESTS_SRC
  binding_unittests.cc
  binding_unittests.h
)

add_library(binding_unittests ${BINDING_UNITTESTS_SRC})
target_link_libraries(binding_unittests PUBLIC
  engine_base
  engine_renderer
  engine_content
  engine_ui
)

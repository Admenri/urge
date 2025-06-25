# Components - live2d cubism renderer
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of limiter
#--------------------------------------------------------------------------------

set(COMPONENTS_CUBISM_SRC
  cubism_model_manager.cc
  cubism_model_manager.h
)

add_library(components_cubismrenderer STATIC ${COMPONENTS_CUBISM_SRC})
target_link_libraries(components_cubismrenderer
 PUBLIC
  engine_base
  engine_renderer
  CubismFramework
  components_filesystem
)

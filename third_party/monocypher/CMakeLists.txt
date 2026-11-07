add_library(urge-monocypher STATIC monocypher.c)
target_include_directories(urge-monocypher PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(urge-monocypher PROPERTIES POSITION_INDEPENDENT_CODE ON)

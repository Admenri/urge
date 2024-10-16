# Components - fpslimiter
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of limiter
#--------------------------------------------------------------------------------

set(COMPONENTS_FPSLIMITER_SRC
  fpslimiter.cc
  fpslimiter.h
)

add_library(components_fpslimiter STATIC ${COMPONENTS_FPSLIMITER_SRC})
target_link_libraries(components_fpslimiter engine_base)

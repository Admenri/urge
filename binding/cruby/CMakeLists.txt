cmake_minimum_required(VERSION 3.25)

#--------------------------------------------------------------------------------
# CRuby binding backend
#--------------------------------------------------------------------------------

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(RB_INCLUDE "third_party/ruby_windows_x64/include/ruby-4.0.0" "third_party/ruby_windows_x64/include/ruby-4.0.0/x64-mingw-ucrt")
  set(RB_LIB "third_party/ruby_windows_x64/lib/libx64-ucrt-ruby400.dll.a")
endif()

add_library(binding_cruby STATIC)

target_sources(binding_cruby
 PUBLIC
  cruby_entry.cc
  cruby_entry.h
  cruby_file.cc
  cruby_file.h
)

target_include_directories(binding_cruby
 PUBLIC
  ${RB_INCLUDE}
)

target_link_libraries(binding_cruby
 PUBLIC
  engine_content
  ${RB_LIB}
)

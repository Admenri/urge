cmake_minimum_required(VERSION 3.25)

add_executable(urge)

target_sources(urge PUBLIC urge_main.cc)

target_link_libraries(urge
 PUBLIC
  engine_content
  engine_ui
)

add_dependencies(urge version_target)

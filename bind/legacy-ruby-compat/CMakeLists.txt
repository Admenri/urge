# Runtime compatibility sources stay in this module.
target_sources(urge-binding PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/legacy_ruby_compat.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/legacy_classes.cc")

cmake_minimum_required(VERSION 3.30)

add_library(urge-binding STATIC)

# `autogen/` holds what `bind/gen_binding.cmake` generated, this directory holds
# the hand-written half of the layer.  Both are compiled, so both are globbed.
#
# Two consequences of the glob:
#
#   * it sees whatever that run wrote, so a brand new autogen/binding_*.cc is
#     compiled without a second configure;
#   * a class that did not exist when the tree was last configured is only in
#     the *fresh* glob, so the first configure after adding a header needs the
#     whole glob re-expanded, which is what CONFIGURE_DEPENDS covers.  It is
#     set on this directory only: it is the one that depends on generated files.
file(GLOB BINDING_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/autogen/*.cc")
target_sources(urge-binding PRIVATE ${BINDING_SOURCES})

# CRuby's zlib extension is plain C, so the glob above misses it.
target_sources(urge-binding PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/zlib.c")

# The generated files include each other by bare name ("binding_init.h"), and
# cruby_main.cc includes them from the repository root ("bind/autogen/...",
# which the top-level `include_directories` covers).
target_include_directories(urge-binding PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/autogen)
target_link_libraries(urge-binding PUBLIC ruby-static urge-core zlibstatic)

# libffi backs the Win32API binding (binding_win32api.cc), which uses it to call
# a DLL export whose signature is only known at runtime.  The dependency is
# Windows-only for the same reason the binding is: `third_party/CMakeLists.txt`
# only brings the library up there, and off Windows the binding compiles to an
# empty initialiser that needs nothing from it.
if(WIN32)
  target_link_libraries(urge-binding PUBLIC libffi)
endif()

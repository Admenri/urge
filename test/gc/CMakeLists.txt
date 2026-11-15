cmake_minimum_required(VERSION 3.30)
project(urge_gc_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# The native heap and its tests intentionally do not link a script runtime,
# renderer, or the main engine target.
get_filename_component(URGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
add_executable(urge-gc-tests
  garbage_collector_test.cc
  "${URGE_ROOT}/core/common/garbage_collector.cc")
target_include_directories(urge-gc-tests PRIVATE "${URGE_ROOT}")
find_package(Threads REQUIRED)
target_link_libraries(urge-gc-tests PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(urge-gc-tests PRIVATE /UNDEBUG /utf-8)
else()
  target_compile_options(urge-gc-tests PRIVATE -UNDEBUG)
endif()

option(URGE_GC_NO_EXCEPTIONS "Test the engine's no-C++-exceptions contract" OFF)
if(URGE_GC_NO_EXCEPTIONS)
  if(MSVC)
    string(REGEX REPLACE "(^| )/EHsc( |$)" "\\1" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    target_compile_options(urge-gc-tests PRIVATE /EHs-c- /we4530)
    target_compile_definitions(urge-gc-tests PRIVATE _HAS_EXCEPTIONS=0)
  else()
    target_compile_options(urge-gc-tests PRIVATE -fno-exceptions)
  endif()
endif()

enable_testing()
add_test(NAME urge-gc-tests COMMAND urge-gc-tests)
set_tests_properties(urge-gc-tests PROPERTIES TIMEOUT 30)

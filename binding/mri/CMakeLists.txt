# Binding - MRI
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Ruby Interpreter import
#--------------------------------------------------------------------------------

## TODO: Enhance ruby build ins
if (CMAKE_SYSTEM_NAME STREQUAL "Android")
  add_subdirectory(third_party/ruby_android)
  include_directories(third_party/ruby_android/include)
else()
  # Find ruby headers, libs in system
  find_package(Ruby REQUIRED)
  set(RUBY_INCLUDE_PATH "${Ruby_INCLUDE_DIRS}")
  set(RUBY_LIBRARY_PATH "${Ruby_LIBRARIES}")

  # Find config file
  add_library(mri_ruby UNKNOWN IMPORTED)  
  if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Using specific configure for Windows
    # y1s1, Ruby's Windows adaptation is similar to dogshit :(
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
      message(FATAL_ERROR "URGE Ruby Configure: Unsupport architecture")
    elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(RUBY_INCLUDE_PATH
        "${PROJECT_SOURCE_DIR}/third_party/ruby_extern/config/"
        ${RUBY_INCLUDE_PATH}
      )
    endif()
  endif()

  message(STATUS "[Binding] Ruby Include Path: ${RUBY_INCLUDE_PATH}")
  message(STATUS "[Binding] Ruby Library: ${RUBY_LIBRARY_PATH}")
  
  set_target_properties(mri_ruby PROPERTIES
                        IMPORTED_LOCATION "${RUBY_LIBRARY_PATH}"
                        INTERFACE_INCLUDE_DIRECTORIES "${RUBY_INCLUDE_PATH}")
endif()

#--------------------------------------------------------------------------------
# Mri Gem Ext
#--------------------------------------------------------------------------------

if (NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
set(GEM_EXT_RUBYPROF_SRC
  "ruby_prof/rp_allocation.c"
  "ruby_prof/rp_call_tree.c"
  "ruby_prof/rp_call_trees.c"
  "ruby_prof/rp_measure_allocations.c"
  "ruby_prof/rp_measure_memory.c"
  "ruby_prof/rp_measure_process_time.c"
  "ruby_prof/rp_measure_wall_time.c"
  "ruby_prof/rp_measurement.c"
  "ruby_prof/rp_method.c"
  "ruby_prof/rp_profile.c"
  "ruby_prof/rp_stack.c"
  "ruby_prof/rp_thread.c"
  "ruby_prof/ruby_prof.c"
)
endif ()

set(GEM_EXT_ZLIB_SRC "zlib/zlib.c")

#--------------------------------------------------------------------------------
# Autogen MRI binding source collection
#--------------------------------------------------------------------------------

set(PYTHON_SCRIPT "${PROJECT_SOURCE_DIR}/binding/mri/tools/autogen.py")
set(INPUT_BINDING_DIR "${PROJECT_SOURCE_DIR}/content/public/")
set(GENERATE_BINDING_DIR "${RGU_AUTOGEN_DIR}/binding/mri/")

execute_process(
  COMMAND "${PYTHON_EXECUTABLE}" -B "${PYTHON_SCRIPT}" "${INPUT_BINDING_DIR}" "${GENERATE_BINDING_DIR}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE SCRIPT_RESULT
)

if (NOT SCRIPT_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to create MRI binding source: ${SCRIPT_RESULT}")
endif ()

file(GLOB GENERATED_SOURCES CONFIGURE_DEPENDS "${GENERATE_BINDING_DIR}/*.cc")
file(GLOB GENERATED_HEADERS CONFIGURE_DEPENDS "${GENERATE_BINDING_DIR}/*.h")

set(BINDING_MRI_SRC
  mri_file.cc
  mri_file.h
  mri_main.cc
  mri_main.h
  mri_util.cc
  mri_util.h

  binding_patch.cc
  binding_patch.h

  ${GENERATED_SOURCES}
  ${GENERATED_HEADERS}

  ${GEM_EXT_RUBYPROF_SRC}
  ${GEM_EXT_ZLIB_SRC}
)

add_library(binding_mri STATIC ${BINDING_MRI_SRC})
target_link_libraries(binding_mri
 PUBLIC
  engine_base
  engine_content
  engine_renderer
  mri_ruby
  zlibstatic
)

if (ENABLE_ADMENRI_BUILD_VERSION)
  target_link_libraries(binding_mri PRIVATE admenri_arb)
  target_compile_definitions(binding_mri PRIVATE HAVE_GET_MACHINE_HASH=1)
endif ()

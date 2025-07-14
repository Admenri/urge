# Binding - MRI
cmake_minimum_required(VERSION 3.20.1)

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

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
include_directories(fiddle)
set(GEM_EXT_FIDDLE_SRC
  "fiddle/closure.c"
  "fiddle/conversions.c"
  "fiddle/fiddle.c"
  "fiddle/function.c"
  "fiddle/handle.c"
  "fiddle/memory_view.c"
  "fiddle/pinned.c"
  "fiddle/pointer.c"
)
endif ()

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
  ${GEM_EXT_FIDDLE_SRC}
  "zlib/zlib.c"
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

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_link_libraries(binding_mri PUBLIC ffi)
endif ()

if (ENABLE_ADMENRI_BUILD_VERSION)
  target_link_libraries(binding_mri PRIVATE admenri_arb)
  target_compile_definitions(binding_mri PRIVATE HAVE_GET_MACHINE_HASH=1)
endif ()

cmake_minimum_required(VERSION 3.25)

#--------------------------------------------------------------------------------
# Step 1: Generate JSON IDL from headers
#--------------------------------------------------------------------------------

set(URGE_ENGINE_IDL_JSON "${CMAKE_CURRENT_SOURCE_DIR}/idl/export.json")

execute_process(
  COMMAND "${PYTHON_EXECUTABLE}"
          -B "${CMAKE_CURRENT_SOURCE_DIR}/build/idl_gen.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/../content"
          -o "${URGE_ENGINE_IDL_JSON}"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  RESULT_VARIABLE SCRIPT_RESULT
)

if (NOT SCRIPT_RESULT EQUAL 0)
  message(FATAL_ERROR "failed to generate binding idl: ${SCRIPT_RESULT}")
endif ()

#--------------------------------------------------------------------------------
# Binding backend
#--------------------------------------------------------------------------------

add_subdirectory(cruby)

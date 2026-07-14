cmake_minimum_required(VERSION 3.25)

#--------------------------------------------------------------------------------
# Scan header files for dependency tracking
#--------------------------------------------------------------------------------

file(GLOB_RECURSE URGE_BINDING_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/../content/*.h"
)

#--------------------------------------------------------------------------------
# Generate JSON IDL from headers
#--------------------------------------------------------------------------------

set(URGE_ENGINE_IDL_JSON "${CMAKE_CURRENT_SOURCE_DIR}/interface/engine_defines.json")

execute_process(
  COMMAND ${PYTHON_EXECUTABLE} -B
          "${CMAKE_CURRENT_SOURCE_DIR}/build/idl_gen.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/../content"
          -b "${CMAKE_CURRENT_SOURCE_DIR}/.."
          -o "${URGE_ENGINE_IDL_JSON}"
  WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
  RESULT_VARIABLE SCRIPT_RESULT
)

#--------------------------------------------------------------------------------
# Binding backend
#--------------------------------------------------------------------------------

if(URGE_BINDING_BACKEND STREQUAL "CRUBY")
  add_subdirectory(cruby)
endif()

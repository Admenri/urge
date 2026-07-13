cmake_minimum_required(VERSION 3.25)

#--------------------------------------------------------------------------------
# Scan header files for dependency tracking
#--------------------------------------------------------------------------------

file(GLOB_RECURSE URGE_BINDING_HEADERS
  "${CMAKE_CURRENT_SOURCE_DIR}/../content/*.h"
)

#--------------------------------------------------------------------------------
# Step 1: Generate JSON IDL from headers
#--------------------------------------------------------------------------------

set(URGE_ENGINE_IDL_JSON "${CMAKE_CURRENT_BINARY_DIR}/engine_idl.json")

add_custom_command(
  OUTPUT "${URGE_ENGINE_IDL_JSON}"
  COMMAND ${PYTHON_EXECUTABLE}
          "${CMAKE_CURRENT_SOURCE_DIR}/build/bind_parser.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/../content"
          -b "${CMAKE_CURRENT_SOURCE_DIR}/.."
          -o "${URGE_ENGINE_IDL_JSON}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/build/bind_parser.py"
          ${URGE_BINDING_HEADERS}
  COMMENT "Generating engine IDL from URGE_BINDING headers"
)

#--------------------------------------------------------------------------------
# Step 2: Generate C++ glue code from headers
#--------------------------------------------------------------------------------

set(URGE_BINDING_GEN_H "${URGE_AUTOGEN_DIR}/engine_binding_gen.h")
set(URGE_BINDING_GEN_CC "${URGE_AUTOGEN_DIR}/engine_binding_gen.cc")

add_custom_command(
  OUTPUT "${URGE_BINDING_GEN_H}" "${URGE_BINDING_GEN_CC}"
  COMMAND ${PYTHON_EXECUTABLE}
          "${CMAKE_CURRENT_SOURCE_DIR}/build/bind_parser.py"
          "${CMAKE_CURRENT_SOURCE_DIR}/../content"
          -b "${CMAKE_CURRENT_SOURCE_DIR}/.."
          --gen-h "${URGE_BINDING_GEN_H}"
          --gen-cc "${URGE_BINDING_GEN_CC}"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/build/bind_parser.py"
          ${URGE_BINDING_HEADERS}
  COMMENT "Generating C++ binding glue code from URGE_BINDING headers"
)

#--------------------------------------------------------------------------------
# Custom target: engine_binding_gen
#--------------------------------------------------------------------------------

add_custom_target(engine_binding_gen
  DEPENDS "${URGE_ENGINE_IDL_JSON}"
          "${URGE_BINDING_GEN_H}"
          "${URGE_BINDING_GEN_CC}"
)

#--------------------------------------------------------------------------------
# Binding backend
#--------------------------------------------------------------------------------

if(URGE_BINDING_BACKEND STREQUAL "CRUBY")
  add_subdirectory(cruby)
endif()

cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# Source of engine renderer
#--------------------------------------------------------------------------------

set(ENGINE_RENDERER_SRC
  device/render_device.cc
  device/render_device.h
  drawable/quad_array.cc
  drawable/quad_array.h
  drawable/quad_drawable.cc
  drawable/quad_drawable.h
  drawable/vertex_layout.cc
  drawable/vertex_layout.h
  pipeline/render_pipeline.cc
  pipeline/render_pipeline.h
)

#--------------------------------------------------------------------------------
# Auto-gen shader embed binary source
#--------------------------------------------------------------------------------

file(GLOB RENDERER_SHADER_FILES "${PROJECT_SOURCE_DIR}/renderer/hlsl/*")
foreach(SHADER_FILE ${RENDERER_SHADER_FILES})
  get_filename_component(FILE_EXTNAME ${SHADER_FILE} EXT)
  get_filename_component(FILE_NAME ${SHADER_FILE} NAME)
  if (FILE_EXTNAME STREQUAL ".hlsl")
    execute_process(
      COMMAND "${PYTHON_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/buildtools/build_file_binary.py" "${SHADER_FILE}" -o "${RGU_AUTOGEN_DIR}/renderer/hlsl/${FILE_NAME}.xxd"
      RESULT_VARIABLE SHADER_GENERATE_RESULT
      OUTPUT_QUIET
    )
    if(NOT SHADER_GENERATE_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to generate shader embed file: ${SHADER_GENERATE_RESULT}")
    endif()
  endif()
endforeach()

#--------------------------------------------------------------------------------
# Engine renderer set
#--------------------------------------------------------------------------------

add_library(engine_renderer STATIC ${ENGINE_RENDERER_SRC})
target_link_libraries(engine_renderer PUBLIC
  engine_base

  Diligent-GraphicsEngine
  Diligent-GraphicsEngineOpenGL-static
  Diligent-GraphicsEngineD3D12-static
  Diligent-GraphicsEngineD3D11-static
  Diligent-GraphicsEngineVk-static
  Diligent-TextureLoader
)

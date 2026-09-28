cmake_minimum_required(VERSION 3.30)

# The Tint subset which reads SPIR-V and writes WGSL. It has no build of its own
# in the tree, so the library is a glob of its sources without the test files.
add_library(tint-extract STATIC)
file(GLOB_RECURSE TINT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")
list(FILTER TINT_SOURCES EXCLUDE REGEX "_test\\.cc$|_bench\\.cc$|_fuzz\\.cc$")

target_sources(tint-extract PRIVATE ${TINT_SOURCES})
target_include_directories(tint-extract PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
# The SPIR-V reader builds its IR with SPIRV-Tools and reads SPIR-V with the
# headers glslang carries in its External directory. The tables SPIRV-Tools
# generates are private to it, so its build directory is added here as well.
target_include_directories(tint-extract PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../glslang/External/spirv-tools"
  "${CMAKE_CURRENT_SOURCE_DIR}/../glslang/External/spirv-tools/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../glslang/External/spirv-headers/include"
  "${CMAKE_BINARY_DIR}/third_party/spirv-tools")
target_link_libraries(tint-extract PRIVATE SPIRV-Tools-static SPIRV-Tools-opt)
add_dependencies(tint-extract core_tables extinst_tables)
target_compile_definitions(tint-extract PRIVATE
  TINT_BUILD_SPV_READER=1
  TINT_BUILD_WGSL_WRITER=1
  TINT_BUILD_WGSL_READER=0
  TINT_BUILD_SPV_WRITER=0
  TINT_BUILD_MSL_WRITER=0
  TINT_BUILD_HLSL_WRITER=0
  TINT_BUILD_GLSL_WRITER=0
  TINT_BUILD_SYNTAX_TREE_WRITER=0)
target_compile_options(tint-extract PRIVATE /wd4267 /wd4244 /wd4996 /wd4005)

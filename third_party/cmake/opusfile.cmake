# ---------------------------------------------------------------------------
# libopusfile is the one Xiph library in this tree that ships no CMake build --
# upstream carries autotools only -- and the submodule under
# third_party/opusfile is kept as upstream's tree verbatim, so its build file
# lives here in the parent instead of inside the submodule.  It is pulled in
# with `include(cmake/opusfile.cmake)` from third_party/CMakeLists.txt.
#
# Only the core library is built.  The `opusurl` half of the project (http.c,
# wincerts.c) exists for streaming over HTTP, which the engine has no use for;
# miniaudio reaches the decoder through
# decoders/libopus/miniaudio_libopus.c, and that only needs op_open_* and
# op_read_*.
#
# The upstream build generates a config.h from configure.ac, but every macro it
# defines (OP_ENABLE_ASSERTIONS, OP_HAVE_CLOCK_GETTIME, OP_HAVE_LRINTF, ...) is
# only consulted through `#if defined(...)` in the sources and has a portable
# fallback, so the library is built here without one.  OP_DISABLE_FLOAT_API in
# particular must stay undefined: miniaudio decodes through op_read_float().
# ---------------------------------------------------------------------------

set(OPUSFILE_DIR "${CMAKE_CURRENT_LIST_DIR}/../opusfile")

add_library(opusfile STATIC
  "${OPUSFILE_DIR}/src/info.c"
  "${OPUSFILE_DIR}/src/internal.c"
  "${OPUSFILE_DIR}/src/opusfile.c"
  "${OPUSFILE_DIR}/src/stream.c"
)

target_include_directories(opusfile
  PUBLIC "${OPUSFILE_DIR}/include"
  PRIVATE "${OPUSFILE_DIR}/src"
)

target_link_libraries(opusfile PUBLIC Opus::opus Ogg::ogg)

if(MSVC)
  target_compile_definitions(opusfile PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

unset(OPUSFILE_DIR)

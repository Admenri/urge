cmake_minimum_required(VERSION 3.30)

# Dear ImGui (docking branch) with the SDL3 platform backend and the WebGPU
# renderer backend, as one static library.
#
# The renderer backend is written against the raw C WebGPU API and includes
# <webgpu/webgpu.h> plus, on the desktop only, <webgpu/wgpu.h>.  Which
# implementation sits behind that C API decides which of the backend's two
# code paths is right:
#
#   * native  -- wgpu-native, and `IMGUI_IMPL_WEBGPU_BACKEND_WGPU`.  Its
#                extension header lives in the wgpu-native tree as ffi/wgpu.h,
#                so it is re-rooted into a webgpu/ folder of our own for the
#                angle-bracket include to resolve.  ffi/wgpu.h includes
#                "webgpu.h" by a relative name, so that copy travels with it.
#
#   * web     -- emdawnwebgpu, and `IMGUI_IMPL_WEBGPU_BACKEND_DAWN`.  Upstream
#                requires the Dawn flavour on Emscripten (imgui_impl_wgpu.h
#                refuses to compile otherwise), and there is no extension
#                header at all: the port implements the standard API and
#                nothing else.
get_filename_component(_IMGUI_TP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
set(_IMGUI_WGPU_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/include")

if(NOT EMSCRIPTEN)
  set(_IMGUI_WGPU_SRC "${_IMGUI_TP_ROOT}/wgpu-cmake/wgpu-native/ffi")

  file(MAKE_DIRECTORY "${_IMGUI_WGPU_INCLUDE}/webgpu")
  file(COPY "${_IMGUI_WGPU_SRC}/wgpu.h"
            "${_IMGUI_WGPU_SRC}/webgpu-headers/webgpu.h"
       DESTINATION "${_IMGUI_WGPU_INCLUDE}/webgpu")
endif()

add_library(imgui-docking STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui_tables.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/imgui_widgets.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp"
)

# Headers and the backend selector travel to the users of the library: the
# engine's ImGuiManager includes imgui.h and the two imgui_impl_*.h directly.
target_include_directories(imgui-docking PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/backends>
  $<BUILD_INTERFACE:${_IMGUI_WGPU_INCLUDE}>
)

if(EMSCRIPTEN)
  target_compile_definitions(imgui-docking PUBLIC
    IMGUI_IMPL_WEBGPU_BACKEND_DAWN=1
  )
else()
  target_compile_definitions(imgui-docking PUBLIC
    IMGUI_IMPL_WEBGPU_BACKEND_WGPU=1
  )
endif()

target_link_libraries(imgui-docking PUBLIC SDL3::SDL3-static webgpu-cpp-header)

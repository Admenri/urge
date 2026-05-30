cmake_minimum_required(VERSION 3.22)

include(FetchContent)

FetchContent_Declare(
  Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG v0.6
)
FetchContent_MakeAvailable(Corrosion)

corrosion_import_crate(MANIFEST_PATH wgpu-native/Cargo.toml)

if(WIN32)
  set(OS_LIBRARIES d3dcompiler ws2_32 userenv bcrypt ntdll opengl32 Propsys RuntimeObject)
elseif(UNIX AND NOT APPLE)
  set(OS_LIBRARIES "-lm -ldl")
elseif(APPLE)
  set(OS_LIBRARIES "-framework Foundation -framework CoreFoundation -framework QuartzCore -framework Metal")
endif()

add_library(webgpu_wgpu INTERFACE)
target_link_libraries(webgpu_wgpu INTERFACE wgpu_native ${OS_LIBRARIES})

# The shim is part of urge-binding so Ruby initialization retains the existing
# entry point. Windows builds never compile or link these handlers.
target_sources(urge-binding PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/binding_win32api_shim.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_shim.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_memory.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_platform.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_process.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_profile.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_sdl.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_shim_macos.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/win32api_shim_wasm.cc)

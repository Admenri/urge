target_sources(${LIB_NAME}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismRenderer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismClippingManager.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CubismClippingManager.tpp
)

# Add specified rendering directory.
add_subdirectory(Diligent)

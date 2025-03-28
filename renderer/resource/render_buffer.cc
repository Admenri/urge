// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/resource/render_buffer.h"

#include <array>

namespace renderer {

// Primitive draw order:
//   Data: 6 index to 4 vertex
//   View:
//  0 +----+ 1
//    | \  |
//    |  \ |
//    |   \|
//  3 +----+ 2
// order: lt -> rt -> rb -> lb
static std::array<uint16_t, 6> kQuadrangleDrawIndices = {
    0, 1, 2, 2, 3, 0,
};

QuadIndexCache::QuadIndexCache(const wgpu::Device& device)
    : device_(device), format_(wgpu::IndexFormat::Uint16) {}

std::unique_ptr<QuadIndexCache> renderer::QuadIndexCache::Make(
    const wgpu::Device& device) {
  return std::unique_ptr<QuadIndexCache>(new QuadIndexCache(device));
}

wgpu::Buffer* QuadIndexCache::Allocate(uint32_t quadrangle_size) {
  uint32_t required_indices_size =
      quadrangle_size * kQuadrangleDrawIndices.size();

  // Generate
  if (cache_.size() < required_indices_size) {
    cache_.clear();
    cache_.reserve(required_indices_size);
    for (int32_t i = 0; i < quadrangle_size; ++i)
      for (const auto& it : kQuadrangleDrawIndices)
        cache_.push_back(i * 4 + it);

    // Reset old buffer
    index_buffer_ = nullptr;
  }

  // Upload to GPU
  if (!index_buffer_) {
    // Allocate bigger buffer
    wgpu::BufferDescriptor buffer_desc;
    buffer_desc.label = "IndexBuffer.Immutable.Quadrangle";
    buffer_desc.usage = wgpu::BufferUsage::Index;
    buffer_desc.mappedAtCreation = true;
    buffer_desc.size = required_indices_size *
                       sizeof(decltype(kQuadrangleDrawIndices)::value_type);
    index_buffer_ = device_.CreateBuffer(&buffer_desc);

    // Re-Write indices data to buffer
    void* dest_memory = index_buffer_.GetMappedRange();
    std::memcpy(dest_memory, cache_.data(), buffer_desc.size);
    index_buffer_.Unmap();
  }

  return &index_buffer_;
}

}  // namespace renderer

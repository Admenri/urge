// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/resource/render_buffer.h"

#include <array>
#include <cstring>

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
static std::array<QuadIndexCache::IndexFormat, 6> kQuadrangleDrawIndices = {
    0, 1, 2, 2, 3, 0,
};

QuadIndexCache::QuadIndexCache(Diligent::IRenderDevice* device)
    : device_(device) {}

void QuadIndexCache::Allocate(size_t quadrangle_size) {
  size_t required_indices_size =
      quadrangle_size * kQuadrangleDrawIndices.size();

  // Generate
  if (cache_.size() < required_indices_size) {
    cache_.clear();
    cache_.reserve(required_indices_size);
    for (size_t i = 0; i < quadrangle_size; ++i)
      for (const auto& it : kQuadrangleDrawIndices)
        cache_.push_back(static_cast<QuadIndexCache::IndexFormat>(i * 4 + it));

    // Reset old buffer
    buffer_.Release();
  }

  // Upload to GPU
  if (!buffer_) {
    // Allocate bigger buffer
    Diligent::BufferDesc buffer_desc;
    buffer_desc.Name = "quad.index.buffer";
    buffer_desc.Usage = Diligent::USAGE_IMMUTABLE;
    buffer_desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    buffer_desc.Size = required_indices_size *
                       sizeof(decltype(kQuadrangleDrawIndices)::value_type);

    // Upload new index data
    Diligent::BufferData buffer_data;
    buffer_data.pData = cache_.data();
    buffer_data.DataSize = buffer_desc.Size;

    // Create buffer object
    device_->CreateBuffer(buffer_desc, &buffer_data, &buffer_);
  }
}

}  // namespace renderer

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/resource/render_buffer.h"

#include <array>
#include <cstring>

namespace renderer {

constexpr size_t IndexSizeInByte(Diligent::VALUE_TYPE type) {
  switch (type) {
    default:
    case Diligent::VT_UINT16:
      return 2;
    case Diligent::VT_UINT32:
      return 4;
  }
}

// Primitive draw order:
//   Data: 6 index to 4 vertex
//   View:
//  0 +----+ 1
//    | \  |
//    |  \ |
//    |   \|
//  3 +----+ 2
// order: lt -> rt -> rb -> lb
static std::array<uint32_t, 6> kQuadrangleDrawIndices = {
    0, 1, 2, 2, 3, 0,
};

QuadIndexCache::QuadIndexCache(Diligent::IRenderDevice* device,
                               Diligent::VALUE_TYPE type)
    : type_(type), index_count_(0), device_(device) {}

void QuadIndexCache::Allocate(size_t quadrangle_size) {
  size_t required_indices = quadrangle_size * kQuadrangleDrawIndices.size();

  // Generate
  if (index_count_ < required_indices) {
    index_count_ = required_indices;
    cache_.resize(required_indices * IndexSizeInByte(type_));
    auto* index_ptr = cache_.data();
    for (size_t i = 0; i < quadrangle_size; ++i) {
      for (const auto& it : kQuadrangleDrawIndices) {
        // Align to maximum size
        *reinterpret_cast<uint32_t*>(index_ptr) =
            static_cast<uint32_t>(i * 4 + it);
        index_ptr += IndexSizeInByte(type_);
      }
    }

    // Reset buffer
    buffer_.Release();
  }

  // Upload to GPU
  if (!buffer_) {
    // Allocate bigger buffer
    Diligent::BufferDesc buffer_desc;
    buffer_desc.Name = "quad.index.buffer";
    buffer_desc.Usage = Diligent::USAGE_IMMUTABLE;
    buffer_desc.BindFlags = Diligent::BIND_INDEX_BUFFER;
    buffer_desc.Size = required_indices * IndexSizeInByte(type_);

    // Upload new index data
    Diligent::BufferData buffer_data;
    buffer_data.pData = cache_.data();
    buffer_data.DataSize = buffer_desc.Size;

    // Create buffer object
    device_->CreateBuffer(buffer_desc, &buffer_data, &buffer_);
  }
}

}  // namespace renderer

// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/resource/mesh.h"

#include <cstring>

#include "content/render/graphics.h"

namespace content {

Mesh::Mesh(uint32_t vertex_bytes, uint32_t index_count) {
  vertices_.assign(vertex_bytes, 0);
  indices_.assign(index_count, 0);
}

void Mesh::UpdateGPUBuffer(renderer::RenderDevice* gfx) {
  const auto& device = gfx->device();
  const auto& queue = gfx->queue();

  // --- Vertex buffer ---
  if (!vertices_.empty()) {
    size_t required = vertices_.size();

    if (vertex_buffer_ && vertex_buffer_.GetSize() >= required) {
      // Same or larger buffer: update in-place (no GPU allocation)
      queue.WriteBuffer(vertex_buffer_, 0, vertices_.data(), required);
    } else {
      // Buffer too small or doesn't exist: recreate
      wgpu::BufferDescriptor desc{};
      desc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
      desc.size = required;
      desc.mappedAtCreation = true;

      vertex_buffer_ = device.CreateBuffer(&desc);
      if (vertex_buffer_) {
        void* mapped = vertex_buffer_.GetMappedRange(0, WGPU_WHOLE_MAP_SIZE);
        if (mapped)
          std::memcpy(mapped, vertices_.data(), required);
        vertex_buffer_.Unmap();
      }
    }
  }

  // --- Index buffer ---
  if (!indices_.empty()) {
    size_t required = indices_.size() * sizeof(uint32_t);

    if (index_buffer_ && index_buffer_.GetSize() >= required) {
      queue.WriteBuffer(index_buffer_, 0, indices_.data(), required);
    } else {
      wgpu::BufferDescriptor desc{};
      desc.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
      desc.size = required;
      desc.mappedAtCreation = true;

      index_buffer_ = device.CreateBuffer(&desc);
      if (index_buffer_) {
        void* mapped = index_buffer_.GetMappedRange(0, WGPU_WHOLE_MAP_SIZE);
        if (mapped)
          std::memcpy(mapped, indices_.data(), required);
        index_buffer_.Unmap();
      }
    }
  }
}

scoped_refptr<Mesh> Mesh::New(uint32_t vertex_bytes,
                              uint32_t index_count,
                              URGE_EXCEPTION) {
  return Object::Create<Mesh>(vertex_bytes, index_count);
}

epointer Mesh::GetVertices(URGE_EXCEPTION) {
  return vertices_.data();
}

uint32_t Mesh::GetVertexBytesSize(URGE_EXCEPTION) {
  return vertices_.size();
}

epointer Mesh::GetIndices(URGE_EXCEPTION) {
  return indices_.data();
}

uint32_t Mesh::GetIndexCount(URGE_EXCEPTION) {
  return indices_.size();
}

void Mesh::SetupSubMeshData(earray<scoped_refptr<SubMesh>> data,
                            URGE_EXCEPTION) {
  mesh_groups_ = data;
}

earray<scoped_refptr<SubMesh>> Mesh::GetSubMeshes(URGE_EXCEPTION) {
  return mesh_groups_;
}

}  // namespace content

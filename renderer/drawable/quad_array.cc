// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/drawable/quad_array.h"

namespace renderer {

QuadArray::QuadArray(RefCntAutoPtr<IRenderDevice> device,
                     scoped_refptr<QuadArrayIndices> indices)
    : device_(device), indices_(indices), quad_size_(0), buffer_size_(0) {}

void QuadArray::Clear() {
  quad_size_ = 0;
  vertices_.clear();
}

void QuadArray::Resize(RefCntAutoPtr<IDeviceContext> context, size_t size) {
  quad_size_ = size;
  vertices_.resize(size * 4);

  indices_->EnsureSize(context, quad_size_);

  size_t update_buffer_size = quad_size_ * sizeof(VertexType) * 4;
  if (update_buffer_size && update_buffer_size > buffer_size_) {
    buffer_size_ = update_buffer_size;

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "Quad array vertex buffer";
    VertBuffDesc.Usage = USAGE_DYNAMIC;
    VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = buffer_size_;

    buffer_.Release();
    device_->CreateBuffer(VertBuffDesc, nullptr, &buffer_);
  }
}

void QuadArray::Draw(RefCntAutoPtr<IDeviceContext> context,
                     size_t offset,
                     size_t count) {
  if (!quad_size_)
    return;

  VertexType* vertices = nullptr;
  context->MapBuffer(buffer_, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)vertices);
  memcpy(vertices, vertices_.data(), buffer_size_);
  context->UnmapBuffer(buffer_, MAP_WRITE);

  context->SetVertexBuffers(0, 1, &buffer_, nullptr,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->SetIndexBuffer(indices_->GetBufferHandle(), 0,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  DrawIndexedAttribs drawAttrs;
  drawAttrs.FirstIndexLocation = offset * 6;
  drawAttrs.NumIndices = count * 6;
  drawAttrs.IndexType = VT_UINT16;
  drawAttrs.Flags = DRAW_FLAG_NONE;
  context->DrawIndexed(drawAttrs);
}

}  // namespace renderer

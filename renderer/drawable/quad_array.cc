// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/drawable/quad_array.h"

namespace renderer {

QuadArray::QuadArray(RefCntAutoPtr<IRenderDevice> device,
                     scoped_refptr<QuadArrayIndices> indices)
    : device_(device), indices_(indices), quad_size_(0), buffer_size_(0) {}

void QuadArray::Resize(size_t size) {
  if (quad_size_ == size)
    return;

  vertices_.resize(size * 4);
  quad_size_ = size;
}

void QuadArray::Clear() {
  if (!quad_size_)
    return;

  vertices_.clear();
  quad_size_ = 0;
}

void QuadArray::Update(RefCntAutoPtr<IDeviceContext> context) {
  if (!quad_size_)
    return;

  size_t update_buffer_size = quad_size_ * sizeof(VertexType) * 4;
  if (update_buffer_size > buffer_size_) {
    buffer_size_ = update_buffer_size;

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name = "Quad array vertex buffer";
    VertBuffDesc.Usage = USAGE_DYNAMIC;
    VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size = buffer_size_;
    device_->CreateBuffer(VertBuffDesc, nullptr, &buffer_);
  }

  {
    VertexType* vertices = nullptr;
    context->MapBuffer(buffer_, MAP_WRITE, MAP_FLAG_DISCARD, (PVoid&)vertices);
    memcpy(vertices, vertices_.data(), update_buffer_size);
    context->UnmapBuffer(buffer_, MAP_WRITE);
  }

  indices_->EnsureSize(context, quad_size_);
}

void QuadArray::Draw(RefCntAutoPtr<IDeviceContext> context,
                     size_t offset,
                     size_t count) {
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

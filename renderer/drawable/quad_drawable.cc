// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/drawable/quad_drawable.h"

#include <array>

static const std::array<uint16_t, 6> kQuadIndexTemplate = {0, 1, 2, 2, 3, 0};

namespace renderer {

QuadArrayIndices::QuadArrayIndices(RefCntAutoPtr<IRenderDevice> device)
    : device_(device) {}

void QuadArrayIndices::EnsureSize(RefCntAutoPtr<IDeviceContext> context,
                                  size_t count) {
  if (buffer_.size() >= count * 6)
    return;

  size_t begin = buffer_.size() / 6;
  buffer_.reserve(count * 6);
  for (size_t i = begin; i < count; ++i)
    for (size_t j = 0; j < 6; ++j)
      buffer_.push_back(static_cast<uint16_t>(i * 4 + kQuadIndexTemplate[j]));

  BufferDesc VertBuffDesc;
  VertBuffDesc.Name = "Quad index buffer";
  VertBuffDesc.Usage = USAGE_IMMUTABLE;
  VertBuffDesc.BindFlags = BIND_INDEX_BUFFER;
  VertBuffDesc.Size = buffer_.size() * sizeof(uint16_t);

  BufferData IBData;
  IBData.pData = buffer_.data();
  IBData.DataSize = buffer_.size() * sizeof(uint16_t);
  IBData.pContext = context;

  device_->CreateBuffer(VertBuffDesc, &IBData, &handle_);
}

QuadDrawable::QuadDrawable(RefCntAutoPtr<IRenderDevice> device,
                           scoped_refptr<QuadArrayIndices> indices)
    : raw_data_(), indices_(indices), need_update_(false) {
  BufferDesc VertBuffDesc;
  VertBuffDesc.Name = "Single quad vertex buffer";
  VertBuffDesc.Usage = USAGE_DYNAMIC;
  VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
  VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
  VertBuffDesc.Size = sizeof(VertexInput::Data) * 4;

  device->CreateBuffer(VertBuffDesc, nullptr, &buffer_handle_);
}

void QuadDrawable::SetPosition(const base::RectF& pos) {
  VertexInput::SetPosition(raw_data_, pos);
  need_update_ = true;
}

void QuadDrawable::SetTexcoord(const base::RectF& tex) {
  VertexInput::SetTexcoord(raw_data_, tex);
  need_update_ = true;
}

void QuadDrawable::SetColor(const base::Vec4& color, int index) {
  VertexInput::SetColor(raw_data_, color, index);
  need_update_ = true;
}

void QuadDrawable::Draw(RefCntAutoPtr<IDeviceContext> context) {
  if (need_update_) {
    VertexInput::Data* vertices = nullptr;
    context->MapBuffer(buffer_handle_, MAP_WRITE, MAP_FLAG_DISCARD,
                       (PVoid&)vertices);
    memcpy(vertices, raw_data_, sizeof(VertexInput::Data) * 4);
    context->UnmapBuffer(buffer_handle_, MAP_WRITE);

    need_update_ = false;
  }

  context->SetIndexBuffer(indices_->GetBufferHandle(), 0,
                          RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->SetVertexBuffers(0, 1, &buffer_handle_, nullptr,
                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  DrawIndexedAttribs drawAttrs;
  drawAttrs.NumIndices = 6;
  drawAttrs.IndexType = VT_UINT16;
  drawAttrs.Flags = DRAW_FLAG_NONE;
  context->DrawIndexed(drawAttrs);
}

}  // namespace renderer

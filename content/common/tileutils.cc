// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/tileutils.h"

#include "renderer/device/render_device.h"

namespace content {

TilemapFlashLayer::TilemapFlashLayer(
    int tile_size,
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
    scoped_refptr<renderer::QuadArrayIndices> indices)
    : quads_(std::make_unique<renderer::QuadArray>(device, indices)),
      tile_size_(tile_size) {}

void TilemapFlashLayer::BeforeComposite(
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context) {
  if (buffer_need_update_) {
    UpdateBuffer(context);
    buffer_need_update_ = false;
  }
}

void TilemapFlashLayer::Composite(CompositeTargetInfo* target_info,
                                  GraphicsHost* screen,
                                  const base::Vec2i offset,
                                  float alpha) {
  auto& shader = screen->renderer()->GetPipelines()->color;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::Addition);
  screen->renderer()->context()->SetPipelineState(pipeline->pso);
  screen->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Color::VSUniform> Constants(
        screen->renderer()->context(), shader.GetVSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, target_info->viewport_size,
        screen->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->transOffset = offset;
  }

  for (auto& v : quads_->vertices())
    v.color.w = alpha;

  quads_->Draw(screen->renderer()->context());
}

bool TilemapFlashLayer::SampleFlashColor(base::Vec4& out, int x, int y) const {
  int16_t packed = TableGetWrapped(flashdata_, x, y);

  if (!packed)
    return false;

  const float max = 0xF;

  float b = ((packed & 0x000F) >> 0) / max;
  float g = ((packed & 0x00F0) >> 4) / max;
  float r = ((packed & 0x0F00) >> 8) / max;

  out = base::Vec4(r, g, b, 1);

  return true;
}

void TilemapFlashLayer::UpdateBuffer(
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context) {
  if (!flashdata_)
    return;

  std::vector<renderer::GeometryVertexLayout::Data> vertices;
  for (int x = 0; x < flash_viewport_.width; ++x) {
    for (int y = 0; y < flash_viewport_.height; ++y) {
      base::Vec4 color;

      if (!SampleFlashColor(color, x + flash_viewport_.x,
                            y + flash_viewport_.y))
        continue;

      base::Rect posRect(x * tile_size_, y * tile_size_, tile_size_,
                         tile_size_);

      renderer::GeometryVertexLayout::Data v[4];
      renderer::GeometryVertexLayout::SetPosition(v, posRect);
      renderer::GeometryVertexLayout::SetColor(v, color);

      for (size_t i = 0; i < 4; ++i)
        vertices.push_back(v[i]);
    }
  }

  if (vertices.empty())
    return;

  size_t quad_size = vertices.size() / 4;
  quads_->Resize(context, quad_size);
  memcpy(quads_->vertices().data(), vertices.data(),
         vertices.size() * sizeof(renderer::GeometryVertexLayout::Data));
}

}  // namespace content

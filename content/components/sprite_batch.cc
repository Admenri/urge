// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/components/sprite_batch.h"

namespace content {

void SpriteQuad::SetPositionRect(SpriteQuad* data, const base::RectF& pos) {
  int i = 0;
  data->vertices[i++].position = base::Vec4(pos.x, pos.y, 0, 1);
  data->vertices[i++].position = base::Vec4(pos.x + pos.width, pos.y, 0, 1);
  data->vertices[i++].position =
      base::Vec4(pos.x + pos.width, pos.y + pos.height, 0, 1);
  data->vertices[i++].position = base::Vec4(pos.x, pos.y + pos.height, 0, 1);
}

void SpriteQuad::SetTexCoordRect(SpriteQuad* data,
                                 const base::RectF& texcoord) {
  int i = 0;
  data->vertices[i++].texcoord = base::Vec2(texcoord.x, texcoord.y);
  data->vertices[i++].texcoord =
      base::Vec2(texcoord.x + texcoord.width, texcoord.y);
  data->vertices[i++].texcoord =
      base::Vec2(texcoord.x + texcoord.width, texcoord.y + texcoord.height);
  data->vertices[i++].texcoord =
      base::Vec2(texcoord.x, texcoord.y + texcoord.height);
}

SpriteBatch::SpriteBatch(renderer::RenderDevice* device,
                         std::unique_ptr<renderer::QuadBatch> vertex_batch,
                         std::unique_ptr<SpriteQuadBuffer> quad_batch,
                         std::unique_ptr<SpriteBatchBuffer> uniform_batch)
    : device_(device),
      current_texture_(nullptr),
      last_batch_index_(-1),
      vertex_batch_(std::move(vertex_batch)),
      quad_batch_(std::move(quad_batch)),
      uniform_batch_(std::move(uniform_batch)) {}

SpriteBatch::~SpriteBatch() {}

std::unique_ptr<SpriteBatch> SpriteBatch::Make(renderer::RenderDevice* device) {
  auto vertex_batch = renderer::QuadBatch::Make(**device, 1);
  auto quad_batch = SpriteQuadBuffer::Make(**device);
  auto uniform_batch = SpriteBatchBuffer::Make(**device);

  return std::unique_ptr<SpriteBatch>(
      new SpriteBatch(device, std::move(vertex_batch), std::move(quad_batch),
                      std::move(uniform_batch)));
}

void SpriteBatch::BeginBatch(TextureAgent* texture) {
  current_texture_ = texture;
  last_batch_index_ = uniform_cache_.size();
}

void SpriteBatch::PushSprite(const SpriteQuad& quad,
                             const renderer::SpriteUniform& uniform) {
  quad_cache_.push_back(quad);
  uniform_cache_.push_back(uniform);
}

void SpriteBatch::EndBatch(uint32_t* instance_offset,
                           uint32_t* instance_count) {
  const int32_t draw_count = uniform_cache_.size() - last_batch_index_;

  *instance_offset = last_batch_index_;
  *instance_count = draw_count;

  current_texture_ = nullptr;
  last_batch_index_ = -1;
}

void SpriteBatch::SubmitBatchDataAndResetCache(wgpu::CommandEncoder* encoder) {
  // Setup index buffer
  device_->GetQuadIndex()->Allocate(uniform_cache_.size());

  // Upload data and rebuild binding
  bool update_binding_group = false;
  if (quad_cache_.size())
    update_binding_group |= quad_batch_->QueueWrite(
        *encoder, quad_cache_.data(), quad_cache_.size());
  if (uniform_cache_.size())
    update_binding_group |= uniform_batch_->QueueWrite(
        *encoder, uniform_cache_.data(), uniform_cache_.size());

  if (update_binding_group)
    uniform_binding_ = renderer::SpriteUniform::CreateGroup(
        **device_, **quad_batch_, **uniform_batch_);

  // Reset cache
  quad_cache_.clear();
  uniform_cache_.clear();
}

}  // namespace content

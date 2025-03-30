// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/components/sprite_batch.h"

namespace content {

SpriteBatch::SpriteBatch(renderer::RenderDevice* device,
                         std::unique_ptr<renderer::QuadBatch> quads_batch,
                         std::unique_ptr<SpriteBatchBuffer> uniform_batch,
                         std::unique_ptr<DrawIndexedBatchBuffer> indirect_batch)
    : device_(device),
      current_texture_(nullptr),
      last_batch_index_(-1),
      last_instance_index_(0),
      quads_batch_(std::move(quads_batch)),
      uniform_batch_(std::move(uniform_batch)),
      indirect_batch_(std::move(indirect_batch)) {}

SpriteBatch::~SpriteBatch() {}

std::unique_ptr<SpriteBatch> SpriteBatch::Make(renderer::RenderDevice* device) {
  auto quads_batch = renderer::QuadBatch::Make(**device);
  auto uniform_batch = SpriteBatchBuffer::Make(**device);
  auto indirect_batch = DrawIndexedBatchBuffer::Make(**device);

  return std::unique_ptr<SpriteBatch>(
      new SpriteBatch(device, std::move(quads_batch), std::move(uniform_batch),
                      std::move(indirect_batch)));
}

void SpriteBatch::BeginBatch(TextureAgent* texture) {
  current_texture_ = texture;
  last_batch_index_ = quads_cache_.size();
}

void SpriteBatch::PushSprite(const renderer::Quad& quad,
                             const renderer::SpriteUniform& uniform) {
  quads_cache_.push_back(quad);
  uniform_cache_.push_back(uniform);

  DrawIndexedIndirectInfo draw_info;
  draw_info.index_count = 6;
  draw_info.instance_count = 1;
  draw_info.first_index = last_instance_index_ * 6;
  draw_info.first_instance = last_instance_index_;
  indirect_cache_.push_back(draw_info);

  last_instance_index_++;
}

void SpriteBatch::EndBatch(int64_t* indirect_offset, uint32_t* indirect_count) {
  const int32_t draw_count = quads_cache_.size() - last_batch_index_;

  *indirect_offset = last_batch_index_ * sizeof(DrawIndexedIndirectInfo);
  *indirect_count = last_instance_index_ - last_batch_index_;

  current_texture_ = nullptr;
  last_batch_index_ = -1;
}

void SpriteBatch::SubmitBatchDataAndResetCache(wgpu::CommandEncoder* encoder) {
  // Setup index buffer
  device_->GetQuadIndex()->Allocate(quads_cache_.size());

  // Upload data and rebuild binding
  if (quads_cache_.size())
    quads_batch_->QueueWrite(*encoder, quads_cache_.data(),
                             quads_cache_.size());
  if (uniform_cache_.size())
    if (uniform_batch_->QueueWrite(*encoder, uniform_cache_.data(),
                                   uniform_cache_.size()))
      uniform_binding_ = renderer::SpriteUniform::CreateInstanceGroup(
          **device_, **uniform_batch_);
  if (indirect_cache_.size())
    indirect_batch_->QueueWrite(*encoder, indirect_cache_.data(),
                                indirect_cache_.size());

  // Reset cache
  quads_cache_.clear();
  uniform_cache_.clear();
  indirect_cache_.clear();
  last_instance_index_ = 0;
}

}  // namespace content

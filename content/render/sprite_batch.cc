// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/sprite_batch.h"

namespace content {

SpriteBatch::SpriteBatch(renderer::RenderDevice* device)
    : device_(device),
      current_texture_(nullptr),
      last_batch_index_(-1),
      binding_(device->GetPipelines()->sprite.CreateBinding()),
      vertex_batch_(renderer::QuadBatch::Make(**device)),
      uniform_batch_(SpriteBatchBuffer::Make(**device)),
      indirect_batch_(renderer::IndexedIndirectBatch::Make(**device)) {}

SpriteBatch::~SpriteBatch() = default;

void SpriteBatch::BeginBatch(TextureAgent* texture) {
  current_texture_ = texture;
  last_batch_index_ = uniform_cache_.size();
}

void SpriteBatch::PushSprite(const renderer::Quad& quad,
                             const renderer::Binding_Sprite::Params& uniform) {
  indirect_cache_.emplace_back(6, 1, 6 * quad_cache_.size(), 0,
                               quad_cache_.size());

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

void SpriteBatch::SubmitBatchDataAndResetCache(
    renderer::RenderDevice* device,
    renderer::RenderContext* context) {
  // Setup index buffer
  device_->GetQuadIndex()->Allocate(uniform_cache_.size());

  // Upload data and rebuild binding
  if (quad_cache_.size())
    vertex_batch_->QueueWrite(**context, quad_cache_.data(),
                              quad_cache_.size());

  if (uniform_cache_.size())
    uniform_batch_->QueueWrite(**context, uniform_cache_.data(),
                               uniform_cache_.size());

  if (indirect_cache_.size())
    indirect_batch_->QueueWrite(**context, indirect_cache_.data(),
                                indirect_cache_.size());

  // Reset cache
  quad_cache_.clear();
  uniform_cache_.clear();
  indirect_cache_.clear();
}

}  // namespace content

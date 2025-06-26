// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/sprite_batch.h"

namespace content {

SpriteBatch::SpriteBatch(renderer::RenderDevice* device)
    : device_(device),
      current_texture_(nullptr),
      last_batch_index_(-1),
      support_storage_buffer_batch_(
          device->GetPipelines()->sprite.storage_buffer_support),
      binding_(device->GetPipelines()->sprite.CreateBinding()),
      vertex_batch_(renderer::QuadBatch::Make(**device)),
      uniform_batch_(SpriteBatchBuffer::Make(**device)),
      instance_batch_(SpriteInstanceBatchBuffer::Make(**device)) {}

SpriteBatch::~SpriteBatch() = default;

void SpriteBatch::BeginBatch(BitmapAgent* texture) {
  current_texture_ = texture;
  last_batch_index_ = uniform_cache_.size();
}

void SpriteBatch::PushSprite(const renderer::Quad& quad,
                             const renderer::Binding_Sprite::Params& uniform) {
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
    Diligent::IDeviceContext* render_context) {
  // Setup index buffer
  device_->GetQuadIndex()->Allocate(uniform_cache_.size());

  // Upload data and rebuild binding
  if (quad_cache_.size())
    vertex_batch_.QueueWrite(render_context, quad_cache_.data(),
                             quad_cache_.size());

  if (uniform_cache_.size()) {
    if (support_storage_buffer_batch_) {
      uniform_batch_.QueueWrite(render_context, uniform_cache_.data(),
                                uniform_cache_.size());
      uniform_binding_ =
          (*uniform_batch_)
              ->GetDefaultView(Diligent::BUFFER_VIEW_SHADER_RESOURCE);
    } else {
      instance_batch_.QueueWrite(render_context, uniform_cache_.data(),
                                 uniform_cache_.size());
    }
  }

  // Reset cache
  quad_cache_.clear();
  uniform_cache_.clear();
}

}  // namespace content

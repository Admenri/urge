// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_SPRITE_BATCH_H_
#define CONTENT_RENDER_SPRITE_BATCH_H_

#include <vector>

#include "content/canvas/canvas_impl.h"
#include "renderer/device/render_device.h"
#include "renderer/pipeline/render_binding.h"

namespace content {

using SpriteBatchBuffer =
    renderer::BatchBuffer<renderer::Binding_Sprite::Params,
                          Diligent::BIND_VERTEX_BUFFER,
                          Diligent::BUFFER_MODE_UNDEFINED,
                          Diligent::CPU_ACCESS_NONE,
                          Diligent::USAGE_DEFAULT>;

class SpriteBatch {
 public:
  SpriteBatch(renderer::RenderDevice* device);
  ~SpriteBatch();

  SpriteBatch(const SpriteBatch&) = delete;
  SpriteBatch& operator=(const SpriteBatch&) = delete;

  TextureAgent* GetCurrentTexture() const { return current_texture_; }

  renderer::Binding_Sprite* GetShaderBinding() { return binding_.get(); }
  Diligent::IBuffer* GetVertexBuffer() { return **vertex_batch_; }
  Diligent::IBuffer* GetInstanceBuffer() { return **uniform_batch_; }
  Diligent::IBuffer* GetIndirectArgsBuffer() { return **indirect_batch_; }

  // Setup a sprite batch
  void BeginBatch(TextureAgent* texture);

  void PushSprite(const renderer::Quad& quad,
                  const renderer::Binding_Sprite::Params& uniform);

  void EndBatch(uint32_t* instance_offset, uint32_t* instance_count);

  // Summit pending batch data to rendering queue
  void SubmitBatchDataAndResetCache(renderer::RenderDevice* device,
                                    renderer::RenderContext* context);

 private:
  renderer::RenderDevice* device_;
  TextureAgent* current_texture_;
  int32_t last_batch_index_;

  base::Vector<renderer::Quad> quad_cache_;
  base::Vector<renderer::Binding_Sprite::Params> uniform_cache_;
  base::Vector<renderer::IndexedIndirectParams> indirect_cache_;

  base::OwnedPtr<renderer::Binding_Sprite> binding_;
  base::OwnedPtr<renderer::QuadBatch> vertex_batch_;
  base::OwnedPtr<SpriteBatchBuffer> uniform_batch_;
  base::OwnedPtr<renderer::IndexedIndirectBatch> indirect_batch_;
};

}  // namespace content

#endif  //! CONTENT_RENDER_SPRITE_BATCH_H_

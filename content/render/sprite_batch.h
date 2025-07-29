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
                          Diligent::BIND_SHADER_RESOURCE,
                          Diligent::BUFFER_MODE_STRUCTURED,
                          Diligent::CPU_ACCESS_WRITE,
                          Diligent::USAGE_DYNAMIC>;

class SpriteBatch {
 public:
  SpriteBatch(renderer::RenderDevice* device);
  ~SpriteBatch();

  SpriteBatch(const SpriteBatch&) = delete;
  SpriteBatch& operator=(const SpriteBatch&) = delete;

  BitmapAgent* GetCurrentTexture() const { return current_texture_; }

  renderer::Binding_Sprite& GetShaderBinding() { return binding_; }
  Diligent::IBuffer* GetVertexBuffer() { return *vertex_batch_; }
  Diligent::IBufferView* GetUniformBinding() { return uniform_binding_; }

  // Setup a sprite batch
  void BeginBatch(BitmapAgent* texture);

  void PushSprite(const renderer::Quad& quad,
                  const renderer::Binding_Sprite::Params& uniform);

  void EndBatch(uint32_t* instance_offset, uint32_t* instance_count);

  // Summit pending batch data to rendering queue
  void SubmitBatchDataAndResetCache(Diligent::IDeviceContext* render_context);

  bool IsBatchEnabled() const { return support_storage_buffer_batch_; }

 private:
  renderer::RenderDevice* device_;
  BitmapAgent* current_texture_;
  int32_t last_batch_index_;
  const bool support_storage_buffer_batch_;

  std::vector<renderer::Quad> quad_cache_;
  std::vector<renderer::Binding_Sprite::Params> uniform_cache_;

  renderer::Binding_Sprite binding_;
  renderer::QuadBatch vertex_batch_;
  SpriteBatchBuffer uniform_batch_;
  RRefPtr<Diligent::IBufferView> uniform_binding_;
};

}  // namespace content

#endif  //! CONTENT_RENDER_SPRITE_BATCH_H_

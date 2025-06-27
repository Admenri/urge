// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_CANVAS_SCHEDULER_H_
#define CONTENT_CANVAS_CANVAS_SCHEDULER_H_

#include "base/worker/thread_worker.h"
#include "content/canvas/canvas_impl.h"
#include "content/context/execution_context.h"
#include "renderer/device/render_device.h"
#include "renderer/resource/render_buffer.h"

namespace content {

class CanvasScheduler {
 public:
  // All bitmap/canvas draw command will be encoded on this worker.
  CanvasScheduler(renderer::RenderDevice* render_device,
                  Diligent::IDeviceContext* primary_context);
  ~CanvasScheduler();

  CanvasScheduler(const CanvasScheduler&) = delete;
  CanvasScheduler& operator=(const CanvasScheduler&) = delete;

  // Sync all pending command to device queue,
  // clear children canvas command queue.
  void SubmitPendingPaintCommands();

  // Setup canvas render target
  // Cached render state
  void SetupRenderTarget(Diligent::ITextureView* render_target,
                         Diligent::ITextureView* depth_stencil,
                         bool clear_target);

  // Create or get generic blt intermediate cache texture.
  Diligent::ITexture* RequireBltCacheTexture(const base::Vec2i& size);

  // Get rendering context for canvas opeartion.
  // The operations of all canvas are regarded as discrete draw commands.
  renderer::RenderDevice* GetRenderDevice();
  Diligent::IDeviceContext* GetDiscreteRenderContext();

  renderer::QuadBatch& quad_batch() { return common_quad_batch_; }
  renderer::Binding_Base& base_binding() { return generic_base_binding_; }
  renderer::Binding_Color& color_binding() { return generic_color_binding_; }
  renderer::Binding_BitmapBlt& blt_binding() { return generic_blt_binding_; }
  renderer::Binding_BitmapFilter& hue_binding() { return generic_hue_binding_; }

 private:
  friend class CanvasImpl;

  base::LinkedList<CanvasImpl> children_;

  renderer::RenderDevice* device_;
  Diligent::IDeviceContext* context_;

  renderer::Binding_Base generic_base_binding_;
  renderer::Binding_Color generic_color_binding_;
  renderer::Binding_BitmapBlt generic_blt_binding_;
  renderer::Binding_BitmapFilter generic_hue_binding_;

  renderer::QuadBatch common_quad_batch_;
  RRefPtr<Diligent::ITexture> generic_blt_texture_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_CANVAS_SCHEDULER_H_

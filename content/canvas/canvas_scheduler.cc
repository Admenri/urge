// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/canvas_scheduler.h"

#include "renderer/utils/texture_utils.h"

namespace content {

CanvasScheduler::CanvasScheduler(renderer::RenderDevice* render_device,
                                 Diligent::IDeviceContext* primary_context,
                                 renderer::PipelineSet* loader)
    : device_(render_device),
      context_(primary_context),
      common_quad_batch_(renderer::QuadBatch::Make(**device_)) {
  // Create initial blt cache
  renderer::CreateTexture2D(**device_, &generic_blt_texture_,
                            "canvas.generic.blt.cache", base::Vec2i(2 << 8));
}

CanvasScheduler::~CanvasScheduler() = default;

void CanvasScheduler::SubmitPendingPaintCommands() {
  for (auto it = children_.head(); it != children_.end(); it = it->next()) {
    // Submit all pending commands (except Blt, StretchBlt)
    it->value()->SubmitQueuedCommands();
  }
}

void CanvasScheduler::SetupRenderTarget(Diligent::ITextureView* render_target,
                                        Diligent::ITextureView* depth_stencil,
                                        bool clear_target) {
  // Setup new render target
  if (render_target) {
    context_->SetRenderTargets(
        1, &render_target, depth_stencil,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply clear buffer if need
    if (clear_target) {
      float clear_color[] = {0, 0, 0, 0};
      context_->ClearRenderTarget(
          render_target, clear_color,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

      if (depth_stencil) {
        // Clear depth buffer if need
        context_->ClearDepthStencil(
            depth_stencil, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
            Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
      }
    }

    return;
  }

  // Reset render target state
  context_->SetRenderTargets(
      0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

Diligent::ITexture* CanvasScheduler::RequireBltCacheTexture(
    const base::Vec2i& size) {
  if (static_cast<int32_t>(generic_blt_texture_->GetDesc().Width) < size.x ||
      static_cast<int32_t>(generic_blt_texture_->GetDesc().Height) < size.y) {
    base::Vec2i new_size;
    new_size.x =
        std::max<int32_t>(size.x, generic_blt_texture_->GetDesc().Width);
    new_size.y =
        std::max<int32_t>(size.y, generic_blt_texture_->GetDesc().Height);

    generic_blt_texture_.Release();
    renderer::CreateTexture2D(**device_, &generic_blt_texture_,
                              "canvas.generic.blt.cache", new_size);
  }

  return generic_blt_texture_;
}

renderer::RenderDevice* CanvasScheduler::GetRenderDevice() {
  return device_;
}

Diligent::IDeviceContext* CanvasScheduler::GetDiscreteRenderContext() {
  return context_;
}

}  // namespace content

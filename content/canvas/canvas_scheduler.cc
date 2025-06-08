// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/canvas_scheduler.h"

namespace content {

CanvasScheduler::CanvasScheduler(renderer::RenderDevice* render_device,
                                 renderer::RenderContext* primary_context)
    : device_(render_device),
      context_(primary_context),
      generic_base_binding_(device_->GetPipelines()->base.CreateBinding()),
      generic_color_binding_(device_->GetPipelines()->color.CreateBinding()),
      generic_hue_binding_(device_->GetPipelines()->bitmaphue.CreateBinding()),
      common_quad_batch_(renderer::QuadBatch::Make(**device_)) {}

CanvasScheduler::~CanvasScheduler() = default;

void CanvasScheduler::SubmitPendingPaintCommands() {
  for (auto it = children_.head(); it != children_.end(); it = it->next()) {
    // Submit all pending commands (except Blt, StretchBlt)
    it->value()->SubmitQueuedCommands();
  }
}

void CanvasScheduler::SetupRenderTarget(Diligent::ITextureView* render_target,
                                        bool clear_target) {
  auto* context = **context_;

  // Setup new render target
  if (render_target) {
    context->SetRenderTargets(
        1, &render_target, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply clear buffer if need
    if (clear_target) {
      float clear_color[] = {0, 0, 0, 0};
      context->ClearRenderTarget(
          render_target, clear_color,
          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    return;
  }

  // Reset render target state
  context->SetRenderTargets(
      0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

renderer::RenderDevice* CanvasScheduler::GetRenderDevice() {
  return device_;
}

renderer::RenderContext* CanvasScheduler::GetDiscreteRenderContext() {
  return context_;
}

}  // namespace content

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/context/render_context.h"

namespace renderer {

RenderContext::RenderContext(
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context)
    : context_(context),
      scissor_(ScissorController::Create(context)),
      current_render_targets_{0},
      current_depth_stencil_(nullptr) {}

RenderContext::~RenderContext() = default;

void RenderContext::SetRenderTargets(
    uint32_t num_targets,
    Diligent::ITextureView** render_targets,
    Diligent::ITextureView* depth_stencil,
    Diligent::RESOURCE_STATE_TRANSITION_MODE mode) {
  context_->SetRenderTargets(num_targets, render_targets, depth_stencil, mode);

  std::memcpy(current_render_targets_, render_targets,
              sizeof(void*) * num_targets);
  current_depth_stencil_ = depth_stencil;
}

void RenderContext::GetRenderTargets(uint32_t num_targets,
                                     Diligent::ITextureView** render_targets,
                                     Diligent::ITextureView** depth_stencil) {
  if (render_targets && num_targets)
    std::memcpy(render_targets, current_render_targets_,
                sizeof(void*) * num_targets);

  if (depth_stencil)
    *depth_stencil = current_depth_stencil_;
}

}  // namespace renderer

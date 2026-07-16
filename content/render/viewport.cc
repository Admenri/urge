// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/viewport.h"

namespace content {

///
/// RenderContext
///

RenderContext::RenderContext(World* world,
                             scoped_refptr<GPUTextureView> rtv,
                             scoped_refptr<GPUTextureView> dsv)
    : world_(world), render_target_view_(rtv), depth_stencil_view_(dsv) {}

scoped_refptr<GPUQueue> RenderContext::GetQueue(URGE_EXCEPTION) {
  return nullptr;
}

scoped_refptr<GPUTextureView> RenderContext::GetRenderTargetView(
    URGE_EXCEPTION) {
  return render_target_view_;
}

scoped_refptr<GPUTextureView> RenderContext::GetDepthStencilView(
    URGE_EXCEPTION) {
  return depth_stencil_view_;
}

scoped_refptr<CullingResults> RenderContext::Cull(scoped_refptr<Camera> camera,
                                                  URGE_EXCEPTION) {
  return nullptr;
}

void RenderContext::DrawRenderers(
    scoped_refptr<GPURenderPassEncoder> pass,
    scoped_refptr<CullingResults> culling_results,
    scoped_refptr<DrawingSettings> drawing_settings,
    scoped_refptr<FilteringSettings> filtering_settings,
    URGE_EXCEPTION) {}

///
/// Viewport
///

// static
scoped_refptr<Viewport> Viewport::New(URGE_EXCEPTION) {
  return Object::Create<Viewport>();
}

Viewport::Viewport() {}

URGE_ATTRIBUTE_DEFINE(
    Viewport,
    World,
    scoped_refptr<World>,
    { return world_; },
    { world_ = value; });

URGE_BINDING()
void Viewport::SetupRenderProcess(RenderCallback callback, URGE_EXCEPTION) {
  render_process_ = callback;
}

void Viewport::Render(scoped_refptr<GPUTextureView> render_target,
                      scoped_refptr<GPUTextureView> depth_stencil,
                      URGE_EXCEPTION) {
  if (!render_process_.is_null()) {
    // Render context
    auto render_context = Object::Create<RenderContext>(
        world_.get(), render_target, depth_stencil);

    // Collected cameras (viewports)
    std::vector<scoped_refptr<Camera>> cameras(world_->cameras_.begin(),
                                               world_->cameras_.end());

    // Calling on custom rendering process
    render_process_.Run(render_context, std::move(cameras));
  } else {
    exception_state.Throw(ExceptionCode::CONTENT_ERROR, "no renderer setup.");
    return;
  }
}

}  // namespace content

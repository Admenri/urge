// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_CONTEXT_RENDER_CONTEXT_H_
#define RENDERER_CONTEXT_RENDER_CONTEXT_H_

#include "renderer/context/scissor_controller.h"
#include "renderer/resource/render_buffer.h"

namespace renderer {

class RenderContext {
 public:
  ~RenderContext();

  RenderContext(const RenderContext&) = delete;
  RenderContext& operator=(const RenderContext&) = delete;

  // Context access
  inline Diligent::IDeviceContext* operator->() { return context_; }
  inline Diligent::IDeviceContext* operator*() { return context_; }

  // Scissor state stack
  ScissorController* ScissorState() const { return scissor_.get(); }

  // Render targets state manage
  void SetRenderTargets(uint32_t num_targets,
                        Diligent::ITextureView** render_targets,
                        Diligent::ITextureView* depth_stencil,
                        Diligent::RESOURCE_STATE_TRANSITION_MODE mode);
  void GetRenderTargets(uint32_t num_targets,
                        Diligent::ITextureView** render_targets,
                        Diligent::ITextureView** depth_stencil);

 private:
  friend struct base::Allocator;
  friend class RenderDevice;
  RenderContext(Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context);

  Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context_;
  base::OwnedPtr<ScissorController> scissor_;

  Diligent::ITextureView* current_render_targets_[DILIGENT_MAX_RENDER_TARGETS];
  Diligent::ITextureView* current_depth_stencil_;
};

}  // namespace renderer

#endif  //! RENDERER_CONTEXT_RENDER_CONTEXT_H_

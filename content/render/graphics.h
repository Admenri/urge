// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/render/viewport.h"
#include "renderer/device/render_device.h"
#include "ui/context/imgui_context.h"

namespace content {

URGE_BINDING()
class Graphics : public Singleton<Graphics> {
 public:
  Graphics(std::unique_ptr<ui::Widget> window,
           std::unique_ptr<renderer::RenderDevice> device);
  ~Graphics();

  Graphics(const Graphics&) = delete;
  Graphics& operator=(const Graphics&) = delete;

  // Window target
  base::WeakPtr<ui::Widget> window() { return window_->AsWeakPtr(); }

  // GFX Device access
  renderer::RenderDevice* gfx() { return gfx_.get(); }

  // Frame iteration present
  void Present();

 public:
  URGE_BINDING()
  scoped_refptr<Viewport> GetViewport(URGE_EXCEPTION);

 private:
  void ConfigureSwapChainInternal();

  std::unique_ptr<ui::Widget> window_;
  std::unique_ptr<renderer::RenderDevice> gfx_;

  glm::ivec2 window_size_;
  std::unique_ptr<ui::IMGUIContext> ui_context_;

  scoped_refptr<Viewport> viewport_;
};

}  // namespace content

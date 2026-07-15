// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "renderer/device/render_device.h"
#include "ui/widget/widget.h"

struct ImDrawData;

namespace ui {

class IMGUIContext {
 public:
  IMGUIContext(renderer::RenderDevice* render_device,
               wgpu::TextureFormat target_format);
  ~IMGUIContext();

  IMGUIContext(const IMGUIContext&) = delete;
  IMGUIContext& operator=(const IMGUIContext&) = delete;

  // Per new frame setup
  static void SetupFrame();

  // GUI Event iteration
  static void ProcessEvent(const SDL_Event* event);

  // Rendering with WGPU Context
  static void Render(ImDrawData* draw_data,
                     const wgpu::RenderPassEncoder& pass);
};

}  // namespace ui

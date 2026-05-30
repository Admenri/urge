// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "renderer/device/render_device.h"
#include "ui/widget/widget.h"

namespace ui {

class IMGUIContext {
 public:
  IMGUIContext(renderer::RenderDevice* render_device);
  ~IMGUIContext();

  IMGUIContext(const IMGUIContext&) = delete;
  IMGUIContext& operator=(const IMGUIContext&) = delete;
};

}  // namespace ui

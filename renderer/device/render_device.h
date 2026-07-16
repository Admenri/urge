// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>

#include "renderer/renderer_config.h"
#include "ui/widget/widget.h"

namespace renderer {

class RenderDevice {
 public:
  ~RenderDevice();

  RenderDevice(const RenderDevice&) = delete;
  RenderDevice& operator=(const RenderDevice&) = delete;

  // Create render device with current window context and resolution,
  // manage all gpu object internal automatically.
  static std::unique_ptr<RenderDevice> Create(base::WeakPtr<ui::Widget> window);

  base::WeakPtr<ui::Widget> window() { return window_; }

  const wgpu::Surface& swapchain() const { return surface_; }
  const wgpu::Adapter& adapter() const { return adapter_; }
  const wgpu::Device& device() const { return device_; }
  const wgpu::Queue& queue() const { return queue_; }

 private:
  RenderDevice(base::WeakPtr<ui::Widget> window,
               wgpu::Instance instance,
               wgpu::Adapter adapter,
               wgpu::Surface surface,
               wgpu::Device device,
               wgpu::Queue queue);

  base::WeakPtr<ui::Widget> window_;
  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Surface surface_;
  wgpu::Device device_;
  wgpu::Queue queue_;
};

}  // namespace renderer

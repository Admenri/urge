// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_DEVICE_RENDER_DEVICE_H_
#define RENDERER_DEVICE_RENDER_DEVICE_H_

#include "renderer/pipeline/render_pipeline.h"
#include "ui/widget/widget.h"
#include "webgpu/webgpu_cpp.h"

#include <memory>

namespace renderer {

class RenderDevice {
 public:
  struct PipelineSet {
    Pipeline_Base base;
    Pipeline_Color color;

    PipelineSet(const wgpu::Device& device, wgpu::TextureFormat target)
        : base(device, target), color(device, target) {}
  };

  std::unique_ptr<RenderDevice> Create(base::WeakPtr<ui::Widget> window_target,
                                       wgpu::BackendType required_backend);

  RenderDevice(const RenderDevice&) = delete;
  RenderDevice& operator=(const RenderDevice&) = delete;

  // WGPU Meta interface
  wgpu::Instance* GetInstance() { return &instance_; }
  wgpu::Adapter* GetAdapter() { return &adapter_; }
  wgpu::Device* GetDevice() { return &device_; }
  wgpu::Surface* GetSurface() { return &surface_; }

  // Pre-compile shaders set storage
  PipelineSet* GetPipelines() const { return pipelines_.get(); }

 private:
  RenderDevice(base::WeakPtr<ui::Widget> window,
               const wgpu::Instance& instance,
               const wgpu::Adapter& adapter,
               const wgpu::Device& device,
               const wgpu::Surface& surface);

  base::WeakPtr<ui::Widget> window_;

  wgpu::Instance instance_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::Surface surface_;

  std::unique_ptr<PipelineSet> pipelines_;
};

}  // namespace renderer

#endif  //! RENDERER_DEVICE_RENDER_DEVICE_H_

// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_DEVICE_RENDER_DEVICE_H_
#define RENDERER_DEVICE_RENDER_DEVICE_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsTools/interface/GraphicsUtilities.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "renderer/drawable/quad_drawable.h"
#include "renderer/pipeline/render_pipeline.h"
#include "ui/widget/widget.h"

namespace renderer {

using namespace Diligent;

class RenderDevice {
 public:
  enum class RendererBackend {
    kOpenGL = 0,
    kD3D12,
    kD3D11,
    kVulkan,
  };

  struct PipelineStorage {
    PipelineInstance_Base base;

    PipelineStorage(RefCntAutoPtr<IRenderDevice> device) : base(device) {}
  };

  ~RenderDevice();

  RenderDevice(const RenderDevice&) = delete;
  RenderDevice& operator=(const RenderDevice&) = delete;

  static std::unique_ptr<RenderDevice> Create(
      base::WeakPtr<ui::Widget> render_window,
      RendererBackend backend);

  RefCntAutoPtr<IRenderDevice> device() { return device_; }
  RefCntAutoPtr<IDeviceContext> context() { return context_; }
  RefCntAutoPtr<ISwapChain> swapchain() { return swapchain_; }

  PipelineStorage* GetPipelines() { return pipelines_.get(); }

  scoped_refptr<QuadArrayIndices> quad_index_buffer() {
    return quad_index_buffer_;
  }

 private:
  RenderDevice() = default;

  RefCntAutoPtr<IRenderDevice> device_;
  RefCntAutoPtr<IDeviceContext> context_;
  RefCntAutoPtr<ISwapChain> swapchain_;

  std::unique_ptr<PipelineStorage> pipelines_;

  scoped_refptr<QuadArrayIndices> quad_index_buffer_;
};

}  // namespace renderer

#endif  //! RENDERER_DEVICE_RENDER_DEVICE_H_

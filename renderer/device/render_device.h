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

inline void MakeProjectionMatrix(float* out,
                                 const base::Vec2& size,
                                 bool origin_bottom) {
  const float aa = 2.0f / size.x;
  const float bb = (origin_bottom ? 2.0f : -2.0f) / size.y;
  const float cc = origin_bottom ? 2.0f : 1.0f;
  const float dd = -1.0f;
  const float ee = origin_bottom ? -1.0f : 1.0f;
  const float ff = origin_bottom ? -1.0f : 0.0f;

  memset(out, 0, sizeof(float) * 16);
  out[0] = aa;
  out[5] = bb;
  out[10] = cc;

  out[3] = dd;
  out[7] = ee;
  out[11] = ff;
  out[15] = 1.0f;
}

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
    PipelineInstance_Blt blt;
    PipelineInstance_Color color;

    PipelineStorage(RefCntAutoPtr<IRenderDevice> device)
        : base(device), blt(device), color(device) {}
  };

  ~RenderDevice();

  RenderDevice(const RenderDevice&) = delete;
  RenderDevice& operator=(const RenderDevice&) = delete;

  static std::unique_ptr<RenderDevice> Create(
      base::WeakPtr<ui::Widget> render_window,
      RendererBackend backend);

  base::WeakPtr<ui::Widget> window() { return render_window_; }

  RefCntAutoPtr<IRenderDevice> device() { return device_; }
  RefCntAutoPtr<IDeviceContext> context() { return context_; }
  RefCntAutoPtr<ISwapChain> swapchain() { return swapchain_; }

  PipelineStorage* GetPipelines() { return pipelines_.get(); }

  scoped_refptr<QuadArrayIndices> quad_index_buffer() {
    return quad_index_buffer_;
  }

  QuadDrawable* common_quad() { return common_quad_.get(); }

  RefCntAutoPtr<ITexture> MakeGenericFramebuffer(const base::Vec2i& size);

 private:
  RenderDevice() = default;

  base::WeakPtr<ui::Widget> render_window_;
  RefCntAutoPtr<IRenderDevice> device_;
  RefCntAutoPtr<IDeviceContext> context_;
  RefCntAutoPtr<ISwapChain> swapchain_;

  std::unique_ptr<PipelineStorage> pipelines_;

  scoped_refptr<QuadArrayIndices> quad_index_buffer_;
  std::unique_ptr<QuadDrawable> common_quad_;
  RefCntAutoPtr<ITexture> generic_framebuffer_;
};

}  // namespace renderer

#endif  //! RENDERER_DEVICE_RENDER_DEVICE_H_

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

#include <algorithm>

namespace renderer {

using namespace Diligent;

void CopyTexture(IDeviceContext* context,
                 ITexture* src,
                 const base::Rect& src_region,
                 ITexture* dst,
                 const base::Vec2i& dst_pos);

void MakeProjectionMatrix(float* out,
                          const base::Vec2& size,
                          bool origin_bottom);

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
    PipelineInstance_BaseSprite basesprite;
    PipelineInstance_Sprite sprite;
    PipelineInstance_Viewport viewport;
    PipelineInstance_AlphaTrans alphatrans;
    PipelineInstance_VagueTrans vaguetrans;
    PipelineInstance_BaseAlpha basealpha;
    PipelineInstance_Tilemap tilemap;
    PipelineInstance_Tilemap2 tilemap2;

    PipelineStorage(RefCntAutoPtr<IRenderDevice> device,
                    TEXTURE_FORMAT target_fmt)
        : base(device, target_fmt),
          blt(device, target_fmt),
          color(device, target_fmt),
          basesprite(device, target_fmt),
          sprite(device, target_fmt),
          viewport(device, target_fmt),
          alphatrans(device, target_fmt),
          vaguetrans(device, target_fmt),
          basealpha(device, target_fmt),
          tilemap(device, target_fmt),
          tilemap2(device, target_fmt) {}
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

  void InitializePipelines(TEXTURE_FORMAT render_target_format);
  PipelineStorage* GetPipelines() { return pipelines_.get(); }

  scoped_refptr<QuadArrayIndices> quad_index_buffer() {
    return quad_index_buffer_;
  }

  QuadDrawable* common_quad() { return common_quad_.get(); }

  RefCntAutoPtr<ITexture> MakeGenericFramebuffer(const base::Vec2i& size,
                                                 TEXTURE_FORMAT texfmt);

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

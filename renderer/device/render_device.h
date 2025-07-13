// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_DEVICE_RENDER_DEVICE_H_
#define RENDERER_DEVICE_RENDER_DEVICE_H_

#include <tuple>

#include "base/memory/allocator.h"
#include "renderer/pipeline/render_pipeline.h"
#include "renderer/resource/render_buffer.h"
#include "ui/widget/widget.h"

namespace renderer {

enum class DriverType {
  UNDEFINED = 0,
  OPENGL,
  VULKAN,
  D3D11,
  D3D12,
};

enum class SamplerType {
  LINEAR = 0,
  NEAREST,
};

class RenderDevice {
 public:
  struct PipelineSet {
    Pipeline_Base base;
    Pipeline_BitmapBlt bitmapblt;
    Pipeline_Color color;
    Pipeline_Flat viewport;
    Pipeline_Sprite sprite;
    Pipeline_AlphaTransition alphatrans;
    Pipeline_VagueTransition mappedtrans;
    Pipeline_Tilemap tilemap;
    Pipeline_Tilemap2 tilemap2;
    Pipeline_BitmapHue bitmaphue;
    Pipeline_Spine2D spine2d;
    Pipeline_YUV yuv;

    PipelineSet(const PipelineInitParams& init_params)
        : base(init_params),
          bitmapblt(init_params),
          color(init_params),
          viewport(init_params),
          sprite(init_params),
          alphatrans(init_params),
          mappedtrans(init_params),
          tilemap(init_params),
          tilemap2(init_params),
          bitmaphue(init_params),
          spine2d(init_params),
          yuv(init_params) {}
  };

  using CreateDeviceResult = std::tuple<base::OwnedPtr<RenderDevice>,
                                        RRefPtr<Diligent::IDeviceContext>>;
  static CreateDeviceResult Create(base::WeakPtr<ui::Widget> window_target,
                                   DriverType driver_type,
                                   SamplerType default_sampler,
                                   bool validation);

  ~RenderDevice();

  RenderDevice(const RenderDevice&) = delete;
  RenderDevice& operator=(const RenderDevice&) = delete;

  // Device access
  Diligent::IRenderDevice* operator->() { return device_; }
  Diligent::IRenderDevice* operator*() { return device_; }

  // Device Attribute interface
  base::WeakPtr<ui::Widget> GetWindow() { return window_; }
  Diligent::ISwapChain* GetSwapChain() const { return swapchain_; }

  // Pre-compile shaders set storage
  PipelineSet* GetPipelines() { return &pipelines_; }
  QuadIndexCache* GetQuadIndex() { return &quad_index_; }

  // Max texture size
  int32_t MaxTextureSize() const { return max_texture_size_; }

  // Managed mobile rendering context
  void SuspendContext();
  int32_t ResumeContext(Diligent::IDeviceContext* immediate_context);

 private:
  friend struct base::Allocator;
  RenderDevice(base::WeakPtr<ui::Widget> window,
               const Diligent::SwapChainDesc& swapchain_desc,
               int32_t max_texture_size,
               const PipelineInitParams& pipeline_default_params,
               Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
               Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain,
               SDL_GLContext gl_context);

  base::WeakPtr<ui::Widget> window_;
  Diligent::SwapChainDesc swapchain_desc_;
  int32_t max_texture_size_;

  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
  Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain_;

  PipelineSet pipelines_;
  QuadIndexCache quad_index_;

  Diligent::RENDER_DEVICE_TYPE device_type_;
  SDL_GLContext gl_context_;
};

}  // namespace renderer

#endif  //! RENDERER_DEVICE_RENDER_DEVICE_H_

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_DEVICE_RENDER_DEVICE_H_
#define RENDERER_DEVICE_RENDER_DEVICE_H_

#include <tuple>

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
  WEBGPU,
  kNums,
};

class RenderDevice {
 public:
  using CreateDeviceResult = std::tuple<std::unique_ptr<RenderDevice>,
                                        RRefPtr<Diligent::IDeviceContext>>;
  static CreateDeviceResult Create(base::WeakPtr<ui::Widget> window_target,
                                   DriverType driver_type,
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

  // Max texture size
  int32_t MaxTextureSize() const { return max_texture_size_; }

  // Managed mobile rendering context
  void SuspendContext();
  int32_t ResumeContext(Diligent::IDeviceContext* immediate_context);

 private:
  RenderDevice(int32_t max_texture_size,
               base::WeakPtr<ui::Widget> window,
               const Diligent::SwapChainDesc& swapchain_desc,
               Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
               Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain,
               SDL_GLContext gl_context);

  base::WeakPtr<ui::Widget> window_;
  Diligent::SwapChainDesc swapchain_desc_;
  int32_t max_texture_size_;

  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device_;
  Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain_;

  Diligent::RENDER_DEVICE_TYPE device_type_;
  SDL_GLContext gl_context_;
};

}  // namespace renderer

#endif  //! RENDERER_DEVICE_RENDER_DEVICE_H_

// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/graphics.h"

namespace content {

Graphics::Graphics(std::unique_ptr<ui::Widget> window,
                   std::unique_ptr<renderer::RenderDevice> device)
    : window_(std::move(window)),
      gfx_(std::move(device)),
      ui_context_(std::make_unique<ui::IMGUIContext>(gfx_.get())) {
  window_size_ = window_->GetSize();

  // SwapChain
  ConfigureSwapChainInternal();
}

Graphics::~Graphics() {
  gfx_->swapchain().Unconfigure();
}

void Graphics::Present() {
  // Test clear
  wgpu::SurfaceTexture surface_tex;
  gfx_->swapchain().GetCurrentTexture(&surface_tex);
  wgpu::CommandEncoderDescriptor encoder_desc;
  auto encoder = gfx_->device().CreateCommandEncoder(&encoder_desc);
  wgpu::RenderPassColorAttachment attachment;
  attachment.view = surface_tex.texture.CreateView(nullptr);
  attachment.clearValue = {0.6f, 0.2f, 1.f, 1.f};
  attachment.loadOp = wgpu::LoadOp::Clear;
  attachment.storeOp = wgpu::StoreOp::Store;
  wgpu::RenderPassDescriptor render_desc;
  render_desc.colorAttachmentCount = 1;
  render_desc.colorAttachments = &attachment;
  auto render = encoder.BeginRenderPass(&render_desc);
  render.End();
  auto buffer = encoder.Finish(nullptr);
  gfx_->queue().Submit(1, &buffer);

  // Resizing
  if (auto current_size = window_->GetSize(); window_size_ != current_size) {
    gfx_->swapchain().Unconfigure();
    ConfigureSwapChainInternal();
    window_size_ = current_size;
  }

  // Final present
  gfx_->swapchain().Present();
}

scoped_refptr<Viewport> Graphics::GetViewport(URGE_EXCEPTION) {
  return nullptr;
}

void Graphics::ConfigureSwapChainInternal() {
  wgpu::SurfaceCapabilities swapchain_info;
  gfx_->swapchain().GetCapabilities(gfx_->adapter(), &swapchain_info);

  wgpu::SurfaceConfiguration swapchain_desc;
  swapchain_desc.device = gfx_->device();
  swapchain_desc.format = swapchain_info.formats[0];
  swapchain_desc.width = window_size_.x;
  swapchain_desc.height = window_size_.y;
  swapchain_desc.presentMode = wgpu::PresentMode::Fifo;

  gfx_->swapchain().Configure(&swapchain_desc);
}

}  // namespace content

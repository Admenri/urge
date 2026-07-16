// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/graphics.h"

#include "imgui/imgui.h"

namespace content {

Graphics::Graphics(std::unique_ptr<ui::Widget> window,
                   std::unique_ptr<renderer::RenderDevice> device)
    : window_(std::move(window)),
      gfx_(std::move(device)),
      window_size_(window_->GetSize()) {
  // Surface capability & surface format
  wgpu::SurfaceCapabilities swapchain_info;
  gfx_->swapchain().GetCapabilities(gfx_->adapter(), &swapchain_info);
  surface_format_ = swapchain_info.formatCount > 0
                        ? swapchain_info.formats[0]
                        : wgpu::TextureFormat::Undefined;

  // UI Context
  ui_context_ = std::make_unique<ui::IMGUIContext>(gfx_.get(), surface_format_);

  // Swapchain configure
  ConfigureSwapChainInternal();

  // Primary viewport
  viewport_ = Object::Create<Viewport>();
}

Graphics::~Graphics() {
  // Release GUI context before unconfigure
  ui_context_.reset();
  gfx_->swapchain().Unconfigure();
}

void Graphics::Present() {
  // Resizing
  if (auto current_size = window_->GetSize(); window_size_ != current_size) {
    gfx_->swapchain().Unconfigure();
    ConfigureSwapChainInternal();
    window_size_ = current_size;
  }

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

  // Test imgui demo
  ui::IMGUIContext::SetupFrame();

  ImGui::NewFrame();
  ImGui::ShowDemoWindow();
  ImGui::EndFrame();
  ImGui::Render();

  // GUI Layer
  ui::IMGUIContext::Render(ImGui::GetDrawData(), render);

  render.End();
  auto buffer = encoder.Finish(nullptr);

  // Submit
  gfx_->queue().Submit(1, &buffer);

  // Main viewport
  ExceptionState render_state;
  viewport_->Render(screen_back_buffer_view_, screen_depth_stencil_view_,
                    render_state);

  // Final present
  gfx_->swapchain().Present();
}

scoped_refptr<Viewport> Graphics::GetViewport(URGE_EXCEPTION) {
  return viewport_;
}

void Graphics::ConfigureSwapChainInternal() {
  // Resize swapchain
  wgpu::SurfaceConfiguration swapchain_desc;
  swapchain_desc.device = gfx_->device();
  swapchain_desc.format = surface_format_;
  swapchain_desc.width = window_size_.x;
  swapchain_desc.height = window_size_.y;
  gfx_->swapchain().Configure(&swapchain_desc);

  // Resize viewport back buffer
  wgpu::TextureDescriptor texture_desc;
  texture_desc.size.width = window_size_.x;
  texture_desc.size.height = window_size_.y;
  texture_desc.usage =
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  texture_desc.dimension = wgpu::TextureDimension::e2D;

  texture_desc.format = surface_format_;
  screen_back_buffer_ = gfx_->device().CreateTexture(&texture_desc);

  texture_desc.format = wgpu::TextureFormat::Depth24PlusStencil8;
  screen_depth_stencil_ = gfx_->device().CreateTexture(&texture_desc);

  // Texture view rebuild
  screen_back_buffer_view_ =
      Object::Create<GPUTextureView>(screen_back_buffer_.CreateView(nullptr));
  screen_depth_stencil_view_ =
      Object::Create<GPUTextureView>(screen_depth_stencil_.CreateView(nullptr));

  // LOG
  LOG(INFO) << "SwapChain Resize: " << window_size_.x << "x" << window_size_.y;
}

}  // namespace content

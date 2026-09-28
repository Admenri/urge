// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/graphics.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "core/common.h"
#include "core/config.h"
#include "core/exception.h"
#include "core/gpu.h"
#include "core/pipeline.h"
#include "core/primitive.h"

namespace urge {

ScreenRootNode::ScreenRootNode() : Node() {}

ScreenRootNode::~ScreenRootNode() {
  Disposable::Dispose();
}

void ScreenRootNode::DisposeObject() {}

void ScreenRootNode::Prepare(DrawParam param) {}

void ScreenRootNode::DoDraw(DrawParam param) {}

void ScreenRootNode::PostDraw(DrawParam param) {}

// -------------------------------------------------------------------------------

Graphics::Graphics() {
  auto window_flag = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
  window_ = SDL_CreateWindow(Config::Get().title.c_str(), Config::Get().width,
                             Config::Get().height, window_flag);

  GPUDevice::Reset(new GPUDevice(window_));
  ShaderSet::Reset(new ShaderSet());

  root_ = MakeRefCounted<ScreenRootNode>();
  ResizeScreen(Config::Get().width, Config::Get().height);

  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
  buffer_desc.size = 6 * sizeof(VertexData);
  vertex_buffer_ = GPUDevice::Get().device().CreateBuffer(&buffer_desc);
}

Graphics::~Graphics() {
  vertex_buffer_ = nullptr;

  screen_texture_.reset();
  root_.reset();

  ShaderSet::Reset(nullptr);
  GPUDevice::Reset(nullptr);

  SDL_DestroyWindow(window_);
}

void Graphics::Update() {
  if (!frozen_)
    root_->Render(screen_texture_);

  PresentInternal();
}

void Graphics::Wait(int32_t duration) {
  for (int32_t i = 0; i < duration; ++i)
    Update();
}

void Graphics::FadeIn(int32_t duration) {
  duration = std::max(duration, 1);
  int32_t step = (255 - brightness_) / duration;
  for (int32_t i = 0; i < duration; ++i) {
    brightness_ += step;
    brightness_ = std::clamp<int32_t>(brightness_, 0, 255);

    Update();
  }
  brightness_ = 255;
}

void Graphics::FadeOut(int32_t duration) {
  duration = std::max(duration, 1);
  int32_t step = brightness_ / duration;
  for (int32_t i = 0; i < duration; ++i) {
    brightness_ -= step;
    brightness_ = std::clamp<int32_t>(brightness_, 0, 255);

    Update();
  }
  brightness_ = 0;
}

void Graphics::Freeze() {
  if (!frozen_) {
    root_->Render(screen_texture_);
    frozen_ = true;
  }
}

void Graphics::Transition(int32_t duration,
                          std::string filename,
                          int32_t vague) {
  RefPtr<Bitmap> mapping = nullptr;
  if (!filename.empty())
    mapping = MakeRefCounted<Bitmap>(filename);

  TransitionBitmap(duration, mapping, vague);
}

void Graphics::TransitionBitmap(int32_t duration,
                                RefPtr<Bitmap> bitmap,
                                int32_t vague) {}

RefPtr<Bitmap> Graphics::SnapToBitmap() {
  auto result = MakeRefCounted<Bitmap>(GetWidth(), GetHeight());
  root_->Render(result);
  return result;
}

void Graphics::FrameReset() {
  // TODO
}

int32_t Graphics::GetWidth() {
  return screen_texture_->GetWidth();
}

int32_t Graphics::GetHeight() {
  return screen_texture_->GetHeight();
}

void Graphics::ResizeScreen(int32_t width, int32_t height) {
  screen_texture_ = MakeRefCounted<Bitmap>(width, height);
  configured_ = false;

  SDL_SetWindowSize(window_, width, height);
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED);
}

void Graphics::PlayMovie(std::string filename) {
  throw Exception(Exception::kRGSSError, "unimplement video playback.");
}

ATTR_DEF(Graphics, int32_t, FrameRate) {
  // TODO
  return value;
}

ATTR_DEF(Graphics, int32_t, FrameCount) {
  // TODO
  return value;
}

ATTR_DEF(Graphics, int32_t, Brightness) {
  if (value.has_value()) {
    brightness_ = std::clamp(*value, 0, 255);
    return std::nullopt;
  } else {
    return brightness_;
  }
}

void Graphics::PresentInternal() {
  auto surface = GPUDevice::Get().swapchain();

  // The surface is configured once and again after the screen was resized, the
  // size it covers is the one the screen texture is rendered at
  if (!configured_) {
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(GPUDevice::Get().adapter(), &capabilities);

    surface.Unconfigure();

    wgpu::SurfaceConfiguration configure;
    configure.device = GPUDevice::Get().device();
    configure.format = wgpu::TextureFormat::RGBA8Unorm;
    configure.usage = wgpu::TextureUsage::RenderAttachment;
    configure.width = static_cast<uint32_t>(GetWidth());
    configure.height = static_cast<uint32_t>(GetHeight());
    configure.presentMode = wgpu::PresentMode::Fifo;
    surface.Configure(&configure);

    configured_ = true;
  }

  wgpu::SurfaceTexture surface_texture;
  surface.GetCurrentTexture(&surface_texture);
  auto surface_view = surface_texture.texture.CreateView(nullptr);

  primitive_.EmitQuad(RectI(0, 0, surface_texture.texture.GetWidth(),
                            surface_texture.texture.GetHeight()),
                      RectI(0, 0, 1, 1), Vec4(1.0f));
  auto vertices = primitive_.End();
  GPUDevice::Get().queue().WriteBuffer(vertex_buffer_, 0, vertices.data(),
                                       vertices.size_bytes());

  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);
  wgpu::RenderPassColorAttachment color_attachment;
  color_attachment.view = surface_view;
  color_attachment.loadOp = wgpu::LoadOp::Clear;
  color_attachment.storeOp = wgpu::StoreOp::Store;
  color_attachment.clearValue = {0.67f, 0.54f, 0.87f, 1.0f};
  wgpu::RenderPassDescriptor pass_desc;
  pass_desc.colorAttachmentCount = 1;
  pass_desc.colorAttachments = &color_attachment;
  auto pass = encoder.BeginRenderPass(&pass_desc);
  pass.SetPipeline(ShaderSet::Get().state.texture_none);
  pass.SetBindGroup(0, screen_texture_->scene_group(), 0, nullptr);
  pass.SetBindGroup(1, screen_texture_->object_group(), 0, nullptr);
  pass.SetBindGroup(2, screen_texture_->texture_group(), 0, nullptr);
  pass.SetVertexBuffer(0, vertex_buffer_, 0, WGPU_WHOLE_SIZE);
  pass.Draw(6, 1, 0, 0);
  pass.End();
  auto command = encoder.Finish(nullptr);
  GPUDevice::Get().queue().Submit(1, &command);

  surface.Present();
}

}  // namespace urge

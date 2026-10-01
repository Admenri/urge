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

#include "SDL3/SDL_events.h"

#include "core/common.h"
#include "core/config.h"
#include "core/exception.h"
#include "core/gpu.h"
#include "core/pipeline.h"
#include "core/primitive.h"
#include "core/uniform.h"

namespace urge {

ScreenRootNode::ScreenRootNode() : Node() {}

ScreenRootNode::~ScreenRootNode() {
  Disposable::Dispose();
}

void ScreenRootNode::DisposeObject() {}

bool ScreenRootNode::Prepare(DrawParam param) {
  const glm::vec4 brightness_tint(
      0.0f, 0.0f, 0.0f,
      static_cast<float>(255 - Graphics::Get().brightness_) / 255.0f);
  param->vertices->EmitQuad(RectI(param->target->size()), RectF(),
                            brightness_tint);
  slot_ = param->vertices->End();

  return true;
}

bool ScreenRootNode::DoDraw(DrawParam param) {
  // The quad of this node is drawn by PostDraw() once the children are drawn
  return true;
}

void ScreenRootNode::PostDraw(DrawParam param) {
  auto pipeline = ShaderSet::Get().state.color_pma;
  param->pass.SetPipeline(pipeline);
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetBindGroup(1, param->target->object_group(), 0, nullptr);
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);
  param->pass.Draw(slot_.count, 1, slot_.first, 0);
}

// -------------------------------------------------------------------------------

Graphics::Graphics() {
  auto window_flag = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS;
  window_ = SDL_CreateWindow(Config::Get().title.c_str(), Config::Get().width,
                             Config::Get().height, window_flag);

  GPUDevice::Reset(new GPUDevice(window_));
  ShaderSet::Reset(new ShaderSet());

  /* The uniform pools and the vertex buffer of a frame outlive the frames, so
     both managers share the lifetime of Graphics. */
  UniformManager::Reset(new UniformManager());
  QuadVertexManager::Reset(new QuadVertexManager());

  root_ = MakeRefCounted<ScreenRootNode>();
  ResizeScreen(Config::Get().width, Config::Get().height);
}

Graphics::~Graphics() {
  present_ = {};
  screen_.reset();
  root_.reset();

  QuadVertexManager::Reset(nullptr);
  UniformManager::Reset(nullptr);
  ShaderSet::Reset(nullptr);
  GPUDevice::Reset(nullptr);

  SDL_DestroyWindow(window_);
}

void Graphics::Update() {
  if (!frozen_)
    root_->Render(screen_, Color::Black());

  PresentInternal();
}

void Graphics::Wait(int32_t duration) {
  for (int32_t i = 0; i < duration; ++i)
    Update();
}

void Graphics::Fadein(int32_t duration) {
  duration = std::max(duration, 1);
  int32_t step = (255 - brightness_) / duration;
  for (int32_t i = 0; i < duration; ++i) {
    brightness_ += step;
    brightness_ = std::clamp<int32_t>(brightness_, 0, 255);

    Update();
  }
  brightness_ = 255;
}

void Graphics::Fadeout(int32_t duration) {
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
    root_->Render(screen_, Color::Black());
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
                                int32_t vague) {
  brightness_ = 255;
  frozen_ = false;
}

RefPtr<Bitmap> Graphics::SnapToBitmap() {
  auto result = MakeRefCounted<Bitmap>(Width(), Height());
  root_->Render(result, Color::Black());
  return result;
}

void Graphics::FrameReset() {
  // TODO
}

int32_t Graphics::Width() {
  return screen_->size().x;
}

int32_t Graphics::Height() {
  return screen_->size().y;
}

void Graphics::ResizeScreen(int32_t width, int32_t height) {
  present_.configured = false;
  screen_ = MakeRefCounted<Bitmap>(width, height);

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
  if (value.has_value()) {
    frame_count_ = *value;
    return std::nullopt;
  } else {
    return frame_count_;
  }
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

  SDL_Event event;
  SDL_PollEvent(&event);
  if (event.type == SDL_EVENT_QUIT)
    throw Exception(Exception::kExitError, {});

  // The surface is configured once and again after the screen was resized, the
  // size it covers is the one the screen texture is rendered at
  if (!present_.configured) {
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(GPUDevice::Get().adapter(), &capabilities);

    surface.Unconfigure();
    wgpu::SurfaceConfiguration configure;
    configure.device = GPUDevice::Get().device();
    configure.format = capabilities.formats[0];
    configure.usage = wgpu::TextureUsage::RenderAttachment;
    configure.width = static_cast<uint32_t>(Width());
    configure.height = static_cast<uint32_t>(Height());
    configure.presentMode = wgpu::PresentMode::Fifo;
    surface.Configure(&configure);

    wgpu::PrimitiveState primitive;
    primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    present_.pipeline = ShaderSet::Get().shader.texture_base.MakeState(
        primitive, std::nullopt,
        {wgpu::ColorTargetState{.format = configure.format}});

    present_.configured = true;
  }

  wgpu::SurfaceTexture surface_texture;
  surface.GetCurrentTexture(&surface_texture);
  auto surface_view = surface_texture.texture.CreateView(nullptr);

  // The emitter owns and uploads the buffer the quad of the present goes into
  present_.primitive.EmitQuad(RectI(0, 0, surface_texture.texture.GetWidth(),
                                    surface_texture.texture.GetHeight()),
                              RectI(0, 0, 1, 1), glm::vec4(1.0f));
  const std::uint32_t vertex_count = present_.primitive.Upload();

  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);
  {
    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = surface_view;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = {0.67f, 0.54f, 0.87f, 1.0f};
    wgpu::RenderPassDescriptor pass_desc;
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;
    auto pass = encoder.BeginRenderPass(&pass_desc);
    {
      pass.SetPipeline(present_.pipeline);
      pass.SetBindGroup(0, screen_->scene_group(), 0, nullptr);
      pass.SetBindGroup(1, screen_->object_group(), 0, nullptr);
      pass.SetBindGroup(2, screen_->texture_group(), 0, nullptr);
      pass.SetVertexBuffer(0, present_.primitive.buffer(), 0, WGPU_WHOLE_SIZE);
      pass.Draw(vertex_count, 1, 0, 0);
    }
    pass.End();
  }
  auto command = encoder.Finish(nullptr);
  GPUDevice::Get().queue().Submit(1, &command);

  surface.Present();
}

}  // namespace urge

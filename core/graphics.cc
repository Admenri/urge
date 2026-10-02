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
#include "SDL3/SDL_timer.h"

#include "core/common.h"
#include "core/config.h"
#include "core/exception.h"
#include "core/gpu.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/primitive.h"
#include "core/uniform.h"

namespace urge {

namespace {

//! The lowest and the highest frame rate SetFrameRate() accepts. The two ends
//! are the usual 30 and 240 of a game loop: below the first a window would look
//! sluggish, above the second the pacing costs more than it gains.
constexpr int32_t kMinFrameRate = 30;
constexpr int32_t kMaxFrameRate = 240;

//! The timestamp of the monotonic clock of SDL, in nanoseconds. The return
//! value of SDL_GetTicksNS() wraps only after ~584 years, so the difference of
//! two of its readings is safe to take and never needs an unsigned guard.
uint64_t NowNS() {
  return SDL_GetTicksNS();
}

}  // namespace

// -------------------------------------------------------------------------------

FrameController::FrameController() {
  SetFrameRate(frame_rate_);
}

void FrameController::SetFrameRate(int32_t frame_rate) {
  // A rate of zero means the caller asked for no cap at all, every other rate
  // is clamped so a stray value cannot make the loop spin or stall.
  frame_rate = std::max(frame_rate, 0);
  if (frame_rate != 0)
    frame_rate = std::clamp(frame_rate, kMinFrameRate, kMaxFrameRate);

  frame_rate_ = frame_rate;
  uncapped_ = frame_rate_ == 0;
  frame_period_ns_ = uncapped_ ? 0 : 1'000'000'000ull / frame_rate_;
}

void FrameController::SetSkipEnabled(bool enabled) {
  skip_enabled_ = enabled;
}

bool FrameController::BeginFrame() {
  frame_start_ns_ = NowNS();

  // A frame that started less than a period after the previous one means the
  // loop is running ahead of its target, which is Delay()'s case to handle and
  // never a reason to skip. A frame that started later than a period is one the
  // loop fell behind on, and skipping it is how the loop catches up.
  bool late = !uncapped_ && last_frame_start_ns_ != 0 &&
              frame_start_ns_ - last_frame_start_ns_ > frame_period_ns_;

  bool skip = skip_enabled_ && late;

  // The clock is not advanced when the frame is skipped: the next frame should
  // still be measured against the last one that was actually rendered.
  if (!skip)
    last_frame_start_ns_ = frame_start_ns_;

  return skip;
}

void FrameController::Delay() {
  if (uncapped_)
    return;

  // A start of zero means Reset() was called between BeginFrame() and here, so
  // there is no frame to pace and the elapsed time would be meaningless.
  if (frame_start_ns_ == 0)
    return;

  // The budget of the frame is measured from the moment BeginFrame() marked its
  // start, so the rendering time of the frame is already part of the elapsed
  // time and only the remainder has to be slept away.
  uint64_t elapsed = NowNS() - frame_start_ns_;
  if (elapsed >= frame_period_ns_)
    return;

  SDL_Delay(static_cast<Uint32>((frame_period_ns_ - elapsed) / 1'000'000));
}

void FrameController::Reset() {
  // A zero last_frame_start_ns_ is what BeginFrame() reads as "no previous
  // frame yet", so the next frame it sees can never be late and is used to seed
  // the measurement again. Dropping the start of the frame underway matters for
  // the same reason from the other side: Delay() computes the remainder against
  // it, and a stale one would report the whole period as already elapsed.
  frame_start_ns_ = 0;
  last_frame_start_ns_ = 0;
}

// -------------------------------------------------------------------------------

ScreenRootNode::ScreenRootNode() : Node() {}

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
  /* SDL_WINDOW_HIGH_PIXEL_DENSITY asks the platform for a back buffer at the
     pixel density of the display: on a 200% scaled screen the window spans the
     same logical size it would at 100% but is backed by twice the pixels, so it
     no longer looks tiny next to the rest of the desktop. The window is sized
     in logical points; the physical pixel size comes from
     SDL_GetWindowSizeInPixels() and is what the swapchain is configured with,
     see PresentInternal. */
  auto window_flag = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS |
                     SDL_WINDOW_HIGH_PIXEL_DENSITY;
  window_ = SDL_CreateWindow(Config::Get().title.c_str(), Config::Get().width,
                             Config::Get().height, window_flag);

  frame_controller_.SetFrameRate(Config::Get().xp() ? 40 : 60);

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
  const bool skip_frame = frame_controller_.BeginFrame();

  if (!skip_frame) {
    if (!frozen_)
      root_->Render(screen_, Color::Black());

    ++frame_count_;
  }

  PresentInternal();

  frame_controller_.Delay();
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
  if (!frozen_)
    return;

  brightness_ = 255;

  const RefPtr<Bitmap> frozen_scene = MakeRefCounted<Bitmap>(screen_);
  const RefPtr<Bitmap> current_scene = SnapToBitmap();

  /* The mapping of a vague transition is the bitmap given by filename, an
     alpha transition has none and fades one scene into the other, see
     kFS_TransitionAlpha / kFS_TransitionMap. */
  const bool mapped = bitmap != nullptr;
  const wgpu::RenderPipeline pipeline =
      mapped ? ShaderSet::Get().state.transition_vague
             : ShaderSet::Get().state.transition_alpha;

  /* Set 2 of a transition shader carries both scenes and, for a vague one, the
     mapping: it is not the single texture of a render target, so its bind group
     is built from the layout of the pipeline rather than by a Bitmap. The
     current scene is re-rendered into the same texture every frame, so the
     group is built once and keeps pointing at it. */
  std::vector<std::pair<uint32_t, WBinding>> bindings = {
      {0, WTextureViewSet(frozen_scene->texture_view())},
      {1, WSamplerSet(frozen_scene->sampler())},
      {2, WTextureViewSet(current_scene->texture_view())},
      {3, WSamplerSet(current_scene->sampler())}};
  if (mapped) {
    bindings.push_back({4, WTextureViewSet(bitmap->texture_view())});
    bindings.push_back({5, WSamplerSet(bitmap->sampler())});
  }
  const wgpu::BindGroup scene_textures =
      CreateWGroup(pipeline.GetBindGroupLayout(2), bindings);

  const int32_t steps = std::max(duration, 1);
  const float vague_norm = std::clamp(vague, 1, 256) / 256.0f;

  for (int32_t frame = 0; frame < steps; ++frame) {
    const float progress = frame * (1.0f / duration);
    const glm::vec4 vertex_color = glm::vec4(vague_norm, 0.0f, 0.0f, progress);

    quad_emitter_.Reset();
    quad_emitter_.EmitQuad(RectF(0.0f, 0.0f, static_cast<float>(Width()),
                                 static_cast<float>(Height())),
                           RectF(0.0f, 0.0f, 1.0f, 1.0f), vertex_color);
    const std::uint32_t vertex_count = quad_emitter_.Upload();

    auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);
    {
      wgpu::RenderPassColorAttachment color_attachment = {
          .view = screen_->texture_view(),
          .loadOp = wgpu::LoadOp::Load,
          .storeOp = wgpu::StoreOp::Store,
      };
      wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment = {
          .view = screen_->depth_stencil_view(),
          .depthLoadOp = wgpu::LoadOp::Load,
          .depthStoreOp = wgpu::StoreOp::Discard,
          .stencilLoadOp = wgpu::LoadOp::Load,
          .stencilStoreOp = wgpu::StoreOp::Discard,
      };
      wgpu::RenderPassDescriptor pass_desc;
      pass_desc.colorAttachmentCount = 1;
      pass_desc.colorAttachments = &color_attachment;
      pass_desc.depthStencilAttachment = &depth_stencil_attachment;
      auto pass = encoder.BeginRenderPass(&pass_desc);
      {
        pass.SetPipeline(pipeline);
        pass.SetBindGroup(0, screen_->scene_group(), 0, nullptr);
        pass.SetBindGroup(1, screen_->object_group(), 0, nullptr);
        pass.SetBindGroup(2, scene_textures, 0, nullptr);
        pass.SetVertexBuffer(0, quad_emitter_.buffer(), 0, WGPU_WHOLE_SIZE);
        pass.Draw(vertex_count, 1, 0, 0);
      }
      pass.End();
    }
    auto command = encoder.Finish(nullptr);
    GPUDevice::Get().queue().Submit(1, &command);

    Update();
  }

  frozen_ = false;
}

RefPtr<Bitmap> Graphics::SnapToBitmap() {
  auto result = MakeRefCounted<Bitmap>(Width(), Height());
  root_->Render(result, Color::Black());
  return result;
}

void Graphics::FrameReset() {
  frame_controller_.Reset();
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

  /* The window is sized in logical points, so the aspect ratio it is asked to
     keep is the one of the screen itself: the surface is then backed by whole
     multiples of the screen and the present quad covers it without distortion,
     even when the user resizes the window by hand. */
  SDL_SetWindowAspectRatio(
      window_, static_cast<float>(width) / static_cast<float>(height),
      static_cast<float>(width) / static_cast<float>(height));
  SDL_SetWindowSize(window_, width, height);
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED);
}

void Graphics::PlayMovie(std::string filename) {
  throw Exception(Exception::kRGSSError, "unimplement video playback.");
}

ATTR_DEF(Graphics, int32_t, FrameRate) {
  if (value.has_value()) {
    frame_controller_.SetFrameRate(*value);
    return std::nullopt;
  } else {
    return frame_controller_.FrameRate();
  }
}

ATTR_DEF(Graphics, bool, FrameSkip) {
  if (value.has_value()) {
    frame_controller_.SetSkipEnabled(*value);
    return std::nullopt;
  } else {
    return frame_controller_.SkipEnabled();
  }
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
  while (SDL_PollEvent(&event)) {
    Input::Get().ProcessEvents(&event);

    if (event.type == SDL_EVENT_QUIT)
      throw Exception(Exception::kExitError, {});

    /* A resize of the window changes the size the swapchain is backed by, and
       WebGPU requires the surface to be reconfigured against it before the
       next GetCurrentTexture, otherwise the frame is presented at the old
       size. Only the pixel size matters here: it follows the density of the
       display, see the window flags in the constructor. */
    if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
      present_.configured = false;
  }

  /* The surface is configured once, and again after the screen or the window
     was resized. Its size is the physical pixel size of the window, which is
     larger than the logical size of the screen on a high density display; the
     screen texture is stretched over it by the present quad. */
  if (!present_.configured) {
    int pixel_width = Width();
    int pixel_height = Height();
    SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height);

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(GPUDevice::Get().adapter(), &capabilities);

    /* Every render target of the engine stores RGBA8Unorm and holds sRGB
       encoded bytes, see kFS_PresentBase. Presenting those bytes into an
       *Srgb swapchain would let the hardware encode them a second time and
       wash the picture out towards white (the "everything looks too bright"
       symptom). Preferring an *Srgb target and decoding once in the present
       fragment stage keeps the two transfers exact inverses, so the pixel is
       reproduced as authored while staying in the hardware's preferred sRGB
       pipeline. */
    present_.format = capabilities.formats[0];
    for (uint32_t i = 0; i < capabilities.formatCount; ++i) {
      const auto candidate = capabilities.formats[i];
      if (candidate == wgpu::TextureFormat::BGRA8UnormSrgb ||
          candidate == wgpu::TextureFormat::RGBA8UnormSrgb) {
        present_.format = candidate;
        break;
      }
    }
    present_.srgb_target =
        present_.format == wgpu::TextureFormat::BGRA8UnormSrgb ||
        present_.format == wgpu::TextureFormat::RGBA8UnormSrgb;

    surface.Unconfigure();
    wgpu::SurfaceConfiguration configure;
    configure.device = GPUDevice::Get().device();
    configure.format = present_.format;
    configure.usage = wgpu::TextureUsage::RenderAttachment;
    configure.width = static_cast<uint32_t>(pixel_width);
    configure.height = static_cast<uint32_t>(pixel_height);
    configure.presentMode = wgpu::PresentMode::Fifo;
    surface.Configure(&configure);

    wgpu::PrimitiveState primitive;
    primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    /* On an sRGB target the present decodes the encoded texel back to linear so
       the hardware encode cancels it, see kFS_PresentBase. A non sRGB target
       has nothing to cancel, so the plain texture stage is used there. */
    present_.pipeline =
        present_.srgb_target
            ? ShaderSet::Get().shader.present_base.MakeState(
                  primitive, std::nullopt,
                  {wgpu::ColorTargetState{.format = present_.format}})
            : ShaderSet::Get().shader.texture_base.MakeState(
                  primitive, std::nullopt,
                  {wgpu::ColorTargetState{.format = present_.format}});

    present_.configured = true;
  }

  wgpu::SurfaceTexture surface_texture;
  surface.GetCurrentTexture(&surface_texture);
  auto surface_view = surface_texture.texture.CreateView(nullptr);

  /* The scene uniform of the screen maps its own logical size over the whole
     render target, see Bitmap::CreateGroup, so the quad is emitted in screen
     coordinates: a quad covering the screen covers the surface as well, at any
     pixel density. Using the surface size here would overshoot whenever the
     window is backed by more pixels than the screen is wide, which is exactly
     what the high density flag asks for. */
  present_.primitive.EmitQuad(RectI(0, 0, Width(), Height()), RectI(0, 0, 1, 1),
                              glm::vec4(1.0f));
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

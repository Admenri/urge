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

#include "core/graphics/graphics.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_timer.h"

#include "core/audio/audio.h"
#include "core/canvas/bitmap_queue.h"
#include "core/common/common.h"
#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/graphics/gpu.h"
#include "core/input/input.h"
#include "core/input/mouse.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/render/imgui_manager.h"
#include "core/render/pipeline.h"
#include "core/render/primitive.h"
#include "core/render/render_stats.h"
#include "core/render/uniform.h"
#include "core/scene/sprite_batch.h"
#include "core/system/config.h"

#include <cmath>

namespace urge {

namespace {

constexpr int32_t kMaxFrameRate = 240;

uint64_t NowNS() {
  return SDL_GetTicksNS();
}

wgpu::PresentMode PickPresentMode(const wgpu::SurfaceCapabilities& caps,
                                  bool vsync) {
  const auto supported = [&caps](wgpu::PresentMode mode) {
    for (size_t i = 0; i < caps.presentModeCount; ++i)
      if (caps.presentModes[i] == mode)
        return true;
    return false;
  };

  if (!vsync) {
    if (supported(wgpu::PresentMode::Immediate))
      return wgpu::PresentMode::Immediate;
    if (supported(wgpu::PresentMode::Mailbox))
      return wgpu::PresentMode::Mailbox;
  }

  return wgpu::PresentMode::Fifo;
}

/* Tells the host's loop how fast the game wants to be ticked.

   On the desktop SDL calls `SDL_AppIterate` as fast as it can, so the host has
   no clock of its own and the hint is what gives it one.  On the web the host
   already has the best clock there is -- the display's refresh -- and the hint
   must not be set: SDL answers it by switching the module off
   `requestAnimationFrame` and onto a timer, and a WebGPU canvas can only be
   presented from inside an animation frame.

   The point of setting it at all is that the frame loop then needs no platform
   branch: both hosts tick, and the loop decides how much game time belongs in
   a tick.  See bind/cruby_main.cc. */
void SyncHostTickRate(int32_t frame_rate) {
#if !defined(OS_EMSCRIPTEN)
  const std::string rate = std::to_string(std::max(frame_rate, 1));
  SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, rate.c_str());
#else
  (void)frame_rate;
#endif
}

}  // namespace

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
  return true;
}

void ScreenRootNode::PostDraw(DrawParam param) {
  if (!param->pass)
    return;

  auto pipeline = ShaderSet::Get().state.graphics.color_pma;
  param->pass.SetPipeline(pipeline);
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetBindGroup(1, param->target->object_group(), 0, nullptr);
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);
  param->pass.Draw(slot_.count, 1, slot_.first, 0);
}

Graphics::Graphics(URGE_EXCEPT)
    : frame_rate_(Config::Get().xp() ? 40 : 60) {
  auto& config = Config::Get();

  vsync_ = config.display.vsync;
  frame_skip_ = config.display.frame_skip;

  SyncHostTickRate(frame_rate_);

  auto window_flag =
      SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_HIDDEN;
  window_ = SDL_CreateWindow(config.game.title.c_str(), config.display.width,
                             config.display.height, window_flag);
  auto dpi = SDL_GetWindowDisplayScale(window_);
  SDL_SetWindowSize(window_, static_cast<int>(config.display.width * dpi),
                    static_cast<int>(config.display.height * dpi));
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window_);

  SDL_SetWindowFullscreen(window_, config.display.fullscreen);

  GPUDevice::Reset(new GPUDevice(exception_state, window_, config.gfx.backend));
  GPU::Reset(new GPU());
  ShaderSet::Reset(new ShaderSet());

  UniformManager::Reset(new UniformManager(exception_state));
  QuadVertexManager::Reset(new QuadVertexManager());
  SpriteBatch::Reset(new SpriteBatch());
  BitmapQueue::Reset(new BitmapQueue());

  root_ = MakeRefCounted<ScreenRootNode>();
  ResizeScreen(config.display.width, config.display.height);

  ImGuiManager::Reset(new ImGuiManager(window_));
}

Graphics::~Graphics() {
  ImGuiManager::Reset(nullptr);

  present_ = {};
  screen_.reset();
  root_.reset();

  BitmapQueue::Reset(nullptr);
  SpriteBatch::Reset(nullptr);
  QuadVertexManager::Reset(nullptr);
  UniformManager::Reset(nullptr);
  ShaderSet::Reset(nullptr);
  GPU::Reset(nullptr);
#if defined(OS_EMSCRIPTEN)
  /* wgpu-native's device destructor waits for the queue to drain and panics
     when that times out, which on the web is always: the module is being torn
     down and nothing can make progress any more.  The page is going away
     anyway, so the device is leaked rather than destroyed -- the alternative is
     an abort that hides whatever error got us here. */
  GPUDevice::Release();
#else
  GPUDevice::Reset(nullptr);
#endif

  SDL_DestroyWindow(window_);
}

void Graphics::Update(URGE_EXCEPT) {
  const uint64_t frame_begin = SDL_GetPerformanceCounter();

  RenderStats& stats = GetRenderStats();

  /* The script's share of the frame is the gap since the previous frame ended:
     `Graphics.update` returns into the game and the game comes back here.
     Everything measured below is the engine's share. */
  const double script_span =
      last_update_end_tick_ == 0
          ? 0.0
          : TicksToMilliseconds(frame_begin - last_update_end_tick_);
  logic_cost_ms_ = static_cast<float>(script_span);

  /* Bitmap work the script itself forced -- `Bitmap#get_pixel` has to drain
     the queue -- is already counted; it belongs to the script bucket rather
     than to the engine's, and is kept apart below so nothing is counted
     twice. */
  const double bitmap_before_update = stats.current.bitmap_total_ms();

  Audio::Get().Update();
  Input::Get().Update();
  Mouse::Get().Update();

  const uint64_t render_begin = SDL_GetPerformanceCounter();
  if (!frozen_)
    root_->Render(screen_, Color::Black());
  ++frame_count_;
  render_cost_ms_ = static_cast<float>(
      TicksToMilliseconds(SDL_GetPerformanceCounter() - render_begin));

  PresentInternal(exception_state);

  /* The between-frames hook runs inside the frame, before the pacer: whatever
     it costs is the frame's own work, which the pacer can then absorb, rather
     than time the next frame inherits.  On the web this is the garbage
     collection, and it is the only thing installed here. */
  if (frame_callback_)
    frame_callback_();

  /* The pacer runs at the very end, after the frame is complete: what it does
     -- sleep, hand the thread to a browser animation frame, or nothing at all
     -- is the host's business, and doing it here means the next call to
     `Update` starts a frame instead of finishing the previous one. */
  const uint64_t pace_begin = SDL_GetPerformanceCounter();
  if (present_callback_)
    present_callback_();
  const double delay_ms =
      TicksToMilliseconds(SDL_GetPerformanceCounter() - pace_begin);

  /* Read after the pacer returns.  The wait belongs to no frame: measuring the
     next frame's script share from the moment this one ended would count the
     idle time as script time, and the F2 panel would report the display's
     refresh rate as the game's own cost. */
  last_update_end_tick_ = SDL_GetPerformanceCounter();

  const double update_span =
      TicksToMilliseconds(last_update_end_tick_ - frame_begin);
  const double bitmap_total = stats.current.bitmap_total_ms();
  const double bitmap_in_update =
      std::max(0.0, bitmap_total - bitmap_before_update);

  other_cost_ms_ =
      static_cast<float>(std::max(0.0, update_span - render_cost_ms_));

  FrameTimings& timings = stats.timings;
  timings.total_ms = script_span + update_span;
  timings.set_phase(FramePhase::kScript, script_span - bitmap_before_update);
  timings.set_phase(FramePhase::kBitmap, bitmap_total);
  timings.set_phase(FramePhase::kPresent, present_cost_ms_);
  timings.set_phase(FramePhase::kUpdate, update_span - bitmap_in_update -
                                             present_cost_ms_ - delay_ms);

  const double measured =
      timings.phase(FramePhase::kScript) + timings.phase(FramePhase::kUpdate) +
      timings.phase(FramePhase::kBitmap) + timings.phase(FramePhase::kPresent);
  timings.set_phase(FramePhase::kOther, timings.total_ms - measured);

  stats.frame_history.Push(timings.total_ms);
  stats.timing_history.Push(timings);

  stats.EndFrame();

  if (Config::Get().HasCommandLine("bstats")) {
    const RenderFrameCounters& counters = stats.last;

    LOGGER_INFO(
        "bstats ops={} bitmap_submits={} frame_submits={} upload={:.3f} "
        "record={:.3f} finish={:.3f} submit={:.3f} bitmap={:.3f} render={:.3f} "
        "script={:.3f} update={:.3f} present={:.3f} other={:.3f} frame={:.3f}",
        counters.bitmap_ops, counters.bitmap_submits, counters.frame_submits,
        counters.bitmap_upload_ms, counters.bitmap_record_ms,
        counters.bitmap_finish_ms, counters.bitmap_submit_ms,
        counters.bitmap_total_ms(), render_cost_ms_,
        timings.phase(FramePhase::kScript), timings.phase(FramePhase::kUpdate),
        timings.phase(FramePhase::kPresent), timings.phase(FramePhase::kOther),
        timings.total_ms);
  }
}

void Graphics::Wait(URGE_EXCEPT, int32_t duration) {
  for (int32_t i = 0; i < duration; ++i)
    Update(exception_state);
}

void Graphics::Fadein(URGE_EXCEPT, int32_t duration) {
  duration = std::max(duration, 1);
  int32_t step = (255 - brightness_) / duration;
  for (int32_t i = 0; i < duration; ++i) {
    brightness_ += step;
    brightness_ = std::clamp<int32_t>(brightness_, 0, 255);

    Update(exception_state);
  }
  brightness_ = 255;
}

void Graphics::Fadeout(URGE_EXCEPT, int32_t duration) {
  duration = std::max(duration, 1);
  int32_t step = brightness_ / duration;
  for (int32_t i = 0; i < duration; ++i) {
    brightness_ -= step;
    brightness_ = std::clamp<int32_t>(brightness_, 0, 255);

    Update(exception_state);
  }
  brightness_ = 0;
}

void Graphics::Freeze(URGE_EXCEPT) {
  if (!frozen_) {
    root_->Render(screen_, Color::Black());
    frozen_ = true;
  }
}

void Graphics::Transition(URGE_EXCEPT, int32_t duration,
                          std::string filename,
                          int32_t vague) {
  RefPtr<Bitmap> mapping = nullptr;
  if (!filename.empty())
    mapping = MakeRefCounted<Bitmap>(exception_state, filename);

  TransitionBitmap(exception_state, duration, mapping, vague);
}

void Graphics::TransitionBitmap(URGE_EXCEPT, int32_t duration,
                                RefPtr<Bitmap> bitmap,
                                int32_t vague) {
  if (!frozen_)
    return;

  brightness_ = 255;

  const RefPtr<Bitmap> frozen_scene = MakeRefCounted<Bitmap>(screen_);
  const RefPtr<Bitmap> current_scene = SnapToBitmap();

  const bool mapped = bitmap != nullptr;
  const wgpu::RenderPipeline pipeline =
      mapped ? ShaderSet::Get().state.graphics.transition_vague
             : ShaderSet::Get().state.graphics.transition_alpha;

  std::vector<std::pair<uint32_t, util::BindingSetType>> bindings = {
      {0, util::TextureViewSet(frozen_scene->texture_view())},
      {1, util::SamplerSet(frozen_scene->sampler())},
      {2, util::TextureViewSet(current_scene->texture_view())},
      {3, util::SamplerSet(current_scene->sampler())}};
  if (mapped) {
    bindings.push_back({4, util::TextureViewSet(bitmap->texture_view())});
    bindings.push_back({5, util::SamplerSet(bitmap->sampler())});
  }
  const wgpu::BindGroup scene_textures =
      util::CreateBindGroup(pipeline.GetBindGroupLayout(2), bindings);

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
      auto pass = screen_->BeginRendering(encoder);
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
    CountSubmit(false);
    GPUDevice::Get().queue().Submit(1, &command);

    Update(exception_state);
  }

  frozen_ = false;
}

RefPtr<Bitmap> Graphics::SnapToBitmap() {
  auto result = MakeRefCounted<Bitmap>(Width(), Height());
  root_->Render(result, Color::Black());
  return result;
}

void Graphics::FrameReset() {
  RequestFrameReset();
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

  auto dpi = SDL_GetWindowDisplayScale(window_);
  SDL_SetWindowSize(window_, width * dpi, height * dpi);
  SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
                        SDL_WINDOWPOS_CENTERED);
}

void Graphics::PlayMovie(URGE_EXCEPT, std::string filename) {
  exception_state.Throw(ExceptionState::kRGSSError,
                        "unimplement video playback.");
  return;}

ATTR_DEF(Graphics, int32_t, FrameRate) {
  if (value.has_value()) {
    /* Zero would make the pacer divide by it; anything above the ceiling is a
       script asking for a rate no display can honour. */
    frame_rate_ = std::clamp(*value, 1, kMaxFrameRate);
    /* The host ticks at the rate the game asks for, so a rate change has to
       reach it or the game would be capped at the old one. */
    SyncHostTickRate(frame_rate_);
    RequestFrameReset();
    return std::nullopt;
  } else {
    return frame_rate_;
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

ATTR_DEF(Graphics, bool, FrameSkip) {
  if (value.has_value()) {
    frame_skip_ = *value;
    RequestFrameReset();
    return std::nullopt;
  } else {
    return frame_skip_;
  }
}

ATTR_DEF(Graphics, bool, Fullscreen) {
  if (value.has_value()) {
    SDL_SetWindowFullscreen(window_, *value);

    present_.configured = false;
    return std::nullopt;
  } else {
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
  }
}

ATTR_DEF(Graphics, bool, VSync) {
  if (value.has_value()) {
    if (vsync_ != *value) {
      vsync_ = *value;

      present_.configured = false;
    }
    return std::nullopt;
  } else {
    return vsync_;
  }
}

void Graphics::PushEvent(const SDL_Event& event) {
  pending_events_.push_back(event);
}

void Graphics::ProcessEvents(URGE_EXCEPT) {
  auto& imgui = ImGuiManager::Get();

  bool quit = false;
  for (SDL_Event& event : pending_events_) {
    imgui.ProcessEvent(event);

    Input::Get().ProcessGamepadEvents(&event);

    if (!imgui.WantsGameInputBlocked()) {
      Input::Get().ProcessEvents(exception_state, &event);
      Mouse::Get().ProcessEvents(exception_state, &event);
    }

    if (event.type == SDL_EVENT_QUIT)
      quit = true;
    else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
      present_.configured = false;
  }
  pending_events_.clear();

  Input::Get().SetBlocked(imgui.WantsGameInputBlocked());

  /* The window was closed.  It is reported as the exit the script already
     knows -- `Graphics.update` raising the same way `exit` does -- so `ensure`
     blocks still run, rather than the loop being torn down underneath them.
     Checked after the whole batch, because the events that came with the quit
     still belong to this frame. */
  if (quit)
    exception_state.Throw(ExceptionState::kExitError, {});
}

void Graphics::PresentInternal(URGE_EXCEPT) {
  auto surface = GPUDevice::Get().swapchain();
  auto& imgui = ImGuiManager::Get();

  ProcessEvents(exception_state);

  if (!present_.configured) {
    int pixel_width = Width();
    int pixel_height = Height();
    SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height);

    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(GPUDevice::Get().adapter(), &capabilities);

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
    configure.presentMode = PickPresentMode(capabilities, vsync_);
    surface.Configure(&configure);

    wgpu::PrimitiveState primitive;
    primitive.topology = wgpu::PrimitiveTopology::TriangleList;

    present_.pipeline =
        present_.srgb_target
            ? ShaderSet::Get().shader.present_base.MakeState(
                  primitive, std::nullopt,
                  {wgpu::ColorTargetState{.format = present_.format}})
            : ShaderSet::Get().shader.texture_base.MakeState(
                  primitive, std::nullopt,
                  {wgpu::ColorTargetState{.format = present_.format}});

    imgui.SetRenderTargetFormat(present_.format);

    present_.configured = true;
  }

  imgui.Update();

  const uint64_t present_begin = SDL_GetPerformanceCounter();

  wgpu::SurfaceTexture surface_texture;
  surface.GetCurrentTexture(&surface_texture);
  switch (surface_texture.status) {
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal:
    case wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal:
      break;
    case wgpu::SurfaceGetCurrentTextureStatus::Timeout:
    case wgpu::SurfaceGetCurrentTextureStatus::Outdated:
    case wgpu::SurfaceGetCurrentTextureStatus::Lost:
      present_.configured = false;
      present_cost_ms_ =
          static_cast<double>(SDL_GetPerformanceCounter() - present_begin) *
          1000.0 / SDL_GetPerformanceFrequency();
      return;
    case wgpu::SurfaceGetCurrentTextureStatus::Error:
      exception_state.Throw(ExceptionState::kRGSSError,
                            "failed to acquire surface texture.");
      return;      break;
  }

  present_.primitive.EmitQuad(RectI(0, 0, Width(), Height()), RectI(0, 0, 1, 1),
                              glm::vec4(1.0f));
  const std::uint32_t vertex_count = present_.primitive.Upload();

  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);
  {
    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = surface_texture.texture.CreateView(nullptr);
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

      imgui.RenderDrawData(pass);
    }
    pass.End();
  }
  auto command = encoder.Finish(nullptr);
  CountSubmit(false);
  GPUDevice::Get().queue().Submit(1, &command);

#if defined(OS_EMSCRIPTEN)
  /* The canvas is presented when the animation frame callback returns, so there
     is nothing to hand over here -- and emdawnwebgpu does not implement
     wgpuSurfacePresent at all ("TODO: This could probably be emulated with
     ASYNCIFY"), it aborts.  The pacer installed on `Graphics` is what puts the
     frame back inside a callback: it yields at the end of `Update`, the
     callback returns, and the browser presents what was submitted above. */
#else
  surface.Present();
#endif

  present_cost_ms_ =
      static_cast<double>(SDL_GetPerformanceCounter() - present_begin) *
      1000.0 / SDL_GetPerformanceFrequency();
}

}

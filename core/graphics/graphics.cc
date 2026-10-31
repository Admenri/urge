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
#include "SDL3/SDL_system.h"
#include "SDL3/SDL_timer.h"

#include "core/audio/audio.h"
#include "core/canvas/bitmap_queue.h"
#include "core/common/common.h"
#if defined(SDL_PLATFORM_ANDROID)
#include <jni.h>
#endif

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/graphics/gpu.h"
#include "core/input/input.h"
#include "core/input/mouse.h"
#include "core/input/text_input.h"
#include "core/input/touch.h"
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

RectF FitScreenRect(int32_t surface_width,
                    int32_t surface_height,
                    int32_t screen_width,
                    int32_t screen_height) {
  if (surface_width <= 0 || surface_height <= 0)
    return RectF(0.0f, 0.0f, static_cast<float>(std::max(surface_width, 0)),
                 static_cast<float>(std::max(surface_height, 0)));

  if (screen_width <= 0 || screen_height <= 0)
    return RectF(0.0f, 0.0f, static_cast<float>(surface_width),
                 static_cast<float>(surface_height));

  const float scale = std::min(
      static_cast<float>(surface_width) / static_cast<float>(screen_width),
      static_cast<float>(surface_height) / static_cast<float>(screen_height));

  const float width = static_cast<float>(screen_width) * scale;
  const float height = static_cast<float>(screen_height) * scale;

  return RectF((static_cast<float>(surface_width) - width) * 0.5f,
               (static_cast<float>(surface_height) - height) * 0.5f, width,
               height);
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

void SyncHostTickRate(int32_t frame_rate) {
#if defined(URGE_RUNNER_FIBER) && !defined(OS_EMSCRIPTEN)
  const std::string rate = std::to_string(std::max(frame_rate, 1));
  SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, rate.c_str());
#else
  (void)frame_rate;
#endif
}

}

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

Graphics::Graphics(URGE_EXCEPT) : frame_rate_(Config::Get().xp() ? 40 : 60) {
  auto& config = Config::Get();

  vsync_ = config.display.vsync;
  frame_skip_ = config.display.frame_skip;

  SyncHostTickRate(frame_rate_);
  limiter_.SetRate(frame_rate_);

  auto window_flag =
      SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_HIDDEN;
  if (config.display.resizable != WindowResizable::kNone)
    window_flag |= SDL_WINDOW_RESIZABLE;

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
  GPUDevice::Release();
#else
  GPUDevice::Reset(nullptr);
#endif

  SDL_DestroyWindow(window_);
}

void Graphics::Update(URGE_EXCEPT) {
  const uint64_t frame_begin = SDL_GetPerformanceCounter();
  frame_begin_tick_ = frame_begin;

  SetCurrentFramePhase(FramePhase::kUpdate);

  RenderStats& stats = GetRenderStats();

  const double script_span =
      last_update_end_tick_ == 0
          ? 0.0
          : TicksToMilliseconds(frame_begin - last_update_end_tick_);
  logic_cost_ms_ = static_cast<float>(script_span);

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

  if (frame_callback_)
    frame_callback_();

  const uint64_t pace_begin = SDL_GetPerformanceCounter();
  if (present_callback_)
    present_callback_();
  const double delay_ms =
      TicksToMilliseconds(SDL_GetPerformanceCounter() - pace_begin) +
      pace_delay_ms_;
  pace_delay_ms_ = 0.0;

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
  stats.run.Add(timings);

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

  SetCurrentFramePhase(FramePhase::kScript);
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

void Graphics::Transition(URGE_EXCEPT,
                          int32_t duration,
                          std::string filename,
                          int32_t vague) {
  RefPtr<Bitmap> mapping = nullptr;
  if (!filename.empty())
    mapping = MakeRefCounted<Bitmap>(exception_state, filename);

  TransitionBitmap(exception_state, duration, mapping, vague);
}

void Graphics::TransitionBitmap(URGE_EXCEPT,
                                int32_t duration,
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

#if !defined(OS_ANDROID)
  if (Config::Get().display.resizable == WindowResizable::kNone) {
    auto dpi = SDL_GetWindowDisplayScale(window_);
    SDL_SetWindowSize(window_, width * dpi, height * dpi);
    SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED);
  }
#endif

  ApplyAspectConstraint();
}

void Graphics::ApplyAspectConstraint() {
  if (!window_)
    return;

  if (Config::Get().display.resizable != WindowResizable::kRatio) {
    SDL_SetWindowAspectRatio(window_, 0.0f, 0.0f);
    return;
  }

  const int32_t width = Width();
  const int32_t height = Height();
  if (width <= 0 || height <= 0)
    return;

  const float aspect = static_cast<float>(width) / static_cast<float>(height);
  SDL_SetWindowAspectRatio(window_, aspect, aspect);
}

void Graphics::PlayMovie(URGE_EXCEPT, std::string filename) {
  exception_state.Throw(ExceptionState::kRGSSError,
                        "unimplement video playback.");
  return;
}

ATTR_DEF(Graphics, int32_t, FrameRate) {
  if (value.has_value()) {
    frame_rate_ = std::clamp(*value, 1, kMaxFrameRate);
    SyncHostTickRate(frame_rate_);
    limiter_.SetRate(frame_rate_);
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
    pending_fullscreen_ = *value;
    QueueHostTask(
        [this, wanted = *value] { SDL_SetWindowFullscreen(window_, wanted); });

    present_.configured = false;
    return std::nullopt;
  } else {
    if (pending_fullscreen_.has_value())
      return *pending_fullscreen_;
    return (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
  }
}

void Graphics::QueueHostTask(std::function<void()> task) {
  host_tasks_.emplace_back(std::move(task));
}

void Graphics::RunHostTasks() {
#if defined(SDL_PLATFORM_ANDROID)
  if (auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv())) {
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      if (!exception_dropped_) {
        exception_dropped_ = true;
        LOGGER_WARN(
            "[Host] dropping a pending Java exception raised on the loop "
            "stack");
      }
    }
  }
#endif

  if (host_tasks_.empty())
    return;

  auto tasks = std::move(host_tasks_);
  host_tasks_.clear();

  LOGGER_INFO("[Host] running {} deferred platform task(s)", tasks.size());
  for (auto& task : tasks)
    task();

  pending_fullscreen_.reset();
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

#if defined(URGE_RUNNER_POLL)
  SDL_Event polled;
  while (SDL_PollEvent(&polled))
    pending_events_.push_back(polled);
#endif

  bool quit = false;
  for (SDL_Event& event : pending_events_) {
    imgui.ProcessEvent(event);

    Input::Get().ProcessGamepadEvents(&event);

    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
        event.type == SDL_EVENT_DID_ENTER_BACKGROUND)
      Touch::Get().CancelAll();

    if (!imgui.WantsGameInputBlocked()) {
      Input::Get().ProcessEvents(exception_state, &event);
      Mouse::Get().ProcessEvents(exception_state, &event);
      Touch::Get().ProcessEvents(exception_state, &event);
      if (auto* text_input = TextInput::TryGet())
        text_input->ProcessEvent(&event);
    }

    if (event.type == SDL_EVENT_QUIT
#if defined(URGE_RUNNER_POLL)
        || event.type == SDL_EVENT_TERMINATING
#endif
    )
      quit = true;
    else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
      present_.configured = false;
  }
  pending_events_.clear();

  Input::Get().SetBlocked(imgui.WantsGameInputBlocked());

  if (quit)
    exception_state.Throw(ExceptionState::kExitError, {});
}

void Graphics::PresentInternal(URGE_EXCEPT) {
  auto& device = GPUDevice::Get();
  auto& imgui = ImGuiManager::Get();

  SetCurrentFramePhase(FramePhase::kPresent);

  ProcessEvents(exception_state);

  if (!device.RefreshSurface()) {
    present_.configured = false;
    return;
  }

  int pixel_width = Width();
  int pixel_height = Height();
  SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height);
  if (present_.surface_generation != device.surface_generation() ||
      present_.pixel_width != pixel_width ||
      present_.pixel_height != pixel_height)
    present_.configured = false;

  auto surface = device.swapchain();

  if (!present_.configured) {
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(device.adapter(), &capabilities);

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
    configure.device = device.device();
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

    present_.surface_generation = device.surface_generation();
    present_.pixel_width = pixel_width;
    present_.pixel_height = pixel_height;
    present_.configured = true;
  }

  imgui.Update();

  const RectF viewport = ScreenRectInPixels();
  if (viewport.width < 1.0f || viewport.height < 1.0f)
    return;

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
      return;
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
    color_attachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};
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
      pass.SetViewport(viewport.x, viewport.y, viewport.width, viewport.height,
                       0.0f, 1.0f);
      pass.Draw(vertex_count, 1, 0, 0);

      imgui.RenderDrawData(pass);
    }
    pass.End();
  }
  auto command = encoder.Finish(nullptr);
  CountSubmit(false);
  GPUDevice::Get().queue().Submit(1, &command);

#if !defined(OS_EMSCRIPTEN)
  surface.Present();
#endif

  present_cost_ms_ =
      static_cast<double>(SDL_GetPerformanceCounter() - present_begin) *
      1000.0 / SDL_GetPerformanceFrequency();

#if defined(URGE_RUNNER_POLL)
  SetCurrentFramePhase(FramePhase::kOther);
  RunHostTasks();

  pace_delay_ms_ = limiter_.Wait(frame_begin_tick_);
  SetCurrentFramePhase(FramePhase::kUpdate);
#endif
}

RectF Graphics::ScreenRectInWindow() const {
  const int32_t screen_width = screen_ ? screen_->size().x : 0;
  const int32_t screen_height = screen_ ? screen_->size().y : 0;

  int32_t window_width = 0, window_height = 0;
  if (!window_ || !SDL_GetWindowSize(window_, &window_width, &window_height))
    return RectF(0.0f, 0.0f, static_cast<float>(screen_width),
                 static_cast<float>(screen_height));

  return FitScreenRect(window_width, window_height, screen_width,
                       screen_height);
}

RectF Graphics::ScreenRectInPixels() const {
  const int32_t screen_width = screen_ ? screen_->size().x : 0;
  const int32_t screen_height = screen_ ? screen_->size().y : 0;

  int32_t pixel_width = 0, pixel_height = 0;
  if (!window_ ||
      !SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height))
    return RectF(0.0f, 0.0f, static_cast<float>(screen_width),
                 static_cast<float>(screen_height));

  return FitScreenRect(pixel_width, pixel_height, screen_width, screen_height);
}

glm::vec2 Graphics::WindowToScreen(float x, float y) const {
  const RectF rect = ScreenRectInWindow();
  if (rect.width <= 0.0f || rect.height <= 0.0f)
    return glm::vec2(x, y);

  const float screen_width =
      screen_ ? static_cast<float>(screen_->size().x) : 0.0f;
  const float screen_height =
      screen_ ? static_cast<float>(screen_->size().y) : 0.0f;

  return glm::vec2((x - rect.x) * screen_width / rect.width,
                   (y - rect.y) * screen_height / rect.height);
}

glm::vec2 Graphics::ScreenToWindow(float x, float y) const {
  const RectF rect = ScreenRectInWindow();
  const float screen_width =
      screen_ ? static_cast<float>(screen_->size().x) : 0.0f;
  const float screen_height =
      screen_ ? static_cast<float>(screen_->size().y) : 0.0f;
  if (screen_width <= 0.0f || screen_height <= 0.0f)
    return glm::vec2(x, y);

  return glm::vec2(x * rect.width / screen_width + rect.x,
                   y * rect.height / screen_height + rect.y);
}

glm::vec2 Graphics::WindowToScreenDelta(float dx, float dy) const {
  const RectF rect = ScreenRectInWindow();
  if (rect.width <= 0.0f || rect.height <= 0.0f)
    return glm::vec2(dx, dy);

  const float screen_width =
      screen_ ? static_cast<float>(screen_->size().x) : 0.0f;
  const float screen_height =
      screen_ ? static_cast<float>(screen_->size().y) : 0.0f;

  return glm::vec2(dx * screen_width / rect.width,
                   dy * screen_height / rect.height);
}

glm::vec2 Graphics::NormalizedToScreen(float nx, float ny) const {
  int32_t window_width = 0, window_height = 0;
  if (!window_ || !SDL_GetWindowSize(window_, &window_width, &window_height))
    return glm::vec2(nx, ny);

  return WindowToScreen(nx * static_cast<float>(window_width),
                        ny * static_cast<float>(window_height));
}

glm::vec2 Graphics::NormalizedToScreenDelta(float nx, float ny) const {
  int32_t window_width = 0, window_height = 0;
  if (!window_ || !SDL_GetWindowSize(window_, &window_width, &window_height))
    return glm::vec2(nx, ny);

  return WindowToScreenDelta(nx * static_cast<float>(window_width),
                             ny * static_cast<float>(window_height));
}

}

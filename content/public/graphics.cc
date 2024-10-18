// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/graphics.h"

#include "components/fpslimiter/fpslimiter.h"
#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/input.h"

#include "SDL3/SDL_timer.h"
#include "fiber/fiber.h"

namespace content {

Graphics::Graphics(CoroutineContext* cc,
                   base::WeakPtr<ui::Widget> window,
                   std::unique_ptr<ScopedFontData> default_font,
                   const base::Vec2i& initial_resolution,
                   APIVersion api_diff)
    : api_version_(api_diff),
      window_size_(window->GetSize()),
      static_font_manager_(std::move(default_font)),
      resolution_(initial_resolution),
      frozen_(false),
      brightness_(255),
      frame_count_(0),
      frame_rate_(api_diff >= APIVersion ::RGSS2 ? 60 : 40),
      vsync_(0),
      elapsed_time_(0),
      smooth_delta_time_(1),
      last_count_time_(SDL_GetPerformanceCounter()),
      desired_delta_time_(SDL_GetPerformanceFrequency() / frame_rate_),
      cc_(cc) {
  // Initialize root viewport
  viewport_rect().rect = initial_resolution;
  viewport_rect().has_scissor = false;

  // Create render device
  renderer_ = renderer::RenderDevice::Create(
      window, renderer::RenderDevice::RendererBackend::kD3D12);

  // Create renderer buffer
  screen_quad_ = std::make_unique<renderer::QuadDrawable>(
      renderer_->device(), renderer_->quad_index_buffer());

  // Create virtual screen buffer
  RebuildScreenBufferInternal(initial_resolution);
}

Graphics::~Graphics() {
  screen_quad_.reset();
  screen_buffer_.Release();
  frozen_snapshot_.Release();

  renderer_.reset();
}

bool Graphics::ExecuteEventMainLoop() {
  // Poll event queue
  SDL_Event queued_event;
  while (SDL_PollEvent(&queued_event)) {
    // Quit event
    if (queued_event.type == SDL_EVENT_QUIT)
      return false;
  }

  // Determine update repeat time
  const uint64_t now_time = SDL_GetPerformanceCounter();
  const uint64_t delta_time = now_time - last_count_time_;
  last_count_time_ = now_time;

  // Calculate smooth frame rate
  const double delta_rate =
      delta_time / static_cast<double>(desired_delta_time_);
  const int repeat_time = DetermineRepeatNumberInternal(delta_rate);

  for (int i = 0; i < repeat_time; ++i)
    fiber_switch(cc_->main_loop_fiber);

  return true;
}

int Graphics::GetBrightness() const {
  return brightness_;
}

void Graphics::SetBrightness(int brightness) {
  brightness = std::clamp(brightness, 0, 255);
  brightness_ = brightness;
}

void Graphics::Wait(int duration) {
  for (int i = 0; i < duration; ++i)
    Update();
}

scoped_refptr<Bitmap> Graphics::SnapToBitmap() {
  scoped_refptr<Bitmap> snap = new Bitmap(this, resolution_);

  return snap;
}

void Graphics::FadeOut(int duration) {
  duration = std::max(duration, 1);

  float current_brightness = static_cast<float>(brightness_);
  for (int i = 0; i < duration; ++i) {
    SetBrightness(current_brightness -
                  current_brightness * (i / static_cast<float>(duration)));
    if (frozen_) {
      FrameProcessInternal();
    } else {
      Update();
    }
  }

  /* Set final brightness */
  SetBrightness(0);
}

void Graphics::FadeIn(int duration) {
  duration = std::max(duration, 1);

  float current_brightness = static_cast<float>(brightness_);
  float diff = 255.0f - current_brightness;
  for (int i = 0; i < duration; ++i) {
    SetBrightness(current_brightness +
                  diff * (i / static_cast<float>(duration)));

    if (frozen_) {
      FrameProcessInternal();
    } else {
      Update();
    }
  }

  /* Set final brightness */
  SetBrightness(255);
}

void Graphics::Update() {
  if (!frozen_) {
    // Setup render frame

    auto* RTV = renderer()->swapchain()->GetCurrentBackBufferRTV();
    float ClearColor[] = {1, 1, 1, 1};
    renderer()->context()->SetRenderTargets(
        1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    renderer()->context()->ClearRenderTarget(
        RTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    renderer()->swapchain()->Present();
  }

  // Process frame delay
  FrameProcessInternal();
}

void Graphics::ResizeScreen(const base::Vec2i& resolution) {
  if (!(resolution_ == resolution))
    RebuildScreenBufferInternal(resolution);
}

void Graphics::Reset() {
  /* Reset freeze */
  frozen_ = false;

  /* Disposed all elements */
  for (auto it = disposable_elements_.tail(); it != disposable_elements_.end();
       it = it->previous()) {
    it->value()->Dispose();
  }

  /* Reset attribute */
  SetFrameRate(api_version() >= APIVersion::RGSS2 ? 60 : 40);
  SetBrightness(255);
  ResetFrame();
}

void Graphics::Freeze() {
  if (frozen_)
    return;

  frozen_ = true;
}

void Graphics::Transition(int duration,
                          scoped_refptr<Bitmap> trans_bitmap,
                          int vague) {
  if (!frozen_)
    return;

  SetBrightness(255);
  vague = std::clamp<int>(vague, 1, 256);

  // Transition process complete
  frozen_ = false;
}

void Graphics::SetFrameRate(int rate) {
  rate = std::max(rate, 1);
  frame_rate_ = rate;
}

int Graphics::GetFrameRate() const {
  return frame_rate_;
}

void Graphics::SetFrameCount(int64_t count) {
  frame_count_ = count;
}

int Graphics::GetFrameCount() const {
  return frame_count_;
}

void Graphics::ResetFrame() {
  frame_count_ = 0;
}

void Graphics::ResizeWindow(int width, int height) {
  auto* win = renderer()->window()->AsSDLWindow();

  SDL_SetWindowSize(win, width, height);
  SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
}

bool Graphics::GetFullscreen() {
  return renderer()->window()->IsFullscreen();
}

void Graphics::SetFullscreen(bool fullscreen) {
  renderer()->window()->SetFullscreen(fullscreen);
}

void Graphics::SetDrawableOffset(const base::Vec2i& offset) {
  viewport_rect().rect.x = offset.x;
  viewport_rect().rect.y = offset.y;
  DrawableParent::NotifyViewportRectChanged();
}

base::Vec2i Graphics::GetDrawableOffset() {
  return viewport_rect().rect.Position();
}

void Graphics::RebuildScreenBufferInternal(const base::Vec2i& resolution) {
  resolution_ = resolution;

  Diligent::TextureDesc TexDesc;
  TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  TexDesc.Format = Diligent::TEX_FORMAT_RGBA8_UNORM_SRGB;
  TexDesc.Usage = Diligent::USAGE_DYNAMIC;
  TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_NONE;
  TexDesc.BindFlags =
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;
  TexDesc.Width = resolution_.x;
  TexDesc.Height = resolution_.y;

  TexDesc.Name = "Screen framebuffer";
  renderer()->device()->CreateTexture(TexDesc, nullptr, &screen_buffer_);

  TexDesc.Name = "Frozen framebuffer";
  renderer()->device()->CreateTexture(TexDesc, nullptr, &frozen_snapshot_);
}

void Graphics::FrameProcessInternal() {
  /* Increase frame render count */
  ++frame_count_;

  /* Switch to primary fiber */
  fiber_switch(cc_->primary_fiber);
}

int Graphics::DetermineRepeatNumberInternal(double delta_rate) {
  smooth_delta_time_ *= 0.8;
  smooth_delta_time_ += std::fmin(delta_rate, 2) * 0.2;

  if (smooth_delta_time_ >= 0.9) {
    elapsed_time_ = 0;
    return std::round(elapsed_time_);
  } else {
    elapsed_time_ += delta_rate;
    if (elapsed_time_ >= 1) {
      elapsed_time_ -= 1;
      return 1;
    }
  }

  return 0;
};

void Graphics::AddDisposable(Disposable* disp) {
  disposable_elements_.Append(disp->disposable_link());
}

void Graphics::RemoveDisposable(Disposable* disp) {
  disp->disposable_link()->RemoveFromList();
}

void Graphics::UpdateWindowViewportInternal() {
  auto window_size = renderer()->window()->GetSize();

  if (!(window_size == window_size_)) {
    window_size_ = window_size;
    renderer()->swapchain()->Resize(window_size_.x, window_size_.y);
  }

  float window_ratio = static_cast<float>(window_size.x) / window_size.y;
  float screen_ratio = static_cast<float>(resolution_.x) / resolution_.y;

  display_viewport_.width = window_size.x;
  display_viewport_.height = window_size.y;

  if (screen_ratio > window_ratio)
    display_viewport_.height = display_viewport_.width / screen_ratio;
  else if (screen_ratio < window_ratio)
    display_viewport_.width = display_viewport_.height * screen_ratio;

  display_viewport_.x = (window_size.x - display_viewport_.width) / 2.0f;
  display_viewport_.y = (window_size.y - display_viewport_.height) / 2.0f;
}

}  // namespace content

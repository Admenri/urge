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

#pragma once

#include <functional>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/graphics/frame_limiter.h"
#include "core/render/primitive.h"
#include "core/render/shader.h"
#include "core/scene/node.h"

namespace urge {

class ScreenRootNode : public Node {
 public:
  ScreenRootNode();

 private:
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void PostDraw(DrawParam param) override;

  PrimitiveEmitter::Slot slot_ = {};
};

URGE_BINDING()
class Graphics : public Singleton<Graphics> {
 public:
  Graphics(URGE_EXCEPT);
  ~Graphics();

  URGE_BINDING()
  void Update(URGE_EXCEPT);
  URGE_BINDING()
  void Wait(URGE_EXCEPT, int32_t duration);
  URGE_BINDING()
  void Fadein(URGE_EXCEPT, int32_t duration);
  URGE_BINDING()
  void Fadeout(URGE_EXCEPT, int32_t duration);
  URGE_BINDING()
  void Freeze(URGE_EXCEPT);
  URGE_BINDING()
  void Transition(URGE_EXCEPT,
                  int32_t duration = 10,
                  std::string filename = {},
                  int32_t vague = 40);
  URGE_BINDING()
  void TransitionBitmap(URGE_EXCEPT,
                        int32_t duration = 10,
                        MaybePtr<Bitmap> bitmap = {},
                        int32_t vague = 40);
  URGE_BINDING()
  RefPtr<Bitmap> SnapToBitmap();
  URGE_BINDING()
  void FrameReset();
  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();
  URGE_BINDING()
  void ResizeScreen(int32_t width, int32_t height);
  URGE_BINDING()
  void PlayMovie(URGE_EXCEPT, std::string filename);

  URGE_BINDING()
  ATTR(int32_t, FrameRate);
  URGE_BINDING()
  ATTR(int32_t, FrameCount);
  URGE_BINDING()
  ATTR(int32_t, Brightness);
  URGE_BINDING()
  ATTR(bool, FrameSkip);
  URGE_BINDING()
  ATTR(bool, Fullscreen);

  void QueueHostTask(std::function<void()> task);

  void RunHostTasks();
  URGE_BINDING()
  ATTR(bool, VSync);

  RefPtr<ScreenRootNode> root() { return root_; }
  SDL_Window* window() const { return window_; }

  RectF ScreenRectInWindow() const;

  RectF ScreenRectInPixels() const;

  glm::vec2 WindowToScreen(float x, float y) const;
  glm::vec2 ScreenToWindow(float x, float y) const;
  glm::vec2 WindowToScreenDelta(float dx, float dy) const;

  glm::vec2 NormalizedToScreen(float nx, float ny) const;
  glm::vec2 NormalizedToScreenDelta(float nx, float ny) const;

  using FrameCallback = void (*)();
  void SetFrameCallback(FrameCallback callback) { frame_callback_ = callback; }

  using FramePresentCallback = void (*)();
  void SetPresentCallback(FramePresentCallback callback) {
    present_callback_ = callback;
  }

  void RequestFrameReset() { frame_reset_requested_ = true; }
  bool ConsumeFrameReset() {
    const bool requested = frame_reset_requested_;
    frame_reset_requested_ = false;
    return requested;
  }

  void PushEvent(const SDL_Event& event);

  int32_t frame_rate() const { return frame_rate_; }
  bool frame_skip() const { return frame_skip_; }

 private:
  friend class ScreenRootNode;

  void ProcessEvents(URGE_EXCEPT);
  void PresentInternal(URGE_EXCEPT);

  void ApplyAspectConstraint();

  FrameCallback frame_callback_ = nullptr;
  FramePresentCallback present_callback_ = nullptr;
  bool frame_reset_requested_ = false;

  std::vector<SDL_Event> pending_events_;

  SDL_Window* window_ = nullptr;
  RefPtr<ScreenRootNode> root_;
  RefPtr<Bitmap> screen_;

  bool frozen_ = false;
  int32_t frame_rate_ = 60;
  int32_t frame_count_ = 0;
  int32_t brightness_ = 255;
  bool frame_skip_ = true;
  bool vsync_ = true;

  uint64_t last_update_end_tick_ = 0;
  float render_cost_ms_ = 0.0f;
  float logic_cost_ms_ = 0.0f;
  float other_cost_ms_ = 0.0f;
  double present_cost_ms_ = 0.0;

  PrimitiveEmitter quad_emitter_;

  FrameLimiter limiter_;

  uint64_t frame_begin_tick_ = 0;

  double pace_delay_ms_ = 0.0;

  std::vector<std::function<void()>> host_tasks_;

  std::optional<bool> pending_fullscreen_;

  bool exception_dropped_ = false;

  struct {
    bool configured = false;

    uint64_t surface_generation = 0;
    int32_t pixel_width = 0;
    int32_t pixel_height = 0;

    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;

    bool srgb_target = false;
    wgpu::RenderPipeline pipeline;
    PrimitiveEmitter primitive;
  } present_;
};

}

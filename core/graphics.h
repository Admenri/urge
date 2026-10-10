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

#include "SDL3/SDL_video.h"

#include "core/bitmap.h"
#include "core/definition.h"
#include "core/node.h"
#include "core/object.h"
#include "core/primitive.h"
#include "core/shader.h"

namespace urge {

class FPSLimiter {
 public:
  FPSLimiter(int frame_rate);

  FPSLimiter(const FPSLimiter&) = delete;
  FPSLimiter& operator=(const FPSLimiter&) = delete;

  void SetDisabled(bool disable);
  bool IsDisabled() const { return disabled_; }

  void SetFrameRate(int frame_rate);
  void Delay();
  bool RequireFrameSkip();
  void Reset();
  void Synchronize();

 private:
  bool disabled_;
  uint64_t last_tick_count_;
  int64_t ticks_per_frame_;
  const uint64_t tick_freq_;
  const double tick_freq_ns_;
  uint64_t skip_last_;
  int64_t skip_ideal_diff_;
  bool skip_reset_flag_;
};

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
  Graphics();
  ~Graphics();

  URGE_BINDING()
  void Update();
  URGE_BINDING()
  void Wait(int32_t duration);
  URGE_BINDING()
  void Fadein(int32_t duration);
  URGE_BINDING()
  void Fadeout(int32_t duration);
  URGE_BINDING()
  void Freeze();
  URGE_BINDING()
  void Transition(int32_t duration = 10,
                  std::string filename = {},
                  int32_t vague = 40);
  URGE_BINDING()
  void TransitionBitmap(int32_t duration = 10,
                        RefPtr<Bitmap> bitmap = {},
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
  void PlayMovie(std::string filename);

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
  URGE_BINDING()
  ATTR(bool, VSync);

  RefPtr<ScreenRootNode> root() { return root_; }
  SDL_Window* window() const { return window_; }

  float render_cost_ms() const { return render_cost_ms_; }
  float logic_cost_ms() const { return logic_cost_ms_; }
  float other_cost_ms() const { return other_cost_ms_; }

 private:
  friend class ScreenRootNode;

  void ProcessEvents();
  void PresentInternal();

  SDL_Window* window_ = nullptr;
  RefPtr<ScreenRootNode> root_;
  RefPtr<Bitmap> screen_;

  bool frozen_ = false;
  int32_t frame_rate_ = 60;
  int32_t frame_count_ = 0;
  int32_t brightness_ = 255;
  bool frame_skip_ = true;
  bool vsync_ = true;
  bool frame_started_ = false;

  uint64_t last_update_end_tick_ = 0;
  float render_cost_ms_ = 0.0f;
  float logic_cost_ms_ = 0.0f;
  float other_cost_ms_ = 0.0f;

  FPSLimiter limiter_;

  PrimitiveEmitter quad_emitter_;

  struct {
    bool configured = false;

    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;

    bool srgb_target = false;
    wgpu::RenderPipeline pipeline;
    PrimitiveEmitter primitive;
  } present_;
};

}  // namespace urge

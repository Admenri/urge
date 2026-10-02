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

//! Paces the frame loop of Graphics.
//!
//! The controller is told the target frame rate, and splits its job in two
//! halves that are called on both sides of a frame:
//!
//!   * BeginFrame() is called before the frame is rendered. It tracks how much
//!     time has passed since the previous frame and decides whether the frame
//!     has to be skipped, which only happens when the previous frame could not
//!     be presented in time and skipping is enabled.
//!   * Delay() is called after the frame was presented. It sleeps for the rest
//!     of the per-frame budget, so the frame rate does not run ahead of the
//!     target when rendering is faster than the target.
//!
//! The two halves are the reason two clocks are involved: the decision of
//! BeginFrame() is based on the wall clock, while the sleeping of Delay() is
//! based on the same clock plus the budget of the frame rate.
class FrameController {
 public:
  FrameController();

  void SetFrameRate(int32_t frame_rate);
  int32_t FrameRate() const { return frame_rate_; }

  void SetSkipEnabled(bool enabled);
  bool SkipEnabled() const { return skip_enabled_; }

  bool BeginFrame();
  void Delay();
  void Reset();

 private:
  int32_t frame_rate_ = 60;
  uint64_t frame_period_ns_ = 0;
  bool uncapped_ = false;
  bool skip_enabled_ = false;
  uint64_t frame_start_ns_ = 0;
  uint64_t last_frame_start_ns_ = 0;
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

class Graphics : public Singleton<Graphics> {
 public:
  Graphics();
  ~Graphics();

  /*-export.begin-*/
  void Update();
  void Wait(int32_t duration);
  void Fadein(int32_t duration);
  void Fadeout(int32_t duration);
  void Freeze();
  void Transition(int32_t duration = 10,
                  std::string filename = {},
                  int32_t vague = 40);
  void TransitionBitmap(int32_t duration = 10,
                        RefPtr<Bitmap> bitmap = {},
                        int32_t vague = 40);
  RefPtr<Bitmap> SnapToBitmap();
  void FrameReset();
  int32_t Width();
  int32_t Height();
  void ResizeScreen(int32_t width, int32_t height);
  void PlayMovie(std::string filename);

  ATTR(int32_t, FrameRate);
  ATTR(int32_t, FrameCount);
  ATTR(int32_t, Brightness);
  ATTR(bool, FrameSkip);
  /*-export.end-*/

  RefPtr<ScreenRootNode> root() { return root_; }

 private:
  friend class ScreenRootNode;
  void PresentInternal();

  SDL_Window* window_ = nullptr;
  RefPtr<ScreenRootNode> root_;
  RefPtr<Bitmap> screen_;

  bool frozen_ = false;
  int32_t frame_count_ = 0;
  int32_t brightness_ = 255;

  FrameController frame_controller_;

  /*! The single quad a transition frame is drawn with, see TransitionBitmap.
      It is an emitter of its own -- uploaded and drawn every step -- so it
      neither shares the buffer of the present nor the frame batch. */
  PrimitiveEmitter quad_emitter_;

  struct {
    bool configured = false;
    //! The format the swapchain is configured with, see PresentInternal.
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    //! Whether \c format is an *Srgb format, which the present shader cancels.
    bool srgb_target = false;
    wgpu::RenderPipeline pipeline;
    PrimitiveEmitter primitive;
  } present_;
};

}  // namespace urge

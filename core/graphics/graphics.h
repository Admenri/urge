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

#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_video.h"

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
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
  void Transition(URGE_EXCEPT, int32_t duration = 10,
                  std::string filename = {},
                  int32_t vague = 40);
  URGE_BINDING()
  void TransitionBitmap(URGE_EXCEPT, int32_t duration = 10,
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
  URGE_BINDING()
  ATTR(bool, VSync);

  RefPtr<ScreenRootNode> root() { return root_; }
  SDL_Window* window() const { return window_; }

  float render_cost_ms() const { return render_cost_ms_; }
  float logic_cost_ms() const { return logic_cost_ms_; }
  float other_cost_ms() const { return other_cost_ms_; }
  double present_cost_ms() const { return present_cost_ms_; }

  /* Called once at the end of every frame, after the frame has been presented
     and the script's `Graphics.update` is about to return.  The host installs
     it for work that has to happen between frames rather than inside them --
     on the web that is the garbage collection, which can only run where the
     interpreter's own stack is the only thing holding live objects (see
     bind/cruby_main.cc).  Nothing else uses it. */
  using FrameCallback = void (*)();
  void SetFrameCallback(FrameCallback callback) { frame_callback_ = callback; }

  /* The frame pacer.  Called after the present of every frame has been
     submitted, and expected to return only when the next frame may begin.

     `Graphics.update` is one frame of work and nothing else: it does not
     sleep, does not skip, and does not know what a display is.  How long a
     frame takes is entirely the host's business, which is what lets the same
     script run under a browser animation frame, under a desktop sleep, or
     under a fixed-step harness with no change here.  The loop that installs
     this lives in bind/cruby_main.cc. */
  using FramePresentCallback = void (*)();
  FramePresentCallback PresentCallback() const { return present_callback_; }
  void SetPresentCallback(FramePresentCallback callback) {
    present_callback_ = callback;
  }

  /* Asks the pacer to restart its clock, for a frame that should not count
     against the next one -- `Graphics.frame_reset` after a transition. */
  void RequestFrameReset() { frame_reset_requested_ = true; }
  bool ConsumeFrameReset() {
    const bool requested = frame_reset_requested_;
    frame_reset_requested_ = false;
    return requested;
  }

  /* The host's event feed.  With SDL's callback main loop the engine is no
     longer the one polling: SDL drains its queue and hands events over one at a
     time, so `SDL_PollEvent` inside the frame would find nothing left.  The
     host pushes what it was given and the frame's event pass reads that
     instead. */
  void PushEvent(const SDL_Event& event);

  /* Read by the pacer, which owns the timing the engine no longer does. */
  int32_t frame_rate() const { return frame_rate_; }
  bool frame_skip() const { return frame_skip_; }

 private:
  friend class ScreenRootNode;

  void ProcessEvents(URGE_EXCEPT);
  void PresentInternal(URGE_EXCEPT);

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

  struct {
    bool configured = false;

    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;

    bool srgb_target = false;
    wgpu::RenderPipeline pipeline;
    PrimitiveEmitter primitive;
  } present_;
};

}

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

namespace urge {

class ScreenRootNode : public Node {
 public:
  ScreenRootNode();
  ~ScreenRootNode() override;

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  //! The brightness quad is emitted and drawn by PostDraw(), so the node has to
  //! ask for that stage: DoDraw() is what gates PostDraw().
  bool DoDraw(DrawParam param) override;
  void PostDraw(DrawParam param) override;

  //! The range the brightness quad of this frame occupies in the vertex batch
  //! of the frame, see QuadVertexManager.
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
  /*-export.end-*/

  RefPtr<ScreenRootNode> root() { return root_; }

 private:
  friend class ScreenRootNode;
  void PresentInternal();

  SDL_Window* window_ = nullptr;
  RefPtr<ScreenRootNode> root_;
  RefPtr<Bitmap> screen_;

  bool frozen_ = false;
  int32_t brightness_ = 255;

  struct {
    bool configured = false;
    wgpu::RenderPipeline pipeline;
    PrimitiveEmitter primitive;
  } present_;
};

}  // namespace urge

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

#include "core/node.h"
#include "core/uniform.h"
#include "core/viewport.h"

namespace urge {

class Sprite : public Node {
 public:
  /*-export.begin-*/
  Sprite(RefPtr<Viewport> viewport = nullptr);
  ~Sprite() override;

  void Flash(RefPtr<Color> color, int32_t duration);
  void Update();

  int32_t Width();
  int32_t Height();

  ATTR(RefPtr<Viewport>, Viewport);
  ATTR(RefPtr<Bitmap>, Bitmap);
  ATTR(RefPtr<Rect>, SrcRect);
  ATTR(int32_t, X);
  ATTR(int32_t, Y);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  ATTR(float, ZoomX);
  ATTR(float, ZoomY);
  ATTR(float, Angle);
  ATTR(int32_t, WaveAmp);
  ATTR(int32_t, WaveLength);
  ATTR(int32_t, WaveSpeed);
  ATTR(float, WavePhase);
  ATTR(bool, Mirror);
  ATTR(int32_t, BushDepth);
  ATTR(int32_t, BushOpacity);
  ATTR(int32_t, Opacity);
  ATTR(int32_t, BlendType);
  ATTR(RefPtr<Color>, Color);
  ATTR(RefPtr<Tone>, Tone);
  /*-export.end-*/

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  SpriteBase::SpriteParam MakeParamInternal();
  PrimitiveEmitter::Slot EmitGeometryInternal(PrimitiveEmitter& emitter);

  RefPtr<Bitmap> bitmap_;
  RefPtr<Rect> src_rect_;
  int32_t x_ = 0, y_ = 0;
  int32_t ox_ = 0, oy_ = 0;
  float zoom_x_ = 1.0f, zoom_y_ = 1.0f;
  float angle_ = 0.0f;
  int32_t wave_amp_ = 0, wave_length_ = 180, wave_speed_ = 360;
  float wave_phase_ = 0.0f;
  bool mirror_ = false;
  int32_t bush_depth_ = 0, bush_opacity_ = 128;
  int32_t opacity_ = 255, blend_type_ = 0;
  RefPtr<Color> color_;
  RefPtr<Tone> tone_;

  struct {
    glm::vec4 color = glm::vec4(0.0f);
    float step = 0.0f;
  } flash_;

  bool rgssvx_style_ = true;

  //! The slot of the object pool this frame put the transform of this sprite
  //! in, which the sprite pipeline binds at set 1.
  UniformBlockPool::Slot object_slot_ = {};
  //! The slot of the sprite pool this frame put the parameter of this sprite
  //! in, which the sprite pipeline binds at set 3.
  UniformBlockPool::Slot param_slot_ = {};
  //! The range EmitGeometryInternal() appended to the vertex batch of the
  //! frame, i.e. the vertices DoDraw() draws.
  PrimitiveEmitter::Slot primitive_slot_ = {};
};

}  // namespace urge

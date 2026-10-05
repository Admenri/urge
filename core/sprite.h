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

#include "core/definition.h"
#include "core/effect.h"
#include "core/node.h"
#include "core/uniform.h"
#include "core/viewport.h"

namespace urge {

URGE_BINDING()
class Sprite : public Node {
 public:
  URGE_BINDING()
  Sprite(RefPtr<Viewport> viewport = nullptr);
  URGE_BINDING()
  ~Sprite() override;

  URGE_BINDING()
  void Flash(RefPtr<Color> color, int32_t duration);
  URGE_BINDING()
  void Update();

  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();

  URGE_BINDING()
  ATTR(RefPtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR(RefPtr<Bitmap>, Bitmap);
  URGE_BINDING()
  ATTR(RefPtr<Rect>, SrcRect);
  URGE_BINDING()
  ATTR(int32_t, X);
  URGE_BINDING()
  ATTR(int32_t, Y);
  URGE_BINDING()
  ATTR(int32_t, OX);
  URGE_BINDING()
  ATTR(int32_t, OY);
  URGE_BINDING()
  ATTR(float, ZoomX);
  URGE_BINDING()
  ATTR(float, ZoomY);
  URGE_BINDING()
  ATTR(float, Angle);
  URGE_BINDING()
  ATTR(int32_t, WaveAmp);
  URGE_BINDING()
  ATTR(int32_t, WaveLength);
  URGE_BINDING()
  ATTR(int32_t, WaveSpeed);
  URGE_BINDING()
  ATTR(float, WavePhase);
  URGE_BINDING()
  ATTR(bool, Mirror);
  URGE_BINDING()
  ATTR(int32_t, BushDepth);
  URGE_BINDING()
  ATTR(int32_t, BushOpacity);
  URGE_BINDING()
  ATTR(int32_t, Opacity);
  URGE_BINDING()
  ATTR(int32_t, BlendType);
  URGE_BINDING()
  ATTR(RefPtr<Color>, Color);
  URGE_BINDING()
  ATTR(RefPtr<Tone>, Tone);
  URGE_BINDING()
  ATTR(RefPtr<Effect>, Effect);

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
  //! The user authored shader the draw runs, or none for the built in sprite
  //! pipeline, see DoDraw.
  RefPtr<Effect> effect_;

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

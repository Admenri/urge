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

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/render/primitive.h"
#include "core/render/uniform.h"
#include "core/scene/node.h"
#include "core/scene/viewport.h"

namespace urge {

URGE_BINDING()
class Plane : public Node {
 public:
  URGE_BINDING()
  Plane(MaybePtr<Viewport> viewport = nullptr);
  ~Plane() override;

  URGE_BINDING()
  ATTR(MaybePtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, Bitmap);
  URGE_BINDING()
  ATTR(int32_t, OX);
  URGE_BINDING()
  ATTR(int32_t, OY);
  URGE_BINDING()
  ATTR(float, ZoomX);
  URGE_BINDING()
  ATTR(float, ZoomY);
  URGE_BINDING()
  ATTR(int32_t, Opacity);
  URGE_BINDING()
  ATTR(int32_t, BlendType);
  URGE_BINDING()
  ATTR(RefPtr<Color>, Color);
  URGE_BINDING()
  ATTR(RefPtr<Tone>, Tone);

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  void CreateEffectBindings();

  PrimitiveEmitter::Slot EmitGeometryInternal(PrimitiveEmitter& emitter,
                                              DrawParam param);

  RefPtr<Bitmap> bitmap_;
  int32_t ox_ = 0, oy_ = 0;
  float zoom_x_ = 1.0f, zoom_y_ = 1.0f;
  int32_t opacity_ = 255, blend_type_ = 0;
  RefPtr<Color> color_;
  RefPtr<Tone> tone_;

  UniformBlockPool::Slot object_slot_ = {};
  PrimitiveEmitter::Slot primitive_slot_ = {};

  wgpu::Buffer tint_uniform_;
  wgpu::BindGroup tint_group_;
};

}

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

#include "core/bitmap.h"
#include "core/node.h"
#include "core/primitive.h"
#include "core/uniform.h"
#include "core/viewport.h"

namespace urge {

class Plane : public Node {
 public:
  /*-export.begin-*/
  Plane(RefPtr<Viewport> viewport = nullptr);
  ~Plane() override;

  ATTR(RefPtr<Viewport>, Viewport);
  ATTR(RefPtr<Bitmap>, Bitmap);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  ATTR(float, ZoomX);
  ATTR(float, ZoomY);
  ATTR(int32_t, Opacity);
  ATTR(int32_t, BlendType);
  ATTR(RefPtr<Color>, Color);
  ATTR(RefPtr<Tone>, Tone);
  /*-export.end-*/

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  //! Creates the buffer and the bind group the tint of this plane is read from,
  //! which the plane pipeline binds at set 3.
  void CreateEffectBindings();
  //! Emits the quad which covers the render target with the tiles of the
  //! bitmap, see Prepare().
  PrimitiveEmitter::Slot EmitGeometryInternal(PrimitiveEmitter& emitter,
                                              DrawParam param);

  RefPtr<Bitmap> bitmap_;
  int32_t ox_ = 0, oy_ = 0;
  float zoom_x_ = 1.0f, zoom_y_ = 1.0f;
  int32_t opacity_ = 255, blend_type_ = 0;
  RefPtr<Color> color_;
  RefPtr<Tone> tone_;

  //! The object pool slot of this plane, bound at set 1. The quad of a plane is
  //! emitted in the pixels of the render target, so the transform it carries is
  //! the identity and the tiles are placed by their texture coordinates instead.
  UniformBlockPool::Slot object_slot_ = {};
  //! The range EmitGeometryInternal() appended to the vertex batch of the frame,
  //! i.e. the vertices DoDraw() draws.
  PrimitiveEmitter::Slot primitive_slot_ = {};

  //! The tint of this plane, `PlaneBase::PlaneParam`, and the bind group of set
  //! 3 which covers it.
  wgpu::Buffer tint_uniform_;
  wgpu::BindGroup tint_group_;
};

}  // namespace urge

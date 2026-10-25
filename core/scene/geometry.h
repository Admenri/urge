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

#include <cstdint>
#include <vector>

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/graphics/effect.h"
#include "core/render/primitive.h"
#include "core/render/uniform.h"
#include "core/scene/node.h"
#include "core/scene/viewport.h"

namespace urge {

URGE_BINDING()
class Geometry : public Node {
 public:
  URGE_BINDING()
  Geometry(MaybePtr<Viewport> viewport = nullptr);
  ~Geometry() override;

  URGE_BINDING()
  void SetPosition(URGE_EXCEPT, int32_t triangle, int32_t point, RefPtr<Vector3> position);
  URGE_BINDING()
  void SetTexcoord(URGE_EXCEPT, int32_t triangle, int32_t point, RefPtr<Vector2> texcoord);
  URGE_BINDING()
  void SetColor(URGE_EXCEPT, int32_t triangle, int32_t point, RefPtr<Color> color);

  URGE_BINDING()
  ATTR(MaybePtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR_EXC(int32_t, Capacity);
  URGE_BINDING()
  ATTR(MaybePtr<Bitmap>, Bitmap);
  URGE_BINDING()
  ATTR(int32_t, BlendType);
  URGE_BINDING()
  ATTR(MaybePtr<Effect>, Effect);

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  PrimitiveEmitter::Slot EmitGeometryInternal(PrimitiveEmitter& emitter);

  RefPtr<Bitmap> bitmap_;
  int32_t blend_type_ = 0;

  RefPtr<Effect> effect_;

  struct TriangleData {
    VertexData vertex[3];
  };

  std::vector<TriangleData> data_;

  UniformBlockPool::Slot object_slot_ = {};

  PrimitiveEmitter::Slot primitive_slot_ = {};
};

}

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

#include "core/definition.h"
#include "core/effect.h"
#include "core/node.h"
#include "core/object.h"
#include "core/primitive.h"
#include "core/uniform.h"
#include "core/viewport.h"

namespace urge {

URGE_BINDING()
class Geometry : public Node {
 public:
  URGE_BINDING()
  Geometry(RefPtr<Viewport> viewport = nullptr);
  URGE_BINDING()
  ~Geometry() override;

  URGE_BINDING()
  void SetPosition(int32_t triangle, int32_t point, RefPtr<Vector3> position);
  URGE_BINDING()
  void SetTexcoord(int32_t triangle, int32_t point, RefPtr<Vector2> texcoord);
  URGE_BINDING()
  void SetColor(int32_t triangle, int32_t point, RefPtr<Color> color);

  URGE_BINDING()
  ATTR(RefPtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR(int32_t, Capacity);
  URGE_BINDING()
  ATTR(RefPtr<Bitmap>, Bitmap);
  URGE_BINDING()
  ATTR(int32_t, BlendType);
  URGE_BINDING()
  ATTR(RefPtr<Effect>, Effect);

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

}  // namespace urge

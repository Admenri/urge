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
#include "core/utility.h"

namespace urge {

class Viewport : public Node {
 public:
  /*-export.begin-*/
  Viewport(int32_t x, int32_t y, int32_t width, int32_t height);
  Viewport(RefPtr<Rect> rect);
  Viewport();
  ~Viewport();

  void Flash(RefPtr<Color> color, int32_t duration);
  void Update();

  ATTR(RefPtr<Rect>, Rect);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  ATTR(RefPtr<Color>, Color);
  ATTR(RefPtr<Tone>, Tone);
  /*-export.end-*/

 private:
  void DisposeObject() override;
  void Prepare(DrawParam param) override;
  void DoDraw(DrawParam param) override;
  void PostDraw(DrawParam param) override;

  void ResetTransform();

  void CreateEffectBindings();
  void AcquirePingPong(const RectI& region);
  wgpu::Buffer AcquireVertexBuffer(size_t size);

  RefPtr<Rect> rect_;
  glm::ivec2 origin_ = glm::ivec2(0);
  RefPtr<Color> color_;
  RefPtr<Tone> tone_;

  struct {
    glm::vec4 color = glm::vec4(0.0f);
    float step = 0.0f;
  } flash_;

  RefPtr<Bitmap> pingpong_;
  wgpu::Buffer object_uniform_, tint_uniform_;
  wgpu::BindGroup object_group_, tint_group_;

  PrimitiveEmitter primitive_;
  wgpu::Buffer vertex_buffer_;
};

}  // namespace urge

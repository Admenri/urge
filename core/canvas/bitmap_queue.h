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

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/refptr.h"
#include "core/render/device.h"
#include "core/render/primitive.h"

namespace urge {

struct BitmapDraw {
  enum class Kind {
    kTexture,
    kTextureMask,
    kColor,
  };

  Kind kind = Kind::kTexture;

  RefPtr<Bitmap> target;
  RefPtr<Bitmap> source;
  RefPtr<Bitmap> mask;

  wgpu::BindGroup text_group;

  RectF rect;
  RectF texcoord;
  glm::vec4 colors[4] = {glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(1.0f),
                         glm::vec4(1.0f)};
};

class BitmapQueue : public Singleton<BitmapQueue> {
 public:
  BitmapQueue();
  ~BitmapQueue();

  void Push(BitmapDraw draw);

  void Flush();

 private:
  void OpenPass(const RefPtr<Bitmap>& target);
  void ClosePass();
  void Record(const BitmapDraw& draw, std::uint32_t first_vertex);

  std::vector<BitmapDraw> draws_;

  wgpu::CommandEncoder encoder_;
  wgpu::RenderPassEncoder pass_;
  RefPtr<Bitmap> target_;

  PrimitiveEmitter vertices_;
  bool executing_ = false;
};

}

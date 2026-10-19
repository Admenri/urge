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

#include "core/bitmap.h"
#include "core/definition.h"
#include "core/device.h"
#include "core/object.h"
#include "core/primitive.h"
#include "core/refptr.h"

namespace urge {

// One recorded drawing command.  A `Bitmap` operation fills one of these in and
// hands it to the queue; it never opens a pass or submits anything itself.
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

  // `DrawText` draws a cached text run instead of a bitmap, so it carries the
  // run's bind group where a source bitmap would go.
  wgpu::BindGroup text_group;

  RectF rect;
  RectF texcoord;
  glm::vec4 colors[4] = {glm::vec4(1.0f), glm::vec4(1.0f), glm::vec4(1.0f),
                         glm::vec4(1.0f)};
};

// Executes the drawing a script does against `Bitmap`s.
//
// A `Bitmap` operation only records: it appends a `BitmapDraw` here instead of
// touching the GPU, so a batch costs one submission rather than one per
// operation.  The batch runs when a bitmap's handle is actually needed --
// `Bitmap::AcquireHandle` -- and before the scene reads the textures, and it
// always runs in the order the commands were recorded: `Bitmap` is a painter's
// order surface, so nothing is reordered, only deferred.
//
// Deferring to a single execution is what lets `PrimitiveEmitter` stay in its
// plain form.  Every command is exactly one quad, so the batch is emitted in
// one go and uploaded once, and a command's range is its index times the vertex
// count of a quad -- no per-command slot bookkeeping anywhere.
//
// A blit may not read the bitmap it draws into, because a pass cannot sample
// its own attachment.  `Blt`, `StretchBlt` and `MaskBlt` reject that case where
// the command is recorded, so nothing here ever has to resolve a self read.
class BitmapQueue : public Singleton<BitmapQueue> {
 public:
  BitmapQueue();
  ~BitmapQueue();

  void Push(BitmapDraw draw);

  // Executes everything recorded so far.  Safe to call anywhere: it returns
  // immediately when the batch is empty, and while the batch is already being
  // executed.
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

}  // namespace urge

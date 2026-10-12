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

#include <vector>

#include "core/bitmap.h"
#include "core/definition.h"
#include "core/device.h"
#include "core/object.h"
#include "core/primitive.h"
#include "core/refptr.h"

namespace urge {

// Records the drawing a script does against `Bitmap`s into one command buffer.
//
// A `Bitmap` operation is not a draw call.  It owns an encoder, a render pass
// and a submission, so a frame that issues a few hundred of them pays the
// submission overhead a few hundred times, and that overhead is almost all of
// what the operation costs.  The queue keeps the frame's encoder open instead:
// the whole batch then costs one `Finish` and one `Submit`, while the
// operations still land in the order they arrived -- the painter's order of
// `Bitmap` is part of the RGSS contract, so nothing is reordered, only
// deferred.  Operations that share a target stay inside one render pass as
// well, and every pass shares the frame's encoder.
//
// Everything that is not a `Bitmap` operation has to `Flush()` first, because
// a recorded draw only happens at the submission:
//  - `Node::Render` flushes before it records its pass, so the scene reads the
//    textures the queue wrote;
//  - a read back (`GetPixel`, `ToImage`) needs the writes to have happened;
//  - `WriteTexture` (`UpdateWithImage`, `UpdateWithPixels`) and
//    `CopyTextureToTexture` (`Bitmap(other)`) on a queued target would
//    otherwise be ordered before the pass that draws into it;
//  - the font cache flushes before it evicts a run, whose texture a recorded
//    draw may still sample.
class BitmapQueue : public Singleton<BitmapQueue> {
 public:
  BitmapQueue();
  ~BitmapQueue();

  // Returns the pass to record `target` into, ending the previous one when it
  // draws somewhere else.
  wgpu::RenderPassEncoder Begin(RefPtr<Bitmap> target);

  // Keeps a bitmap a recorded draw samples alive until the batch is submitted.
  // A script can drop its last reference to a bitmap right after blitting from
  // it, and the Ruby collector would then take the texture with it while the
  // recorded pass still reads it.
  void Pin(RefPtr<Bitmap> texture);

  // Returns the bind group to sample `texture` with while the pass draws into
  // `target`.  A render pass may not sample the attachment it writes, so
  // `bitmap.blt(0, 0, bitmap)` -- which is legal RGSS -- is served from a copy
  // of the bitmap taken before the pass carries on, and the result is what
  // reading it in place would have produced.
  wgpu::BindGroup ResolveRead(RefPtr<Bitmap> target, RefPtr<Bitmap> texture);

  PrimitiveEmitter& vertices() { return vertices_; }

  // Ends the open pass and submits the frame's batch.  Nothing is submitted
  // when no operation was recorded.
  void Flush();

 private:
  void EnsureScratch(const glm::ivec2& size);

  wgpu::CommandEncoder encoder_;
  wgpu::RenderPassEncoder pass_;
  RefPtr<Bitmap> target_;
  RefPtr<Bitmap> scratch_;
  std::vector<RefPtr<Bitmap>> pinned_;
  PrimitiveEmitter vertices_;
};

}  // namespace urge

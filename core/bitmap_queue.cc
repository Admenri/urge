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

#include "core/bitmap_queue.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "SDL3/SDL_timer.h"

#include "core/gpu.h"
#include "core/render_stats.h"

namespace urge {

namespace {

// A frame of bitmap drawing is a few hundred quads at most, so reserving them
// up front keeps the vertex buffer from being replaced while the frame's draws
// are already recorded against it.
constexpr std::size_t kInitialVertexCapacity = 8192;

}  // namespace

BitmapQueue::BitmapQueue() {
  vertices_.Reserve(kInitialVertexCapacity);
}

BitmapQueue::~BitmapQueue() {
  Flush();
  vertices_.Reset();
}

wgpu::RenderPassEncoder BitmapQueue::Begin(RefPtr<Bitmap> target) {
  if (!encoder_)
    encoder_ = g_device.CreateCommandEncoder(nullptr);

  if (pass_ && target_ == target)
    return pass_;

  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }

  target_ = std::move(target);
  pass_ = target_->BeginRendering(encoder_);
  return pass_;
}

void BitmapQueue::Pin(RefPtr<Bitmap> texture) {
  for (const RefPtr<Bitmap>& pinned : pinned_)
    if (pinned.get() == texture.get())
      return;

  pinned_.push_back(std::move(texture));
}

wgpu::BindGroup BitmapQueue::ResolveRead(RefPtr<Bitmap> target,
                                         RefPtr<Bitmap> texture) {
  if (texture.get() != target.get())
    return texture->texture_group();

  // The pass is about to write this bitmap, so it cannot sample it.  Close the
  // pass, take a copy of what has been drawn into it so far, and let the
  // caller sample that: the copy is recorded ahead of the pass that carries
  // on, so it holds exactly the content the operation is supposed to read.
  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }

  if (!encoder_)
    encoder_ = g_device.CreateCommandEncoder(nullptr);

  EnsureScratch(texture->size());

  wgpu::TexelCopyTextureInfo source;
  source.texture = texture->texture();

  wgpu::TexelCopyTextureInfo destination;
  destination.texture = scratch_->texture();

  wgpu::Extent3D size;
  size.width = static_cast<std::uint32_t>(texture->size().x);
  size.height = static_cast<std::uint32_t>(texture->size().y);

  encoder_.CopyTextureToTexture(&source, &destination, &size);

  return scratch_->texture_group();
}

void BitmapQueue::EnsureScratch(const glm::ivec2& size) {
  if (scratch_ && scratch_->size() == size)
    return;

  // The scratch is normalized against the bitmap it copies, so it has to match
  // its size exactly.  The one it replaces stays alive until the batch that
  // recorded draws against it has been submitted.
  if (scratch_)
    pinned_.push_back(scratch_);

  scratch_ = MakeRefCounted<Bitmap>(size.x, size.y);
}

void BitmapQueue::Flush() {
  if (!encoder_)
    return;

  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }
  target_ = nullptr;

  const std::uint64_t recorded = SDL_GetPerformanceCounter();
  auto command = encoder_.Finish(nullptr);
  const std::uint64_t finished = SDL_GetPerformanceCounter();
  encoder_ = nullptr;

  CountSubmit(true);
  g_queue.Submit(1, &command);
  const std::uint64_t submitted = SDL_GetPerformanceCounter();

  RenderFrameCounters& counters = GetRenderStats().current;
  counters.bitmap_finish_ms += TicksToMilliseconds(finished - recorded);
  counters.bitmap_submit_ms += TicksToMilliseconds(submitted - finished);

  pinned_.clear();
  vertices_.ReleaseRetiredBuffers();
}

}  // namespace urge

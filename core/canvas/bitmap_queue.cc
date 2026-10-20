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

#include "core/scene/bitmap_queue.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "SDL3/SDL_timer.h"

#include "core/graphics/gpu.h"
#include "core/graphics/pipeline.h"
#include "core/graphics/render_stats.h"
#include "core/graphics/shader.h"

namespace urge {

namespace {

// A frame of bitmap drawing is a few hundred quads at most; reserving them up
// front keeps the vertex buffer from being replaced while the batch is being
// recorded.
constexpr std::size_t kInitialVertexCapacity = 8192;

}  // namespace

BitmapQueue::BitmapQueue() {
  vertices_.Reserve(kInitialVertexCapacity);
}

BitmapQueue::~BitmapQueue() {
  Flush();
  vertices_.Reset();
}

void BitmapQueue::Push(BitmapDraw draw) {
  draws_.push_back(std::move(draw));
}

void BitmapQueue::OpenPass(const RefPtr<Bitmap>& target) {
  if (!encoder_)
    encoder_ = g_device.CreateCommandEncoder(nullptr);

  if (pass_ && target_ == target)
    return;

  ClosePass();

  target_ = target;
  pass_ = target->BeginRendering(encoder_);
}

void BitmapQueue::ClosePass() {
  if (!pass_)
    return;

  pass_.End();
  pass_ = nullptr;
}

void BitmapQueue::Record(const BitmapDraw& draw, std::uint32_t first_vertex) {
  const bool masked = draw.kind == BitmapDraw::Kind::kTextureMask;

  OpenPass(draw.target);

  pass_.SetPipeline(draw.kind == BitmapDraw::Kind::kColor
                        ? ShaderSet::Get().state.bitmap.color_noblend
                        : (masked
                               ? ShaderSet::Get().state.bitmap.texture_mask_pma
                               : ShaderSet::Get().state.bitmap.texture_pma));
  pass_.SetBindGroup(0, draw.target->scene_group(), 0, nullptr);
  pass_.SetBindGroup(1, draw.target->object_group(), 0, nullptr);

  if (draw.kind != BitmapDraw::Kind::kColor) {
    // A pass may not sample the attachment it writes, so a blit whose source is
    // its own destination is rejected where it is recorded -- which is why a
    // command never has to resolve a self read here.
    pass_.SetBindGroup(2,
                       draw.text_group ? draw.text_group
                                       : draw.source->texture_group(),
                       0, nullptr);
    if (masked)
      pass_.SetBindGroup(3, draw.mask->texture_group(), 0, nullptr);
  }

  pass_.SetVertexBuffer(0, vertices_.buffer(), 0, WGPU_WHOLE_SIZE);
  pass_.Draw(kQuadVertexCount, 1, first_vertex, 0);
}

void BitmapQueue::Flush() {
  if (draws_.empty() || executing_)
    return;

  std::vector<BitmapDraw> draws;
  draws.swap(draws_);

  executing_ = true;

  const std::uint64_t begin = SDL_GetPerformanceCounter();

  // Every command is one quad, so the batch goes into the buffer in one go and
  // each command's range follows from its index.
  for (const BitmapDraw& draw : draws) {
    vertices_.EmitQuad(draw.rect, draw.texcoord, draw.colors[0], draw.colors[1],
                       draw.colors[2], draw.colors[3]);
    vertices_.End();
  }
  vertices_.Upload();

  const std::uint64_t uploaded = SDL_GetPerformanceCounter();

  for (std::size_t index = 0; index < draws.size(); ++index)
    Record(draws[index],
           static_cast<std::uint32_t>(index) * kQuadVertexCount);

  ClosePass();

  const std::uint64_t recorded = SDL_GetPerformanceCounter();
  auto command = encoder_.Finish(nullptr);
  const std::uint64_t finished = SDL_GetPerformanceCounter();
  encoder_ = nullptr;
  target_ = nullptr;

  CountSubmit(true);
  g_queue.Submit(1, &command);
  const std::uint64_t submitted = SDL_GetPerformanceCounter();

  RenderFrameCounters& counters = GetRenderStats().current;
  counters.bitmap_ops += static_cast<std::uint32_t>(draws.size());
  counters.bitmap_upload_ms += TicksToMilliseconds(uploaded - begin);
  counters.bitmap_record_ms += TicksToMilliseconds(recorded - uploaded);
  counters.bitmap_finish_ms += TicksToMilliseconds(finished - recorded);
  counters.bitmap_submit_ms += TicksToMilliseconds(submitted - finished);

  executing_ = false;
}

}  // namespace urge

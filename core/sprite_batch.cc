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

#include "core/sprite_batch.h"

#include <algorithm>
#include <cstdlib>

#include "core/config.h"
#include "core/device.h"
#include "core/drawable.h"
#include "core/gpu_utils.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/shader.h"

namespace urge {

namespace {

constexpr std::size_t kInitialParamSlots = 1024;

static_assert(sizeof(SpriteParam) == 112, "unexpected SpriteParam layout");
static_assert(offsetof(SpriteParam, blend_color) == 64,
              "unexpected SpriteParam layout");
static_assert(offsetof(SpriteParam, blend_tone) == 80,
              "unexpected SpriteParam layout");
static_assert(offsetof(SpriteParam, bush_depth) == 96,
              "unexpected SpriteParam layout");
static_assert(offsetof(SpriteParam, bush_opacity) == 100,
              "unexpected SpriteParam layout");

}  // namespace

SpriteBatch::SpriteBatch() {
  disabled_ = !Config::Get().gfx.sprite_batch;

  emitter_.Reserve(4096);

  EnsureParamBuffer(kInitialParamSlots * sizeof(SpriteParam));
  BeginFrame();
}

SpriteBatch::~SpriteBatch() {
  emitter_.Reset();
  param_group_ = nullptr;
  param_buffer_ = nullptr;
}

void SpriteBatch::BeginFrame() {
  emitter_.Clear();
  params_.clear();
  run_ = {};
}

void SpriteBatch::Flush() {
  emitter_.Upload();

  if (params_.empty())
    return;

  const std::size_t bytes = params_.size() * sizeof(SpriteParam);
  EnsureParamBuffer(bytes);
  g_queue.WriteBuffer(param_buffer_, 0, params_.data(), bytes);
}

void SpriteBatch::SetParam(uint32_t vertex_first, const SpriteParam& param) {
  const std::size_t index = vertex_first / kQuadVertexCount;

  if (params_.size() <= index)
    params_.resize(index + 1);

  params_[index] = param;
}

void SpriteBatch::EnsureParamBuffer(std::size_t bytes) {
  if (param_buffer_ && param_buffer_.GetSize() >= bytes)
    return;

  const std::uint64_t size =
      std::max<std::uint64_t>(bytes, kInitialParamSlots * sizeof(SpriteParam));

  wgpu::BufferDescriptor desc;
  desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  desc.size = size;
  param_buffer_ = g_device.CreateBuffer(&desc);

  if (!param_buffer_) {
    LOGGER_ERROR("the device rejected a {} byte sprite parameter buffer", size);
    param_group_ = nullptr;
    return;
  }

  /* The array is read as a whole, so the binding covers the buffer rather
     than one element of it, and the shader is free to index any quad of the
     frame with it. */
  util::BufferSet binding(param_buffer_, 0, size);

  param_group_ =
      util::CreateBindGroup(ShaderSet::Get()
                                .state.sprite.sprite_blends.at(BLEND_NORMAL)
                                .GetBindGroupLayout(1),
                            {{0, binding}});
}

void FlushSpriteBatch(DrawContext* param) {
  SpriteBatch::Run& run = SpriteBatch::Get().run();
  if (!run.active)
    return;

  const uint32_t count = run.end_vertex - run.first_vertex;
  run.active = false;

  if (!count || !run.texture || !run.scene || !param->pass)
    return;

  param->pass.SetPipeline(ShaderSet::Get().state.sprite.sprite_blends.at(
      static_cast<BlendType>(run.blend_type)));
  param->pass.SetBindGroup(0, run.scene, 0, nullptr);
  param->pass.SetBindGroup(1, SpriteBatch::Get().param_group(), 0, nullptr);
  param->pass.SetBindGroup(2, run.texture->texture_group(), 0, nullptr);
  param->pass.SetVertexBuffer(0, SpriteBatch::Get().emitter().buffer(), 0,
                              WGPU_WHOLE_SIZE);
  param->pass.Draw(count, 1, run.first_vertex, 0);
}

}  // namespace urge

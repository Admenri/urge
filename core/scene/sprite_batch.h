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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "glm/glm.hpp"

#include "core/bitmap.h"
#include "core/object.h"
#include "core/primitive.h"

namespace urge {

struct DrawContext;

struct alignas(16) SpriteParam {
  glm::mat4 model_mat;
  glm::vec4 blend_color;
  glm::vec4 blend_tone;
  float bush_depth = 0.0f;
  float bush_opacity = 1.0f;
  float padding[2] = {0.0f, 0.0f};
};

class SpriteBatch : public Singleton<SpriteBatch> {
 public:
  struct Run {
    bool active = false;
    uint32_t first_vertex = 0;
    uint32_t end_vertex = 0;
    int32_t blend_type = 0;
    RefPtr<Bitmap> texture;
    wgpu::BindGroup scene;
  };

  SpriteBatch();
  ~SpriteBatch();

  bool disabled() const { return disabled_; }
  void SetDisabled(bool disabled) { disabled_ = disabled; }

  void BeginFrame();

  void Flush();

  PrimitiveEmitter& emitter() { return emitter_; }

  void SetParam(uint32_t vertex_first, const SpriteParam& param);

  wgpu::BindGroup param_group() { return param_group_; }

  Run& run() { return run_; }

 private:
  void EnsureParamBuffer(std::size_t bytes);

  PrimitiveEmitter emitter_;
  std::vector<SpriteParam> params_;
  wgpu::Buffer param_buffer_ = nullptr;
  wgpu::BindGroup param_group_ = nullptr;
  Run run_;
  bool disabled_ = false;
};

void FlushSpriteBatch(DrawContext* param);

}  // namespace urge

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
#include <string_view>

#include "glm/vec2.hpp"
#include "glm/vec4.hpp"
#include "webgpu/webgpu_cpp.hpp"

namespace urge {

// Screen upscaling for the final present pass.  `nearest` and `linear` are
// sampler-only modes handled by Graphics; the shader modes render the screen
// into an intermediate texture and hand that to the normal present pipeline.
//
// The shader code lives entirely in upscaler.cc so the render pipeline and
// Graphics present code stay independent of the FSR/bicubic/Lanczos details.
class Upscaler {
 public:
  enum class Mode {
    kOff,
    kNearest,
    kLinear,
    kBicubic,
    kLanczos,
    kFsr1,
  };

  struct alignas(16) Params {
    glm::vec4 a;
    glm::vec4 b;
    glm::vec4 c;
    glm::vec4 d;
  };

  static Mode ParseMode(std::string_view name);
  static const char* ModeName(Mode mode);

  explicit Upscaler(Mode mode);
  ~Upscaler();

  Upscaler(const Upscaler&) = delete;
  Upscaler& operator=(const Upscaler&) = delete;

  Mode mode() const { return mode_; }
  bool UsesShader() const;
  uint64_t generation() const { return generation_; }

  // Renders SOURCE (game resolution) into an output texture of OUTPUT_SIZE and
  // returns the view to present.  Returns an empty view for sampler-only modes.
  wgpu::TextureView Run(wgpu::CommandEncoder encoder,
                        wgpu::TextureView source,
                        const glm::ivec2& source_size,
                        const glm::ivec2& output_size);

 private:
  void CreatePipelines();
  void EnsureOutputs(const glm::ivec2& size, int count);
  void WriteParams(int index, const Params& params);
  wgpu::BindGroup MakeSourceGroup(const wgpu::RenderPipeline& state,
                                  wgpu::TextureView view);
  wgpu::BindGroup MakeParamsGroup(const wgpu::RenderPipeline& state,
                                  wgpu::Buffer buffer);
  void Draw(wgpu::CommandEncoder encoder,
            const wgpu::RenderPipeline& state,
            wgpu::TextureView target,
            wgpu::BindGroup params,
            wgpu::BindGroup source);

  Mode mode_ = Mode::kOff;

  wgpu::RenderPipeline bicubic_;
  wgpu::RenderPipeline lanczos_;
  wgpu::RenderPipeline fsr_easu_;
  wgpu::RenderPipeline fsr_rcas_;

  wgpu::Sampler sampler_;
  wgpu::Buffer params_[2];

  wgpu::Texture outputs_[2];
  wgpu::TextureView output_views_[2];
  glm::ivec2 output_size_ = glm::ivec2(0);
  int output_count_ = 0;
  uint64_t generation_ = 0;
};

}

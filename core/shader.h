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
#include <set>
#include <string>
#include <vector>

#include "webgpu/webgpu.h"
#include "webgpu/webgpu_cpp.hpp"

namespace urge {

struct ShaderBinding {
  uint32_t binding = 0;
  WGPUBufferBindingType buffer = WGPUBufferBindingType_Undefined;
  bool texture = false;
  bool sampler = false;
  uint64_t min_binding_size = 0;
};

struct ShaderGroup {
  std::vector<ShaderBinding> bindings;
};

struct ShaderAttribute {
  uint32_t location = 0;
  WGPUVertexFormat format = WGPUVertexFormat_Float32x4;
};

struct ShaderReflection {
  std::vector<ShaderGroup> groups;
  std::vector<ShaderAttribute> attributes;
};

class Shader {
 public:
  Shader() = default;

  static Shader Compile(wgpu::ShaderStage stage, std::string_view glsl);

  const std::vector<uint32_t>& spirv() const { return spirv_; }
  const std::string& entry_point() const { return entry_point_; }
  const ShaderReflection& reflection() const { return reflection_; }

 private:
  std::vector<uint32_t> spirv_;
  std::string entry_point_;
  ShaderReflection reflection_;
};

class Pipeline {
 public:

  Pipeline(std::string_view vs_glsl,
           std::string_view fs_glsl,
           std::vector<std::vector<uint32_t>> vb_layouts,
           const std::set<uint32_t>& dynamic_sets = {},
           uint32_t min_sets = 0);

  wgpu::RenderPipeline MakeState(wgpu::PrimitiveState primitive,
                                 std::optional<wgpu::DepthStencilState> depth,
                                 std::vector<wgpu::ColorTargetState> blends,
                                 wgpu::MultisampleState samples = {});

  wgpu::RenderPipeline MakeDefaultState(const wgpu::BlendState* blend);

  std::vector<ShaderBinding> group_bindings(uint32_t set) const;

 private:
  struct VertexBuffer {
    uint64_t stride = 0;
    std::vector<wgpu::VertexAttribute> attributes;
  };

  Shader vertex_;
  Shader fragment_;
  wgpu::ShaderModule vertex_module_;
  wgpu::ShaderModule fragment_module_;
  wgpu::PipelineLayout layout_;
  std::vector<VertexBuffer> buffers_;

  std::set<uint32_t> dynamic_sets_;
};

}  // namespace urge

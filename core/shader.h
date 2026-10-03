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
  /*! \param[in] min_sets The number of bind group layouts the pipeline always
      carries, even when neither stage declares a binding in one of them: an
      Effect binds its custom set (set 2) on every draw, so its pipeline has to
      own a layout for it whether or not the stages read it. */
  Pipeline(std::string_view vs_glsl,
           std::string_view fs_glsl,
           std::vector<std::vector<uint32_t>> vb_layouts,
           const std::set<uint32_t>& dynamic_sets = {},
           uint32_t min_sets = 0);

  wgpu::RenderPipeline MakeState(wgpu::PrimitiveState primitive,
                                 std::optional<wgpu::DepthStencilState> depth,
                                 std::vector<wgpu::ColorTargetState> blends,
                                 wgpu::MultisampleState samples = {});

  /*! Builds the state of the plain draw of this shader with the defaults of
      the engine: a triangle list into the single RGBA8Unorm color target every
      pass carries, with the depth-stencil state of a pass which ignores the
      attachment, blended with \p blend or not blended at all when it is null,
      see GetDefaultPrimitiveState and GetBlendState. The nodes which draw with
      a user authored shader -- an Effect -- take the same state as the built in
      pipelines, which is what makes a sprite swap only its shader. */
  wgpu::RenderPipeline MakeDefaultState(const wgpu::BlendState* blend);

  /*! The bindings of one bind group of this shader, merged from both stages
      and keyed by their binding. A stage which does not touch the group leaves
      it out, so a caller which builds a bind group of its own -- an Effect,
      whose custom set belongs to the user -- knows which entries to fill. */
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
  //! The sets whose buffer bindings carry a dynamic offset, see the ctor.
  std::set<uint32_t> dynamic_sets_;
};

}  // namespace urge

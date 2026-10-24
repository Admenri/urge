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
#include <optional>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include "core/render/gpu_utils.h"
#include "webgpu/webgpu_cpp.hpp"

namespace urge {

struct VertexAttribute {
  uint32_t location = 0;
  wgpu::VertexFormat format = wgpu::VertexFormat::Float32x4;
};

struct ShaderSource {
  std::string_view wgsl;
  std::string_view entry_point;
  std::vector<std::pair<uint32_t, std::vector<util::LayoutEntry>>> groups;
};

inline std::vector<std::vector<VertexAttribute>> QuadVertexLayout() {
  return {{{0, wgpu::VertexFormat::Float32x4},
           {1, wgpu::VertexFormat::Float32x2},
           {2, wgpu::VertexFormat::Float32x4}}};
}

inline std::vector<std::vector<VertexAttribute>> MeshVertexLayout() {
  return {{{0, wgpu::VertexFormat::Float32x3},
           {1, wgpu::VertexFormat::Float32x3},
           {2, wgpu::VertexFormat::Float32x2}}};
}

inline std::vector<std::vector<VertexAttribute>> SkinnedMeshVertexLayout() {
  return {{{0, wgpu::VertexFormat::Float32x3},
           {1, wgpu::VertexFormat::Float32x3},
           {2, wgpu::VertexFormat::Float32x2},
           {3, wgpu::VertexFormat::Uint32x4},
           {4, wgpu::VertexFormat::Float32x4}}};
}

inline std::vector<std::vector<VertexAttribute>> InstancedMeshVertexLayout() {
  return {{{0, wgpu::VertexFormat::Float32x3},
           {1, wgpu::VertexFormat::Float32x3},
           {2, wgpu::VertexFormat::Float32x2}},
          {{5, wgpu::VertexFormat::Float32x4},
           {6, wgpu::VertexFormat::Float32x4},
           {7, wgpu::VertexFormat::Float32x4},
           {8, wgpu::VertexFormat::Float32x4}}};
}

inline std::vector<std::vector<VertexAttribute>>
InstancedSkinnedMeshVertexLayout() {
  return {{{0, wgpu::VertexFormat::Float32x3},
           {1, wgpu::VertexFormat::Float32x3},
           {2, wgpu::VertexFormat::Float32x2},
           {3, wgpu::VertexFormat::Uint32x4},
           {4, wgpu::VertexFormat::Float32x4}},
          {{5, wgpu::VertexFormat::Float32x4},
           {6, wgpu::VertexFormat::Float32x4},
           {7, wgpu::VertexFormat::Float32x4},
           {8, wgpu::VertexFormat::Float32x4}}};
}

inline std::vector<wgpu::VertexStepMode> MeshStepModes(bool instanced) {
  if (!instanced)
    return {wgpu::VertexStepMode::Vertex};
  return {wgpu::VertexStepMode::Vertex, wgpu::VertexStepMode::Instance};
}

class Pipeline {
 public:
  Pipeline(const ShaderSource& vertex,
           const ShaderSource& fragment,
           std::vector<std::vector<VertexAttribute>> vertex_layout,
           const std::set<uint32_t>& dynamic_sets = {},
           std::vector<wgpu::VertexStepMode> step_modes = {});

  static Pipeline AutoLayout(
      const ShaderSource& vertex,
      const ShaderSource& fragment,
      std::vector<std::vector<VertexAttribute>> vertex_layout);

  wgpu::RenderPipeline MakeState(wgpu::PrimitiveState primitive,
                                 std::optional<wgpu::DepthStencilState> depth,
                                 std::vector<wgpu::ColorTargetState> blends,
                                 wgpu::MultisampleState samples = {});

  wgpu::RenderPipeline MakeDefaultState(const wgpu::BlendState* blend);

 private:
  struct VertexBuffer {
    uint64_t stride = 0;
    std::vector<wgpu::VertexAttribute> attributes;
    wgpu::VertexStepMode step_mode = wgpu::VertexStepMode::Vertex;
  };

  Pipeline() = default;

  void Initialize(const ShaderSource& vertex,
                  const ShaderSource& fragment,
                  std::vector<std::vector<VertexAttribute>> vertex_layout,
                  const std::set<uint32_t>* dynamic_sets,
                  std::vector<wgpu::VertexStepMode> step_modes);

  wgpu::ShaderModule vertex_module_;
  wgpu::ShaderModule fragment_module_;
  std::string_view vertex_entry_;
  std::string_view fragment_entry_;
  wgpu::PipelineLayout layout_;
  std::vector<VertexBuffer> buffers_;
};

}

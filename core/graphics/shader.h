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

#include "core/gpu_utils.h"
#include "webgpu/webgpu_cpp.hpp"

namespace urge {

// One vertex input, in the order the vertex buffer feeds it.  `Pipeline` turns
// these into `wgpu::VertexAttribute`s: the offset of each one is the stride the
// ones before it used up, so the declaration order is what fixes the layout.
struct VertexAttribute {
  uint32_t location = 0;
  wgpu::VertexFormat format = wgpu::VertexFormat::Float32x4;
};

// One stage of a pipeline.
//
// The source is WGSL and it reaches the device as it stands -- both wgpu
// backends parse it, so nothing here compiles or scans anything.
//
// `groups` is the part that used to be reflected out of the text, and it is
// written by hand next to the source now.  Each pair is one `@group`: the group
// number first, then the entries inside it, in binding order.  A group the
// stage declares nothing in is simply absent -- the number is carried with the
// entries, so a fragment stage that only samples from `@group(2)` does not have
// to spell out two empty groups in front of it.
//
// `util::CreateBindLayout` turns one of these lists into a
// `wgpu::BindGroupLayout`, and `Pipeline` merges the two stages' lists by group
// number.
struct ShaderSource {
  std::string_view wgsl;
  std::string_view entry_point;
  std::vector<std::pair<uint32_t, std::vector<util::LayoutEntry>>> groups;
};

// The vertex layout every pipeline in the engine takes: position, texture
// coordinate, vertex colour, in that order.  `PrimitiveEmitter` fills the
// buffer in the same shape and core/primitive.h asserts the offsets (16 for the
// coordinate, 24 for the colour), so the two have to stay in step.  Custom
// shaders are handed the same one, which is why an `Effect` has to feed its
// geometry through the emitter rather than inventing its own inputs.
inline std::vector<std::vector<VertexAttribute>> QuadVertexLayout() {
  return {{{0, wgpu::VertexFormat::Float32x4},
           {1, wgpu::VertexFormat::Float32x2},
           {2, wgpu::VertexFormat::Float32x4}}};
}

class Pipeline {
 public:
  // Builds the pipeline layout from the entries the two stages declare.
  // `dynamic_sets` names the groups whose uniform is addressed with a dynamic
  // offset, and the flag lands on every buffer entry of those groups.
  Pipeline(const ShaderSource& vertex,
           const ShaderSource& fragment,
           std::vector<std::vector<VertexAttribute>> vertex_layout,
           const std::set<uint32_t>& dynamic_sets = {});

  // Hands the device a null pipeline layout instead, so it derives one from the
  // two stages itself.  That is what the custom shader path wants: what a
  // user-written shader declares is only knowable once wgpu has read it, and
  // `wgpu::RenderPipeline::GetBindGroupLayout()` hands the result back -- see
  // core/effect.cc.
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
  };

  // Only `AutoLayout` uses this, to get a pipeline it can fill in without a
  // declared layout.
  Pipeline() = default;

  void Initialize(const ShaderSource& vertex,
                  const ShaderSource& fragment,
                  std::vector<std::vector<VertexAttribute>> vertex_layout,
                  const std::set<uint32_t>* dynamic_sets);

  wgpu::ShaderModule vertex_module_;
  wgpu::ShaderModule fragment_module_;
  std::string_view vertex_entry_;
  std::string_view fragment_entry_;
  wgpu::PipelineLayout layout_;
  std::vector<VertexBuffer> buffers_;
};

}  // namespace urge

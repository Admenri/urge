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
#include <string>
#include <vector>

#include "webgpu/webgpu.h"
#include "webgpu/webgpu_cpp.hpp"

namespace urge {

//! One binding of a shader, as the SPIR-V of it describes it.
struct ShaderBinding {
  uint32_t binding = 0;
  //! The kind of buffer a uniform or a storage binding is, or undefined.
  WGPUBufferBindingType buffer = WGPUBufferBindingType_Undefined;
  bool texture = false;
  bool sampler = false;
  //! Size of a uniform block, which a bind group has to be at least as large.
  uint64_t min_binding_size = 0;
};

//! One descriptor set of a shader, in the order its binding appears.
struct ShaderGroup {
  std::vector<ShaderBinding> bindings;
};

//! One vertex input of a shader.
struct ShaderAttribute {
  uint32_t location = 0;
  WGPUVertexFormat format = WGPUVertexFormat_Float32x4;
};

//! What a shader needs, read from the SPIR-V of it.
struct ShaderReflection {
  std::vector<ShaderGroup> groups;
  std::vector<ShaderAttribute> attributes;
};

/**
\brief Compiles a GLSL shader and translates it to the WGSL of the engine.

The engine shaders are written in GLSL, the shaders of a stage are one shader
each:

\code
auto shader = Shader::Compile(wgpu::ShaderStage::Vertex, R"(
  #version 450
  void main() {}
)");
\endcode

The source is compiled to SPIR-V with glslang, the SPIR-V is read by
spirv-reflect, and the WGSL the device is asked for is written from the SPIR-V
by the Tint subset of the tree. Compiling throws urge::Exception of type
Exception::kGPUError when a step of it fails.
*/
class Shader {
 public:
  Shader() = default;

  /*-export.begin-*/
  //! Compiles the GLSL of one stage, see the class comment.
  static Shader Compile(wgpu::ShaderStage stage, std::string_view glsl);
  /*-export.end-*/

  //! The WGSL which was written from the SPIR-V.
  const std::string& wgsl() const { return wgsl_; }
  //! The name of the entry point of the module.
  const std::string& entry_point() const { return entry_point_; }
  //! What the shader binds and reads, see ShaderReflection.
  const ShaderReflection& reflection() const { return reflection_; }

 private:
  std::string wgsl_;
  std::string entry_point_;
  ShaderReflection reflection_;
};

/**
\brief A render pipeline of a vertex and a fragment stage, compiled from GLSL.

The layout of the pipeline and the vertex input of its state are read from the
reflection of the two stages instead of the automatic mode of the device, so
they stay in step with the shaders:

\code
Pipeline pipeline(kVertexGlsl, kFragmentGlsl, {{0, 1}, {2}});
wgpu::RenderPipeline state = pipeline.MakeState(primitive, depth, {blend});
\endcode

The vertex buffer layouts of the constructor say which of the input locations
share one vertex buffer, in the order the buffers are bound: {{0, 1}, {2}}
makes one buffer out of the locations 0 and 1, and a second one out of the
location 2. Within a buffer the attributes are packed in the order of their
location.
*/
class Pipeline {
 public:
  Pipeline(std::string_view vs_glsl,
           std::string_view fs_glsl,
           std::vector<std::vector<uint32_t>> vb_layouts);

  wgpu::RenderPipeline MakeState(wgpu::PrimitiveState primitive,
                                 std::optional<wgpu::DepthStencilState> depth,
                                 std::vector<wgpu::ColorTargetState> blends,
                                 wgpu::MultisampleState samples = {});

 private:
  //! One vertex buffer slot: the stride of it and the attributes it feeds.
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
};

}  // namespace urge

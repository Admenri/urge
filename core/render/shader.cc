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

#include "core/render/shader.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "core/common/exception.h"
#include "core/render/device.h"
#include "core/render/pipeline.h"

namespace urge {

namespace {

[[noreturn]] void Fail(std::string_view what, const std::string& detail) {
  throw Exception(Exception::kGPUError, "shader {} failed: {}", what, detail);
}

wgpu::ShaderModule CreateModule(std::string_view wgsl) {
  wgpu::ShaderSourceWGSL source;
  source.code = wgsl;

  wgpu::ShaderModuleDescriptor module_desc;
  module_desc.nextInChain = &source;

  wgpu::ShaderModule module =
      GPUDevice::Get().device().CreateShaderModule(&module_desc);
  if (module == nullptr)
    Fail("module", "the device rejected the WGSL of the shader");
  return module;
}

uint64_t FormatSize(wgpu::VertexFormat format) {
  switch (format) {
    default:
    case wgpu::VertexFormat::Uint32x2:
    case wgpu::VertexFormat::Sint32x2:
    case wgpu::VertexFormat::Float32x2:
      return 8;
    case wgpu::VertexFormat::Uint32x3:
    case wgpu::VertexFormat::Sint32x3:
    case wgpu::VertexFormat::Float32x3:
      return 12;
    case wgpu::VertexFormat::Uint32x4:
    case wgpu::VertexFormat::Sint32x4:
    case wgpu::VertexFormat::Float32x4:
      return 16;
  }
}

void MarkDynamic(util::LayoutEntry* entry) {
  if (auto* buffer = std::get_if<util::BufferLayout>(&std::get<2>(*entry)))
    if (buffer->type == wgpu::BufferBindingType::Uniform)
      buffer->has_dynamic_offset = true;
}

std::vector<wgpu::BindGroupLayout> CreateGroupLayouts(
    const ShaderSource& vertex,
    const ShaderSource& fragment,
    const std::set<uint32_t>& dynamic_sets) {
  std::map<uint32_t, std::vector<util::LayoutEntry>> merged;
  const auto append = [&merged](const ShaderSource& stage) {
    for (const auto& [set, entries] : stage.groups) {
      std::vector<util::LayoutEntry>& into = merged[set];
      into.insert(into.end(), entries.begin(), entries.end());
    }
  };
  append(vertex);
  append(fragment);

  std::vector<wgpu::BindGroupLayout> layouts;
  const uint32_t group_count = merged.empty() ? 0 : merged.rbegin()->first + 1;

  for (uint32_t group = 0; group < group_count; ++group) {
    std::vector<util::LayoutEntry> entries;
    const auto found = merged.find(group);
    if (found != merged.end())
      entries = found->second;

    if (dynamic_sets.contains(group))
      for (util::LayoutEntry& entry : entries)
        MarkDynamic(&entry);

    wgpu::BindGroupLayout layout = util::CreateBindLayout(entries);
    if (layout == nullptr)
      Fail("layout", "the device rejected a bind group layout of the shader");
    layouts.push_back(std::move(layout));
  }
  return layouts;
}

}

void Pipeline::Initialize(
    const ShaderSource& vertex,
    const ShaderSource& fragment,
    std::vector<std::vector<VertexAttribute>> vertex_layout,
    const std::set<uint32_t>* dynamic_sets,
    std::vector<wgpu::VertexStepMode> step_modes) {
  const wgpu::Device device = GPUDevice::Get().device();

  vertex_module_ = CreateModule(vertex.wgsl);
  if (!fragment.wgsl.empty())
    fragment_module_ = CreateModule(fragment.wgsl);
  vertex_entry_ = vertex.entry_point;
  fragment_entry_ = fragment.entry_point;

  if (dynamic_sets != nullptr) {
    const std::vector<wgpu::BindGroupLayout> groups =
        CreateGroupLayouts(vertex, fragment, *dynamic_sets);

    wgpu::PipelineLayoutDescriptor layout_desc;
    layout_desc.bindGroupLayoutCount = groups.size();
    layout_desc.bindGroupLayouts = groups.empty() ? nullptr : groups.data();
    layout_ = device.CreatePipelineLayout(&layout_desc);
    if (layout_ == nullptr)
      Fail("layout", "the device rejected the pipeline layout of the shader");
  }

  buffers_.reserve(vertex_layout.size());
  for (const std::vector<VertexAttribute>& slot : vertex_layout) {
    VertexBuffer buffer;
    for (const VertexAttribute& attribute : slot) {
      wgpu::VertexAttribute mapped;
      mapped.format = attribute.format;
      mapped.offset = buffer.stride;
      mapped.shaderLocation = attribute.location;
      buffer.attributes.push_back(mapped);
      buffer.stride += FormatSize(attribute.format);
    }
    if (buffers_.size() < step_modes.size())
      buffer.step_mode = step_modes[buffers_.size()];
    buffers_.push_back(std::move(buffer));
  }
}

Pipeline::Pipeline(const ShaderSource& vertex,
                   const ShaderSource& fragment,
                   std::vector<std::vector<VertexAttribute>> vertex_layout,
                   const std::set<uint32_t>& dynamic_sets,
                   std::vector<wgpu::VertexStepMode> step_modes) {
  Initialize(vertex, fragment, std::move(vertex_layout), &dynamic_sets,
             std::move(step_modes));
}

Pipeline Pipeline::AutoLayout(
    const ShaderSource& vertex,
    const ShaderSource& fragment,
    std::vector<std::vector<VertexAttribute>> vertex_layout) {
  Pipeline pipeline;
  pipeline.Initialize(vertex, fragment, std::move(vertex_layout), nullptr, {});
  return pipeline;
}

wgpu::RenderPipeline Pipeline::MakeState(
    wgpu::PrimitiveState primitive,
    std::optional<wgpu::DepthStencilState> depth,
    std::vector<wgpu::ColorTargetState> blends,
    wgpu::MultisampleState samples) {
  std::vector<wgpu::VertexBufferLayout> buffers;
  buffers.reserve(buffers_.size());
  for (const VertexBuffer& buffer : buffers_) {
    wgpu::VertexBufferLayout layout;
    layout.stepMode = buffer.step_mode;
    layout.arrayStride = buffer.stride;
    layout.attributeCount = buffer.attributes.size();
    layout.attributes =
        buffer.attributes.empty() ? nullptr : buffer.attributes.data();
    buffers.push_back(layout);
  }

  wgpu::VertexState vertex_state;
  vertex_state.module = vertex_module_;
  vertex_state.entryPoint = vertex_entry_;
  vertex_state.bufferCount = buffers.size();
  vertex_state.buffers = buffers.empty() ? nullptr : buffers.data();

  wgpu::FragmentState fragment_state;
  fragment_state.module = fragment_module_;
  fragment_state.entryPoint = fragment_entry_;
  fragment_state.targetCount = blends.size();
  fragment_state.targets = blends.empty() ? nullptr : blends.data();

  wgpu::RenderPipelineDescriptor descriptor;
  descriptor.layout = layout_;
  descriptor.vertex = vertex_state;
  descriptor.primitive = primitive;
  descriptor.depthStencil = depth ? &*depth : nullptr;
  descriptor.multisample = samples;
  if (fragment_module_ != nullptr)
    descriptor.fragment = &fragment_state;

  wgpu::RenderPipeline pipeline =
      GPUDevice::Get().device().CreateRenderPipeline(&descriptor);
  if (pipeline == nullptr)
    Fail("state", "the device rejected the state of the render pipeline");

  return pipeline;
}

wgpu::RenderPipeline Pipeline::MakeDefaultState(const wgpu::BlendState* blend) {
  wgpu::ColorTargetState target;
  target.format = kColorTargetFormat;
  target.blend = blend;

  return MakeState(GetDefaultPrimitiveState(), *GetDepthStencilState(),
                   {target});
}

}

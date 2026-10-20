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

#include "core/scene3d/world3d.h"

#include <algorithm>
#include <utility>

#include "core/common/exception.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/render/pipeline.h"
#include "core/scene3d/render_context3d.h"

namespace urge {

namespace {

constexpr std::string_view kQuadShader = R"(
struct Quad {
  rect: vec4<f32>,
  viewport: vec4<f32>,
};

@group(0) @binding(0) var<uniform> u_quad: Quad;

struct VertexOutput {
  @builtin(position) position: vec4<f32>,
  @location(0) v_uv: vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) index: u32) -> VertexOutput {
  var corners = array<vec2<f32>, 6>(
    vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 0.0), vec2<f32>(1.0, 1.0),
    vec2<f32>(0.0, 0.0), vec2<f32>(1.0, 1.0), vec2<f32>(0.0, 1.0),
  );

  let corner = corners[index];
  let pixel = u_quad.rect.xy + corner * u_quad.rect.zw;
  let ndc = vec2<f32>(pixel.x * u_quad.viewport.z * 2.0 - 1.0,
                      1.0 - pixel.y * u_quad.viewport.w * 2.0);

  var output: VertexOutput;
  output.position = vec4<f32>(ndc, 0.0, 1.0);
  output.v_uv = corner;
  return output;
}

@group(1) @binding(0) var u_texture: texture_2d<f32>;
@group(1) @binding(1) var u_sampler: sampler;

@fragment
fn fs_main(input: VertexOutput) -> @location(0) vec4<f32> {
  return textureSample(u_texture, u_sampler, input.v_uv);
}
)";

}

World3D::World3D(int32_t x, int32_t y, int32_t width, int32_t height)
    : x_(x),
      y_(y),
      width_(std::max(1, width)),
      height_(std::max(1, height)) {
  Node::SetupTrait(this);
}

World3D::~World3D() {
  Disposable::Dispose();
}

void World3D::DisposeObject() {
  Node::DisposeObject();
  target_.reset();
  root_.reset();
  graph_.reset();
  camera_.reset();
}

void World3D::EnsureQuadPipeline() {
  if (quad_pipeline_)
    return;

  const wgpu::Device device = GPUDevice::Get().device();

  static const std::string shader_source(kQuadShader);

  wgpu::ShaderSourceWGSL source;
  source.code = wgpu::StringView(shader_source);
  wgpu::ShaderModuleDescriptor module_desc;
  module_desc.nextInChain = &source;
  const wgpu::ShaderModule module = device.CreateShaderModule(&module_desc);

  const std::vector<util::LayoutEntry> quad_entries = {
      {0, wgpu::ShaderStage::Vertex,
       util::BufferLayout::Uniform(false, 32)},
  };
  const std::vector<util::LayoutEntry> texture_entries = {
      {0, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
      {1, wgpu::ShaderStage::Fragment, util::SamplerLayout::Filtering()},
  };

  wgpu::BindGroupLayout layouts[2] = {
      util::CreateBindLayout(quad_entries),
      util::CreateBindLayout(texture_entries),
  };

  wgpu::PipelineLayoutDescriptor layout_desc;
  layout_desc.bindGroupLayoutCount = 2;
  layout_desc.bindGroupLayouts = layouts;
  const wgpu::PipelineLayout pipeline_layout =
      device.CreatePipelineLayout(&layout_desc);

  wgpu::DepthStencilState depth;
  depth.format = wgpu::TextureFormat::Depth24PlusStencil8;
  depth.depthWriteEnabled = wgpu::OptionalBool::False;
  depth.depthCompare = wgpu::CompareFunction::Always;

  wgpu::ColorTargetState target;
  target.format = kColorTargetFormat;

  wgpu::FragmentState fragment;
  fragment.module = module;
  fragment.entryPoint = wgpu::StringView("fs_main");
  fragment.targetCount = 1;
  fragment.targets = &target;

  wgpu::RenderPipelineDescriptor desc;
  desc.layout = pipeline_layout;
  desc.vertex.module = module;
  desc.vertex.entryPoint = wgpu::StringView("vs_main");
  desc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
  desc.depthStencil = &depth;
  desc.fragment = &fragment;

  quad_pipeline_ = device.CreateRenderPipeline(&desc);
  if (!quad_pipeline_)
    throw Exception(Exception::kGPUError,
                    "world3d: the device rejected the blit pipeline.");

  wgpu::BufferDescriptor uniform_desc;
  uniform_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  uniform_desc.size = 32;
  quad_uniform_ = device.CreateBuffer(&uniform_desc);

  wgpu::BindGroupEntry quad_entry = {};
  quad_entry.binding = 0;
  quad_entry.buffer = quad_uniform_;
  quad_entry.offset = 0;
  quad_entry.size = 32;

  wgpu::BindGroupDescriptor quad_group_desc;
  quad_group_desc.layout = layouts[0];
  quad_group_desc.entryCount = 1;
  quad_group_desc.entries = &quad_entry;
  quad_group_ = device.CreateBindGroup(&quad_group_desc);

  wgpu::SamplerDescriptor sampler_desc;
  sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
  sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
  sampler_desc.magFilter = wgpu::FilterMode::Linear;
  sampler_desc.minFilter = wgpu::FilterMode::Linear;
  sampler_ = device.CreateSampler(&sampler_desc);
}

void World3D::UpdateQuad(glm::ivec2 target_size) {
  if (quad_uniform_ == nullptr)
    return;

  struct QuadData {
    glm::vec4 rect;
    glm::vec4 target;
  } data;

  data.rect = glm::vec4(static_cast<float>(x_), static_cast<float>(y_),
                        static_cast<float>(width_), static_cast<float>(height_));
  const glm::vec2 size(static_cast<float>(std::max(1, target_size.x)),
                       static_cast<float>(std::max(1, target_size.y)));
  data.target = glm::vec4(size, 1.0f / size.x, 1.0f / size.y);

  GPUDevice::Get().queue().WriteBuffer(quad_uniform_, 0, &data, sizeof(data));

  if (texture_group_ == nullptr && target_) {
    wgpu::BindGroupEntry entries[2] = {};
    entries[0].binding = 0;
    entries[0].textureView = target_->color_view();
    entries[1].binding = 1;
    entries[1].sampler = sampler_;

    wgpu::BindGroupDescriptor desc;
    desc.layout = quad_pipeline_.GetBindGroupLayout(1);
    desc.entryCount = 2;
    desc.entries = entries;
    texture_group_ = GPUDevice::Get().device().CreateBindGroup(&desc);
    if (!texture_group_)
      throw Exception(Exception::kGPUError,
                      "world3d: the device rejected the texture group.");
  }
}

void World3D::RenderScene(DrawParam param) {
  const glm::ivec2 size(width_, height_);
  if (!target_ || target_->Width() != size.x || target_->Height() != size.y) {
    target_ = MakeRefCounted<RenderTarget3D>(size.x, size.y, true);
    texture_group_ = nullptr;
  }

  RenderContext3D context(param->command, target_);
  context.Collect(root_.get(), camera_.get());
  context.Flush();
  graph_->Execute(&context);
}

bool World3D::Prepare(DrawParam param) {
  if (!root_ || !graph_ || !camera_)
    return false;
  if (width_ <= 0 || height_ <= 0)
    return false;

  EnsureQuadPipeline();
  RenderScene(param);
  UpdateQuad(param->target->size());

  return texture_group_ != nullptr;
}

bool World3D::DoDraw(DrawParam param) {
  if (!quad_pipeline_ || !texture_group_)
    return false;

  param->pass.SetPipeline(quad_pipeline_);
  param->pass.SetBindGroup(0, quad_group_, 0, nullptr);
  param->pass.SetBindGroup(1, texture_group_, 0, nullptr);
  param->pass.Draw(6, 1, 0, 0);

  return false;
}

}

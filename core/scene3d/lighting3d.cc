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

#include "core/scene3d/lighting3d.h"

#include <vector>

#include "core/render/device.h"
#include "core/render/gpu_utils.h"

namespace urge {

namespace {

constexpr auto kBothStages = static_cast<wgpu::ShaderStage>(
    static_cast<uint32_t>(wgpu::ShaderStage::Vertex) |
    static_cast<uint32_t>(wgpu::ShaderStage::Fragment));

}

std::vector<util::LayoutEntry> ViewEntries() {
  return {
      {0, kBothStages, util::BufferLayout::Uniform(true, sizeof(ViewData))},
      {1, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Depth(wgpu::TextureViewDimension::e2D)},
      {2, wgpu::ShaderStage::Fragment, util::SamplerLayout::Comparison()},
      {3, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Depth(wgpu::TextureViewDimension::e2DArray)},
      {4, wgpu::ShaderStage::Fragment, util::SamplerLayout::Comparison()},
      {5, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Depth(wgpu::TextureViewDimension::Cube)},
      {6, wgpu::ShaderStage::Fragment, util::SamplerLayout::Comparison()},
  };
}

std::vector<util::LayoutEntry> ShadowViewEntries() {
  return {
      {0, wgpu::ShaderStage::Vertex,
       util::BufferLayout::Uniform(true, sizeof(ViewData))},
  };
}

std::vector<util::LayoutEntry> MeshEntries() {
  return {
      {0, kBothStages, util::BufferLayout::Uniform(true, sizeof(MeshData))},
      {1, kBothStages, util::BufferLayout::ReadOnlyStorage()},
  };
}

std::vector<util::LayoutEntry> MeshUniformEntries() {
  return {
      {0, wgpu::ShaderStage::Vertex,
       util::BufferLayout::Uniform(true, sizeof(MeshData))},
  };
}

std::vector<util::LayoutEntry> GlobalsEntries() {
  return {
      {0, kBothStages, util::BufferLayout::Uniform(false, sizeof(GlobalsData))},
  };
}

std::vector<util::LayoutEntry> MaterialEntries() {
  constexpr uint64_t kParamBytes = 16 * sizeof(float);

  return {
      {0, kBothStages, util::BufferLayout::Uniform(false, kParamBytes)},
      {1, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
      {2, wgpu::ShaderStage::Fragment, util::SamplerLayout::Filtering()},
      {3, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
      {4, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
      {5, wgpu::ShaderStage::Fragment,
       util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
      {6, wgpu::ShaderStage::Fragment, util::SamplerLayout::Filtering()},
  };
}

wgpu::BindGroupLayout AcquireViewLayout() {
  static wgpu::BindGroupLayout layout = util::CreateBindLayout(ViewEntries());
  return layout;
}

wgpu::BindGroupLayout AcquireShadowViewLayout() {
  static wgpu::BindGroupLayout layout =
      util::CreateBindLayout(ShadowViewEntries());
  return layout;
}

wgpu::BindGroupLayout AcquireMeshLayout() {
  static wgpu::BindGroupLayout layout = util::CreateBindLayout(MeshEntries());
  return layout;
}

wgpu::BindGroupLayout AcquireMeshUniformLayout() {
  static wgpu::BindGroupLayout layout =
      util::CreateBindLayout(MeshUniformEntries());
  return layout;
}

wgpu::BindGroupLayout AcquireGlobalsLayout() {
  static wgpu::BindGroupLayout layout =
      util::CreateBindLayout(GlobalsEntries());
  return layout;
}

wgpu::TextureView AcquireEmptyShadowView() {
  static wgpu::TextureView view = nullptr;
  if (view != nullptr)
    return view;

  wgpu::TextureDescriptor desc;
  desc.usage = wgpu::TextureUsage::TextureBinding |
               wgpu::TextureUsage::RenderAttachment;
  desc.dimension = wgpu::TextureDimension::e2D;
  desc.size.width = 1;
  desc.size.height = 1;
  desc.format = wgpu::TextureFormat::Depth32Float;

  const wgpu::Texture texture = GPUDevice::Get().device().CreateTexture(&desc);
  view = texture.CreateView(nullptr);
  return view;
}

wgpu::Buffer AcquireDummyJointBuffer() {
  static wgpu::Buffer buffer = nullptr;
  if (buffer != nullptr)
    return buffer;

  wgpu::BufferDescriptor desc;
  desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
  desc.size = sizeof(glm::mat4);
  buffer = GPUDevice::Get().device().CreateBuffer(&desc);
  if (buffer == nullptr)
    return nullptr;

  const glm::mat4 identity(1.0f);
  GPUDevice::Get().queue().WriteBuffer(buffer, 0, &identity, sizeof(identity));
  return buffer;
}

wgpu::TextureView AcquireEmptyShadowArrayView() {
  static wgpu::TextureView view = nullptr;
  if (view != nullptr)
    return view;

  wgpu::TextureDescriptor desc;
  desc.usage = wgpu::TextureUsage::TextureBinding |
               wgpu::TextureUsage::RenderAttachment;
  desc.dimension = wgpu::TextureDimension::e2D;
  desc.size.width = 1;
  desc.size.height = 1;
  desc.size.depthOrArrayLayers = 1;
  desc.format = wgpu::TextureFormat::Depth32Float;

  const wgpu::Texture texture = GPUDevice::Get().device().CreateTexture(&desc);

  wgpu::TextureViewDescriptor array_desc;
  array_desc.dimension = wgpu::TextureViewDimension::e2DArray;
  array_desc.baseArrayLayer = 0;
  array_desc.arrayLayerCount = 1;
  view = texture.CreateView(&array_desc);
  return view;
}

wgpu::TextureView AcquireEmptyShadowCubeView() {
  static wgpu::TextureView view = nullptr;
  if (view != nullptr)
    return view;

  wgpu::TextureDescriptor desc;
  desc.usage = wgpu::TextureUsage::TextureBinding |
               wgpu::TextureUsage::RenderAttachment;
  desc.dimension = wgpu::TextureDimension::e2D;
  desc.size.width = 1;
  desc.size.height = 1;
  desc.size.depthOrArrayLayers = 6;
  desc.format = wgpu::TextureFormat::Depth32Float;

  const wgpu::Texture texture = GPUDevice::Get().device().CreateTexture(&desc);

  wgpu::TextureViewDescriptor cube_desc;
  cube_desc.dimension = wgpu::TextureViewDimension::Cube;
  cube_desc.baseArrayLayer = 0;
  cube_desc.arrayLayerCount = 6;
  view = texture.CreateView(&cube_desc);
  return view;
}

wgpu::Sampler AcquireShadowSampler() {
  static wgpu::Sampler sampler = nullptr;
  if (sampler != nullptr)
    return sampler;

  wgpu::SamplerDescriptor desc;
  desc.addressModeU = wgpu::AddressMode::ClampToEdge;
  desc.addressModeV = wgpu::AddressMode::ClampToEdge;
  desc.addressModeW = wgpu::AddressMode::ClampToEdge;
  desc.magFilter = wgpu::FilterMode::Linear;
  desc.minFilter = wgpu::FilterMode::Linear;
  desc.compare = wgpu::CompareFunction::LessEqual;
  sampler = GPUDevice::Get().device().CreateSampler(&desc);
  return sampler;
}

}

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

#include "webgpu/webgpu_cpp.hpp"

#include "core/definition.h"
#include "core/device.h"
#include "core/object.h"
#include "core/refptr.h"

namespace urge {

class GPUBuffer;
class GPUTexture;
class GPUTextureView;
class GPUSampler;
class GPUShaderModule;
class GPUBindGroupLayout;
class GPUPipelineLayout;
class GPUBindGroup;
class GPURenderPipeline;
class GPUComputePipeline;
class GPUQuerySet;
class GPUCommandBuffer;
class GPUCommandEncoder;
class GPURenderPassEncoder;
class GPUComputePassEncoder;
class GPUQueue;

URGE_BINDING()
class GPUObject : public Object {
 public:
  GPUObject() = default;
};

URGE_BINDING()
class GPUBufferDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUBufferDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING(Name : "size")
  uint64_t GetSize() { return size; }
  URGE_BINDING(Name : "size=")
  void SetSize(uint64_t value) { size = value; }

  URGE_BINDING()
  ATTR(uint32_t, Usage) {
    if (value)
      usage = *value;
    return usage;
  }

  URGE_BINDING()
  ATTR(bool, MappedAtCreation) {
    if (value)
      mapped_at_creation = *value;
    return mapped_at_creation;
  }

 public:
  std::string label;
  uint64_t size = 0;
  uint32_t usage = 0;
  bool mapped_at_creation = false;
};

URGE_BINDING()
class GPUTextureDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUTextureDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(uint32_t, Usage) {
    if (value)
      usage = *value;
    return usage;
  }
  URGE_BINDING()
  ATTR(uint32_t, Dimension) {
    if (value)
      dimension = *value;
    return dimension;
  }
  URGE_BINDING()
  ATTR(uint32_t, Format) {
    if (value)
      format = *value;
    return format;
  }
  URGE_BINDING()
  ATTR(uint32_t, Width) {
    if (value)
      width = *value;
    return width;
  }
  URGE_BINDING()
  ATTR(uint32_t, Height) {
    if (value)
      height = *value;
    return height;
  }
  URGE_BINDING()
  ATTR(uint32_t, DepthOrArrayLayers) {
    if (value)
      depth_or_array_layers = *value;
    return depth_or_array_layers;
  }
  URGE_BINDING()
  ATTR(uint32_t, MipLevelCount) {
    if (value)
      mip_level_count = *value;
    return mip_level_count;
  }
  URGE_BINDING()
  ATTR(uint32_t, SampleCount) {
    if (value)
      sample_count = *value;
    return sample_count;
  }

 public:
  std::string label;
  uint32_t usage = 0;
  uint32_t dimension = 0;
  uint32_t format = 0;
  uint32_t width = 1;
  uint32_t height = 1;
  uint32_t depth_or_array_layers = 1;
  uint32_t mip_level_count = 1;
  uint32_t sample_count = 1;
};

URGE_BINDING()
class GPUTextureViewDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUTextureViewDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(uint32_t, Format) {
    if (value)
      format = *value;
    return format;
  }
  URGE_BINDING()
  ATTR(uint32_t, Dimension) {
    if (value)
      dimension = *value;
    return dimension;
  }
  URGE_BINDING()
  ATTR(uint32_t, BaseMipLevel) {
    if (value)
      base_mip_level = *value;
    return base_mip_level;
  }
  URGE_BINDING()
  ATTR(uint32_t, MipLevelCount) {
    if (value)
      mip_level_count = *value;
    return mip_level_count;
  }
  URGE_BINDING()
  ATTR(uint32_t, BaseArrayLayer) {
    if (value)
      base_array_layer = *value;
    return base_array_layer;
  }
  URGE_BINDING()
  ATTR(uint32_t, ArrayLayerCount) {
    if (value)
      array_layer_count = *value;
    return array_layer_count;
  }
  URGE_BINDING()
  ATTR(uint32_t, Aspect) {
    if (value)
      aspect = *value;
    return aspect;
  }

 public:
  std::string label;
  uint32_t format = 0;
  uint32_t dimension = 0;
  uint32_t base_mip_level = 0;
  uint32_t mip_level_count = 0xFFFFFFFFu;
  uint32_t base_array_layer = 0;
  uint32_t array_layer_count = 0xFFFFFFFFu;
  uint32_t aspect = 0;
};

URGE_BINDING()
class GPUSamplerDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUSamplerDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(uint32_t, AddressModeU) {
    if (value)
      address_mode_u = *value;
    return address_mode_u;
  }
  URGE_BINDING()
  ATTR(uint32_t, AddressModeV) {
    if (value)
      address_mode_v = *value;
    return address_mode_v;
  }
  URGE_BINDING()
  ATTR(uint32_t, AddressModeW) {
    if (value)
      address_mode_w = *value;
    return address_mode_w;
  }
  URGE_BINDING()
  ATTR(uint32_t, MagFilter) {
    if (value)
      mag_filter = *value;
    return mag_filter;
  }
  URGE_BINDING()
  ATTR(uint32_t, MinFilter) {
    if (value)
      min_filter = *value;
    return min_filter;
  }
  URGE_BINDING()
  ATTR(uint32_t, MipmapFilter) {
    if (value)
      mipmap_filter = *value;
    return mipmap_filter;
  }
  URGE_BINDING()
  ATTR(float, LodMinClamp) {
    if (value)
      lod_min_clamp = *value;
    return lod_min_clamp;
  }
  URGE_BINDING()
  ATTR(float, LodMaxClamp) {
    if (value)
      lod_max_clamp = *value;
    return lod_max_clamp;
  }
  URGE_BINDING()
  ATTR(uint32_t, Compare) {
    if (value)
      compare = *value;
    return compare;
  }
  URGE_BINDING()
  ATTR(uint32_t, MaxAnisotropy) {
    if (value)
      max_anisotropy = *value;
    return max_anisotropy;
  }

 public:
  std::string label;
  uint32_t address_mode_u = 0;
  uint32_t address_mode_v = 0;
  uint32_t address_mode_w = 0;
  uint32_t mag_filter = 0;
  uint32_t min_filter = 0;
  uint32_t mipmap_filter = 0;
  float lod_min_clamp = 0.f;
  float lod_max_clamp = 32.f;
  uint32_t compare = 0;
  uint32_t max_anisotropy = 1;
};

URGE_BINDING()
class GPUShaderModuleDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUShaderModuleDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING(Name : "code")
  std::string GetCode() { return code; }
  URGE_BINDING(Name : "code=")
  void SetCode(std::string value) { code = value; }

  URGE_BINDING()
  ATTR(uint32_t, Language) {
    if (value)
      language = *value;
    return language;
  }

  URGE_BINDING()
  ATTR(uint32_t, Stage) {
    if (value)
      stage = *value;
    return stage;
  }

  URGE_BINDING(Name : "entry_point")
  std::string GetEntryPoint() { return entry_point; }
  URGE_BINDING(Name : "entry_point=")
  void SetEntryPoint(std::string value) { entry_point = value; }

 public:
  std::string label;
  std::string code;
  uint32_t language = 0;
  uint32_t stage = 0;
  std::string entry_point = "main";
};

URGE_BINDING()
class GPUBindGroupLayoutDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUBindGroupLayoutDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  void AddUniform(uint32_t binding, uint32_t visibility);
  URGE_BINDING()
  void AddStorage(uint32_t binding,
                  uint32_t visibility,
                  bool read_only = false);
  URGE_BINDING()
  void AddTexture(uint32_t binding, uint32_t visibility);
  URGE_BINDING()
  void AddSampler(uint32_t binding, uint32_t visibility);

 public:
  std::string label;
  std::vector<wgpu::BindGroupLayoutEntry> entries;
};

URGE_BINDING()
class GPUPipelineLayoutDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUPipelineLayoutDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(uint32_t, ImmediateSize) {
    if (value)
      immediate_size = *value;
    return immediate_size;
  }

  URGE_BINDING()
  void AddBindGroupLayout(RefPtr<GPUBindGroupLayout> layout);

 public:
  std::string label;
  uint32_t immediate_size = 0;
  std::vector<wgpu::BindGroupLayout> layouts;
};

URGE_BINDING()
class GPUBindGroupDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUBindGroupDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(RefPtr<GPUBindGroupLayout>, Layout) {
    if (value)
      layout = *value;
    return layout;
  }

  URGE_BINDING()
  void AddBuffer(uint32_t binding, RefPtr<GPUBuffer> buffer);
  URGE_BINDING()
  void AddTexture(uint32_t binding, RefPtr<GPUTextureView> view);
  URGE_BINDING()
  void AddSampler(uint32_t binding, RefPtr<GPUSampler> sampler);

 public:
  std::string label;
  RefPtr<GPUBindGroupLayout> layout;
  std::vector<wgpu::BindGroupEntry> entries;
};

URGE_BINDING()
class GPUComputePipelineDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUComputePipelineDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(RefPtr<GPUPipelineLayout>, Layout) {
    if (value)
      layout = *value;
    return layout;
  }

  URGE_BINDING()
  ATTR(RefPtr<GPUShaderModule>, Module) {
    if (value)
      module = *value;
    return module;
  }

  URGE_BINDING(Name : "entry_point")
  std::string GetEntryPoint() { return entry_point; }
  URGE_BINDING(Name : "entry_point=")
  void SetEntryPoint(std::string value) { entry_point = value; }

 public:
  std::string label;
  RefPtr<GPUPipelineLayout> layout;
  RefPtr<GPUShaderModule> module;
  std::string entry_point;
};

URGE_BINDING()
class GPURenderPipelineDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPURenderPipelineDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(RefPtr<GPUPipelineLayout>, Layout) {
    if (value)
      layout = *value;
    return layout;
  }

  URGE_BINDING()
  ATTR(RefPtr<GPUShaderModule>, VertexModule) {
    if (value)
      vertex_module = *value;
    return vertex_module;
  }

  URGE_BINDING(Name : "vertex_entry_point")
  std::string GetVertexEntryPoint() { return vertex_entry_point; }
  URGE_BINDING(Name : "vertex_entry_point=")
  void SetVertexEntryPoint(std::string value) { vertex_entry_point = value; }

  URGE_BINDING()
  ATTR(RefPtr<GPUShaderModule>, FragmentModule) {
    if (value)
      fragment_module = *value;
    return fragment_module;
  }

  URGE_BINDING(Name : "fragment_entry_point")
  std::string GetFragmentEntryPoint() { return fragment_entry_point; }
  URGE_BINDING(Name : "fragment_entry_point=")
  void SetFragmentEntryPoint(std::string value) {
    fragment_entry_point = value;
  }

  URGE_BINDING()
  ATTR(uint32_t, Topology) {
    if (value)
      topology = *value;
    return topology;
  }

  URGE_BINDING()
  ATTR(uint32_t, Format) {
    if (value)
      format = *value;
    return format;
  }

  URGE_BINDING()
  ATTR(uint32_t, VertexStride) {
    if (value)
      vertex_stride = *value;
    return vertex_stride;
  }

  URGE_BINDING()
  void AddVertexAttribute(uint32_t location, uint32_t format, uint64_t offset);

 public:
  std::string label;
  RefPtr<GPUPipelineLayout> layout;
  RefPtr<GPUShaderModule> vertex_module;
  std::string vertex_entry_point = "main";
  RefPtr<GPUShaderModule> fragment_module;
  std::string fragment_entry_point = "main";
  uint32_t topology =
      static_cast<uint32_t>(wgpu::PrimitiveTopology::TriangleList);
  uint32_t format = 0;
  uint32_t vertex_stride = 0;
  std::vector<wgpu::VertexAttribute> attributes;
};

URGE_BINDING()
class GPURenderPassDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPURenderPassDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(RefPtr<GPUTextureView>, ColorView) {
    if (value)
      color_view = *value;
    return color_view;
  }

  URGE_BINDING()
  ATTR(uint32_t, LoadOp) {
    if (value)
      load_op = *value;
    return load_op;
  }

  URGE_BINDING()
  ATTR(uint32_t, StoreOp) {
    if (value)
      store_op = *value;
    return store_op;
  }

  URGE_BINDING()
  ATTR(float, ClearR) {
    if (value)
      clear_r = *value;
    return clear_r;
  }
  URGE_BINDING()
  ATTR(float, ClearG) {
    if (value)
      clear_g = *value;
    return clear_g;
  }
  URGE_BINDING()
  ATTR(float, ClearB) {
    if (value)
      clear_b = *value;
    return clear_b;
  }
  URGE_BINDING()
  ATTR(float, ClearA) {
    if (value)
      clear_a = *value;
    return clear_a;
  }

 public:
  std::string label;
  RefPtr<GPUTextureView> color_view;
  uint32_t load_op = 1;
  uint32_t store_op = 1;
  float clear_r = 0.f;
  float clear_g = 0.f;
  float clear_b = 0.f;
  float clear_a = 1.f;
};

URGE_BINDING()
class GPUComputePassDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUComputePassDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

 public:
  std::string label;
};

URGE_BINDING()
class GPUQuerySetDescriptor : public GPUObject {
 public:
  URGE_BINDING()
  GPUQuerySetDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  ATTR(uint32_t, Type) {
    if (value)
      type = *value;
    return type;
  }

  URGE_BINDING()
  ATTR(uint32_t, Count) {
    if (value)
      count = *value;
    return count;
  }

 public:
  std::string label;
  uint32_t type = 0;
  uint32_t count = 0;
};

URGE_BINDING()
class GPUBuffer : public GPUObject {
 public:
  GPUBuffer(wgpu::Buffer buffer) : buffer_(buffer) {}

  URGE_BINDING(Name : "size")
  uint64_t GetSize() { return buffer_.GetSize(); }
  URGE_BINDING()
  ATTR(uint32_t, Usage) { return static_cast<uint32_t>(buffer_.GetUsage()); }
  URGE_BINDING()
  ATTR(uint32_t, MapState) {
    return static_cast<uint32_t>(buffer_.GetMapState());
  }

  URGE_BINDING()
  bool MapAsync(uint32_t mode, uint64_t offset, uint64_t size);
  URGE_BINDING()
  void Unmap() { buffer_.Unmap(); }
  URGE_BINDING()
  std::string Read(uint64_t offset, uint64_t size);
  URGE_BINDING()
  void Write(uint64_t offset, std::string data);
  URGE_BINDING()
  void Destroy() { buffer_.Destroy(); }

  wgpu::Buffer handle() const { return buffer_; }

 private:
  wgpu::Buffer buffer_;
};

URGE_BINDING()
class GPUTexture : public GPUObject {
 public:
  GPUTexture(wgpu::Texture texture) : texture_(texture) {}

  URGE_BINDING()
  uint32_t Width() { return texture_.GetWidth(); }
  URGE_BINDING()
  uint32_t Height() { return texture_.GetHeight(); }
  URGE_BINDING()
  uint32_t DepthOrArrayLayers() { return texture_.GetDepthOrArrayLayers(); }
  URGE_BINDING()
  uint32_t MipLevelCount() { return texture_.GetMipLevelCount(); }
  URGE_BINDING()
  uint32_t SampleCount() { return texture_.GetSampleCount(); }
  URGE_BINDING()
  uint32_t Dimension() {
    return static_cast<uint32_t>(texture_.GetDimension());
  }
  URGE_BINDING()
  uint32_t Format() { return static_cast<uint32_t>(texture_.GetFormat()); }
  URGE_BINDING()
  uint32_t Usage() { return static_cast<uint32_t>(texture_.GetUsage()); }

  URGE_BINDING()
  RefPtr<GPUTextureView> CreateView(
      RefPtr<GPUTextureViewDescriptor> descriptor = nullptr);
  URGE_BINDING()
  void Destroy() { texture_.Destroy(); }

  wgpu::Texture handle() const { return texture_; }

 private:
  wgpu::Texture texture_;
};

URGE_BINDING()
class GPUTextureView : public GPUObject {
 public:
  GPUTextureView(wgpu::TextureView view) : view_(view) {}

  wgpu::TextureView handle() const { return view_; }

 private:
  wgpu::TextureView view_;
};

URGE_BINDING()
class GPUSampler : public GPUObject {
 public:
  GPUSampler(wgpu::Sampler sampler) : sampler_(sampler) {}

  wgpu::Sampler handle() const { return sampler_; }

 private:
  wgpu::Sampler sampler_;
};

URGE_BINDING()
class GPUShaderModule : public GPUObject {
 public:
  GPUShaderModule(wgpu::ShaderModule module) : module_(module) {}

  wgpu::ShaderModule handle() const { return module_; }

 private:
  wgpu::ShaderModule module_;
};

URGE_BINDING()
class GPUBindGroupLayout : public GPUObject {
 public:
  GPUBindGroupLayout(wgpu::BindGroupLayout layout) : layout_(layout) {}

  wgpu::BindGroupLayout handle() const { return layout_; }

 private:
  wgpu::BindGroupLayout layout_;
};

URGE_BINDING()
class GPUPipelineLayout : public GPUObject {
 public:
  GPUPipelineLayout(wgpu::PipelineLayout layout) : layout_(layout) {}

  wgpu::PipelineLayout handle() const { return layout_; }

 private:
  wgpu::PipelineLayout layout_;
};

URGE_BINDING()
class GPUBindGroup : public GPUObject {
 public:
  GPUBindGroup(wgpu::BindGroup group) : group_(group) {}

  wgpu::BindGroup handle() const { return group_; }

 private:
  wgpu::BindGroup group_;
};

URGE_BINDING()
class GPURenderPipeline : public GPUObject {
 public:
  GPURenderPipeline(wgpu::RenderPipeline pipeline) : pipeline_(pipeline) {}

  URGE_BINDING()
  RefPtr<GPUBindGroupLayout> GetBindGroupLayout(uint32_t group_index);

  wgpu::RenderPipeline handle() const { return pipeline_; }

 private:
  wgpu::RenderPipeline pipeline_;
};

URGE_BINDING()
class GPUComputePipeline : public GPUObject {
 public:
  GPUComputePipeline(wgpu::ComputePipeline pipeline) : pipeline_(pipeline) {}

  URGE_BINDING()
  RefPtr<GPUBindGroupLayout> GetBindGroupLayout(uint32_t group_index);

  wgpu::ComputePipeline handle() const { return pipeline_; }

 private:
  wgpu::ComputePipeline pipeline_;
};

URGE_BINDING()
class GPUQuerySet : public GPUObject {
 public:
  GPUQuerySet(wgpu::QuerySet query_set) : query_set_(query_set) {}

  URGE_BINDING()
  uint32_t Type() { return static_cast<uint32_t>(query_set_.GetType()); }
  URGE_BINDING()
  uint32_t Count() { return query_set_.GetCount(); }
  URGE_BINDING()
  void Destroy() { query_set_.Destroy(); }

  wgpu::QuerySet handle() const { return query_set_; }

 private:
  wgpu::QuerySet query_set_;
};

URGE_BINDING()
class GPUCommandBuffer : public GPUObject {
 public:
  GPUCommandBuffer(wgpu::CommandBuffer command_buffer)
      : command_buffer_(command_buffer) {}

  wgpu::CommandBuffer handle() const { return command_buffer_; }

 private:
  wgpu::CommandBuffer command_buffer_;
};

URGE_BINDING()
class GPUCommandEncoder : public GPUObject {
 public:
  GPUCommandEncoder(wgpu::CommandEncoder encoder) : encoder_(encoder) {}

  URGE_BINDING()
  RefPtr<GPURenderPassEncoder> BeginRenderPass(
      RefPtr<GPURenderPassDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUComputePassEncoder> BeginComputePass(
      RefPtr<GPUComputePassDescriptor> descriptor = nullptr);
  URGE_BINDING()
  void CopyBufferToBuffer(RefPtr<GPUBuffer> source,
                          uint64_t source_offset,
                          RefPtr<GPUBuffer> destination,
                          uint64_t destination_offset,
                          uint64_t size);
  URGE_BINDING()
  void ClearBuffer(RefPtr<GPUBuffer> buffer, uint64_t offset, uint64_t size);
  URGE_BINDING()
  RefPtr<GPUCommandBuffer> Finish();

  wgpu::CommandEncoder handle() const { return encoder_; }

 private:
  wgpu::CommandEncoder encoder_;
};

URGE_BINDING()
class GPURenderPassEncoder : public GPUObject {
 public:
  GPURenderPassEncoder(wgpu::RenderPassEncoder encoder) : encoder_(encoder) {}

  URGE_BINDING()
  void SetPipeline(RefPtr<GPURenderPipeline> pipeline);
  URGE_BINDING()
  void SetBindGroup(uint32_t index, RefPtr<GPUBindGroup> group);
  URGE_BINDING()
  void SetVertexBuffer(uint32_t slot,
                       RefPtr<GPUBuffer> buffer,
                       uint64_t offset = 0);
  URGE_BINDING()
  void SetIndexBuffer(RefPtr<GPUBuffer> buffer,
                      uint32_t format,
                      uint64_t offset = 0);
  URGE_BINDING()
  void Draw(uint32_t vertex_count,
            uint32_t instance_count = 1,
            uint32_t first_vertex = 0,
            uint32_t first_instance = 0);
  URGE_BINDING()
  void DrawIndexed(uint32_t index_count,
                   uint32_t instance_count = 1,
                   uint32_t first_index = 0,
                   int32_t base_vertex = 0,
                   uint32_t first_instance = 0);
  URGE_BINDING()
  void SetViewport(float x,
                   float y,
                   float width,
                   float height,
                   float min_depth = 0.f,
                   float max_depth = 1.f);
  URGE_BINDING()
  void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
  URGE_BINDING()
  void End() { encoder_.End(); }

 private:
  wgpu::RenderPassEncoder encoder_;
};

URGE_BINDING()
class GPUComputePassEncoder : public GPUObject {
 public:
  GPUComputePassEncoder(wgpu::ComputePassEncoder encoder) : encoder_(encoder) {}

  URGE_BINDING()
  void SetPipeline(RefPtr<GPUComputePipeline> pipeline);
  URGE_BINDING()
  void SetBindGroup(uint32_t index, RefPtr<GPUBindGroup> group);
  URGE_BINDING()
  void DispatchWorkgroups(uint32_t x, uint32_t y = 1, uint32_t z = 1);
  URGE_BINDING()
  void End() { encoder_.End(); }

 private:
  wgpu::ComputePassEncoder encoder_;
};

URGE_BINDING()
class GPUQueue : public GPUObject {
 public:
  GPUQueue(wgpu::Queue queue) : queue_(queue) {}

  URGE_BINDING()
  void Submit(RefPtr<GPUCommandBuffer> command_buffer);
  URGE_BINDING()
  void WriteBuffer(RefPtr<GPUBuffer> buffer, uint64_t offset, std::string data);

  wgpu::Queue handle() const { return queue_; }

 private:
  wgpu::Queue queue_;
};

URGE_BINDING()
class GPU : public Singleton<GPU> {
 public:
  URGE_BINDING()
  enum MapMode {
    MapRead = 1,
    MapWrite = 2,
  };

  URGE_BINDING()
  enum BufferUsage {
    CopySrc = 4,
    CopyDst = 8,
    Index = 16,
    Vertex = 32,
    Uniform = 64,
    Storage = 128,
    Indirect = 256,
    QueryResolve = 512,
  };

  URGE_BINDING()
  enum TextureUsage {
    TextureBinding = 4,
    TextureStorageBinding = 8,
    TextureRenderAttachment = 16,
  };

  URGE_BINDING()
  enum LoadOp {
    Load = 1,
    Clear = 2,
  };

  URGE_BINDING()
  enum StoreOp {
    Store = 1,
    Discard = 2,
  };

  URGE_BINDING()
  enum ShaderLanguage {
    ShaderLanguageWgsl = 0,
    ShaderLanguageGlsl = 1,
  };

  URGE_BINDING()
  enum ShaderStage {
    ShaderStageVertex = static_cast<uint32_t>(wgpu::ShaderStage::Vertex),
    ShaderStageFragment = static_cast<uint32_t>(wgpu::ShaderStage::Fragment),
  };

  URGE_BINDING()
  enum IndexFormat {
    IndexFormatUint16 = static_cast<uint32_t>(wgpu::IndexFormat::Uint16),
    IndexFormatUint32 = static_cast<uint32_t>(wgpu::IndexFormat::Uint32),
  };

  URGE_BINDING()
  enum VertexFormat {
    VertexFormatFloat32 = static_cast<uint32_t>(wgpu::VertexFormat::Float32),
    VertexFormatFloat32x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float32x2),
    VertexFormatFloat32x3 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float32x3),
    VertexFormatFloat32x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float32x4),
  };

  URGE_BINDING()
  enum TextureFormat {
    TextureFormatUndefined =
        static_cast<uint32_t>(wgpu::TextureFormat::Undefined),
    TextureFormatRgba8Unorm =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA8Unorm),
  };

  URGE_BINDING()
  enum FilterMode {
    FilterModeNearest = static_cast<uint32_t>(wgpu::FilterMode::Nearest),
    FilterModeLinear = static_cast<uint32_t>(wgpu::FilterMode::Linear),
  };

  URGE_BINDING()
  enum AddressMode {
    AddressModeClampToEdge =
        static_cast<uint32_t>(wgpu::AddressMode::ClampToEdge),
    AddressModeRepeat = static_cast<uint32_t>(wgpu::AddressMode::Repeat),
    AddressModeMirrorRepeat =
        static_cast<uint32_t>(wgpu::AddressMode::MirrorRepeat),
  };

  GPU() = default;

  URGE_BINDING()
  RefPtr<GPUQueue> queue();

  URGE_BINDING()
  RefPtr<GPUBuffer> CreateBuffer(RefPtr<GPUBufferDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUTexture> CreateTexture(RefPtr<GPUTextureDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUSampler> CreateSampler(RefPtr<GPUSamplerDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUShaderModule> CreateShaderModule(
      RefPtr<GPUShaderModuleDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUBindGroupLayout> CreateBindGroupLayout(
      RefPtr<GPUBindGroupLayoutDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUPipelineLayout> CreatePipelineLayout(
      RefPtr<GPUPipelineLayoutDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUBindGroup> CreateBindGroup(
      RefPtr<GPUBindGroupDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPURenderPipeline> CreateRenderPipeline(
      RefPtr<GPURenderPipelineDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUComputePipeline> CreateComputePipeline(
      RefPtr<GPUComputePipelineDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUQuerySet> CreateQuerySet(RefPtr<GPUQuerySetDescriptor> descriptor);
  URGE_BINDING()
  RefPtr<GPUCommandEncoder> CreateCommandEncoder();
};

}  // namespace urge

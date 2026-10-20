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
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/refptr.h"
#include "core/render/device.h"

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

  URGE_BINDING(Name : "entry_point")
  std::string GetEntryPoint() { return entry_point; }
  URGE_BINDING(Name : "entry_point=")
  void SetEntryPoint(std::string value) { entry_point = value; }

 public:
  std::string label;
  std::string code;
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
  void AddTexture(uint32_t binding,
                  uint32_t visibility,
                  uint32_t sample_type = 0,
                  uint32_t view_dimension = 0);
  URGE_BINDING()
  void AddSampler(uint32_t binding,
                  uint32_t visibility,
                  uint32_t sampler_type = 0);

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
  void AddBufferRange(uint32_t binding,
                      RefPtr<GPUBuffer> buffer,
                      uint64_t offset,
                      uint64_t size);
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
  struct VertexBufferLayout {
    uint64_t stride = 0;
    uint32_t step_mode = 1;
    std::vector<wgpu::VertexAttribute> attributes;
  };

  struct ColorTarget {
    uint32_t format = 0;
    uint32_t write_mask = 0xF;
    std::optional<wgpu::BlendState> blend;
  };

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
  void AddVertexAttribute(uint32_t slot,
                          uint32_t location,
                          uint32_t format,
                          uint64_t offset);

  URGE_BINDING()
  void SetVertexBuffer(uint32_t slot, uint64_t stride, uint32_t step_mode = 1);

  URGE_BINDING()
  void AddColorTarget(uint32_t format, uint32_t write_mask = 0xF);

  URGE_BINDING()
  void SetColorBlend(uint32_t index,
                     uint32_t color_op,
                     uint32_t src_factor,
                     uint32_t dst_factor,
                     uint32_t alpha_op = 0,
                     uint32_t alpha_src = 0,
                     uint32_t alpha_dst = 0);

  URGE_BINDING()
  void SetDepthStencil(uint32_t format,
                       bool depth_write,
                       uint32_t depth_compare,
                       int32_t depth_bias = 0,
                       float bias_slope_scale = 0.f,
                       float bias_clamp = 0.f);

  URGE_BINDING()
  void SetStencilFront(uint32_t compare,
                       uint32_t fail_op,
                       uint32_t depth_fail_op,
                       uint32_t pass_op);

  URGE_BINDING()
  void SetStencilBack(uint32_t compare,
                      uint32_t fail_op,
                      uint32_t depth_fail_op,
                      uint32_t pass_op);

  URGE_BINDING()
  void SetStencilMasks(uint32_t read_mask, uint32_t write_mask);

  URGE_BINDING()
  void SetPrimitive(uint32_t topology,
                    uint32_t cull_mode = 0,
                    uint32_t front_face = 0,
                    uint32_t strip_index_format = 0);

  URGE_BINDING()
  void SetMultisample(uint32_t count,
                      uint32_t mask = 0xFFFFFFFF,
                      bool alpha_to_coverage = false);

 public:
  std::string label;
  RefPtr<GPUPipelineLayout> layout;
  RefPtr<GPUShaderModule> vertex_module;
  std::string vertex_entry_point = "main";
  RefPtr<GPUShaderModule> fragment_module;
  std::string fragment_entry_point = "main";
  uint32_t topology =
      static_cast<uint32_t>(wgpu::PrimitiveTopology::TriangleList);
  uint32_t cull_mode = 0;
  uint32_t front_face = 0;
  uint32_t strip_index_format = 0;
  uint32_t sample_count = 1;
  uint32_t sample_mask = 0xFFFFFFFFu;
  bool alpha_to_coverage = false;
  std::vector<VertexBufferLayout> vertex_buffers;
  std::vector<ColorTarget> color_targets;
  std::optional<wgpu::DepthStencilState> depth_stencil;
};

URGE_BINDING()
class GPURenderPassDescriptor : public GPUObject {
 public:
  struct ColorAttachment {
    RefPtr<GPUTextureView> view;
    RefPtr<GPUTextureView> resolve;
    uint32_t load_op = 1;
    uint32_t store_op = 1;
    float clear_r = 0.f;
    float clear_g = 0.f;
    float clear_b = 0.f;
    float clear_a = 1.f;
  };

  URGE_BINDING()
  GPURenderPassDescriptor() = default;

  URGE_BINDING(Name : "label")
  std::string GetLabel() { return label; }
  URGE_BINDING(Name : "label=")
  void SetLabel(std::string value) { label = value; }

  URGE_BINDING()
  void AddColorAttachment(RefPtr<GPUTextureView> view,
                          uint32_t load_op,
                          uint32_t store_op,
                          float clear_r = 0.f,
                          float clear_g = 0.f,
                          float clear_b = 0.f,
                          float clear_a = 1.f);

  URGE_BINDING()
  void SetColorResolve(RefPtr<GPUTextureView> resolve);

  URGE_BINDING()
  void SetDepthStencilAttachment(RefPtr<GPUTextureView> view,
                                 uint32_t depth_load_op,
                                 uint32_t depth_store_op,
                                 float depth_clear_value = 1.f,
                                 uint32_t stencil_load_op = 2,
                                 uint32_t stencil_store_op = 1,
                                 uint32_t stencil_clear_value = 0,
                                 bool depth_read_only = false,
                                 bool stencil_read_only = false);

  URGE_BINDING()
  void SetOcclusionQuerySet(RefPtr<GPUQuerySet> query_set);

 public:
  std::string label;

  std::vector<ColorAttachment> color_attachments;

  RefPtr<GPUTextureView> depth_view;
  uint32_t depth_load_op = 2;
  uint32_t depth_store_op = 1;
  float depth_clear_value = 1.f;
  uint32_t stencil_load_op = 2;
  uint32_t stencil_store_op = 1;
  uint32_t stencil_clear_value = 0;
  bool depth_read_only = false;
  bool stencil_read_only = false;

  RefPtr<GPUQuerySet> occlusion_query_set;
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
  void CopyBufferToTexture(RefPtr<GPUBuffer> source,
                           uint32_t bytes_per_row,
                           uint32_t rows_per_image,
                           RefPtr<GPUTexture> destination,
                           uint32_t destination_mip_level,
                           uint32_t destination_x,
                           uint32_t destination_y,
                           uint32_t width,
                           uint32_t height,
                           uint32_t depth_or_array_layers = 1);
  URGE_BINDING()
  void CopyTextureToBuffer(RefPtr<GPUTexture> source,
                           uint32_t source_mip_level,
                           uint32_t source_x,
                           uint32_t source_y,
                           RefPtr<GPUBuffer> destination,
                           uint32_t bytes_per_row,
                           uint32_t rows_per_image,
                           uint32_t width,
                           uint32_t height,
                           uint32_t depth_or_array_layers = 1);
  URGE_BINDING()
  void CopyTextureToTexture(RefPtr<GPUTexture> source,
                            uint32_t source_mip_level,
                            uint32_t source_x,
                            uint32_t source_y,
                            RefPtr<GPUTexture> destination,
                            uint32_t destination_mip_level,
                            uint32_t destination_x,
                            uint32_t destination_y,
                            uint32_t width,
                            uint32_t height,
                            uint32_t depth_or_array_layers = 1);
  URGE_BINDING()
  void ResolveQuerySet(RefPtr<GPUQuerySet> query_set,
                       uint32_t first_query,
                       uint32_t query_count,
                       RefPtr<GPUBuffer> destination,
                       uint64_t destination_offset);
  URGE_BINDING()
  void ClearBuffer(RefPtr<GPUBuffer> buffer, uint64_t offset, uint64_t size);
  URGE_BINDING()
  void InsertDebugMarker(std::string marker_label);
  URGE_BINDING()
  void PushDebugGroup(std::string group_label);
  URGE_BINDING()
  void PopDebugGroup();
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
  void DrawIndirect(RefPtr<GPUBuffer> indirect_buffer,
                    uint64_t indirect_offset = 0);
  URGE_BINDING()
  void DrawIndexedIndirect(RefPtr<GPUBuffer> indirect_buffer,
                           uint64_t indirect_offset = 0);
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
  void SetStencilReference(uint32_t reference);
  URGE_BINDING()
  void SetBlendConstant(float r, float g, float b, float a);
  URGE_BINDING()
  void InsertDebugMarker(std::string marker_label);
  URGE_BINDING()
  void PushDebugGroup(std::string group_label);
  URGE_BINDING()
  void PopDebugGroup();
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
  void InsertDebugMarker(std::string marker_label);
  URGE_BINDING()
  void PushDebugGroup(std::string group_label);
  URGE_BINDING()
  void PopDebugGroup();
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
  void WriteBuffer(RefPtr<GPUBuffer> buffer,
                   uint64_t offset,
                   std::string data);
  URGE_BINDING()
  void WriteTexture(RefPtr<GPUTexture> texture,
                    uint32_t mip_level,
                    uint32_t x,
                    uint32_t y,
                    uint32_t width,
                    uint32_t height,
                    std::string data,
                    uint32_t bytes_per_row = 0,
                    uint32_t rows_per_image = 1);

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
    TextureCopySrc = 32,
    TextureCopyDst = 64,
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
  enum IndexFormat {
    IndexFormatUndefined = static_cast<uint32_t>(wgpu::IndexFormat::Undefined),
    IndexFormatUint16 = static_cast<uint32_t>(wgpu::IndexFormat::Uint16),
    IndexFormatUint32 = static_cast<uint32_t>(wgpu::IndexFormat::Uint32),
  };

  URGE_BINDING()
  enum VertexFormat {
    VertexFormatUint8x2 = static_cast<uint32_t>(wgpu::VertexFormat::Uint8x2),
    VertexFormatUint8x4 = static_cast<uint32_t>(wgpu::VertexFormat::Uint8x4),
    VertexFormatSint8x2 = static_cast<uint32_t>(wgpu::VertexFormat::Sint8x2),
    VertexFormatSint8x4 = static_cast<uint32_t>(wgpu::VertexFormat::Sint8x4),
    VertexFormatUnorm8x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Unorm8x2),
    VertexFormatUnorm8x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Unorm8x4),
    VertexFormatSnorm8x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Snorm8x2),
    VertexFormatSnorm8x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Snorm8x4),
    VertexFormatUint16x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Uint16x2),
    VertexFormatUint16x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Uint16x4),
    VertexFormatSint16x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Sint16x2),
    VertexFormatSint16x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Sint16x4),
    VertexFormatUnorm16x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Unorm16x2),
    VertexFormatUnorm16x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Unorm16x4),
    VertexFormatSnorm16x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Snorm16x2),
    VertexFormatSnorm16x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Snorm16x4),
    VertexFormatFloat16x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float16x2),
    VertexFormatFloat16x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float16x4),
    VertexFormatFloat32 = static_cast<uint32_t>(wgpu::VertexFormat::Float32),
    VertexFormatFloat32x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float32x2),
    VertexFormatFloat32x3 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float32x3),
    VertexFormatFloat32x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Float32x4),
    VertexFormatUint32 = static_cast<uint32_t>(wgpu::VertexFormat::Uint32),
    VertexFormatUint32x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Uint32x2),
    VertexFormatUint32x3 =
        static_cast<uint32_t>(wgpu::VertexFormat::Uint32x3),
    VertexFormatUint32x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Uint32x4),
    VertexFormatSint32 = static_cast<uint32_t>(wgpu::VertexFormat::Sint32),
    VertexFormatSint32x2 =
        static_cast<uint32_t>(wgpu::VertexFormat::Sint32x2),
    VertexFormatSint32x3 =
        static_cast<uint32_t>(wgpu::VertexFormat::Sint32x3),
    VertexFormatSint32x4 =
        static_cast<uint32_t>(wgpu::VertexFormat::Sint32x4),
  };

  URGE_BINDING()
  enum VertexStepMode {
    VertexStepModeVertex =
        static_cast<uint32_t>(wgpu::VertexStepMode::Vertex),
    VertexStepModeInstance =
        static_cast<uint32_t>(wgpu::VertexStepMode::Instance),
  };

  URGE_BINDING()
  enum TextureFormat {
    TextureFormatUndefined =
        static_cast<uint32_t>(wgpu::TextureFormat::Undefined),
    TextureFormatR8Unorm = static_cast<uint32_t>(wgpu::TextureFormat::R8Unorm),
    TextureFormatR8Snorm = static_cast<uint32_t>(wgpu::TextureFormat::R8Snorm),
    TextureFormatR8Uint = static_cast<uint32_t>(wgpu::TextureFormat::R8Uint),
    TextureFormatR8Sint = static_cast<uint32_t>(wgpu::TextureFormat::R8Sint),
    TextureFormatR16Uint = static_cast<uint32_t>(wgpu::TextureFormat::R16Uint),
    TextureFormatR16Sint = static_cast<uint32_t>(wgpu::TextureFormat::R16Sint),
    TextureFormatR16Float =
        static_cast<uint32_t>(wgpu::TextureFormat::R16Float),
    TextureFormatRg8Unorm =
        static_cast<uint32_t>(wgpu::TextureFormat::RG8Unorm),
    TextureFormatRg8Uint = static_cast<uint32_t>(wgpu::TextureFormat::RG8Uint),
    TextureFormatRg8Sint = static_cast<uint32_t>(wgpu::TextureFormat::RG8Sint),
    TextureFormatR32Float =
        static_cast<uint32_t>(wgpu::TextureFormat::R32Float),
    TextureFormatR32Uint = static_cast<uint32_t>(wgpu::TextureFormat::R32Uint),
    TextureFormatR32Sint = static_cast<uint32_t>(wgpu::TextureFormat::R32Sint),
    TextureFormatRg16Uint =
        static_cast<uint32_t>(wgpu::TextureFormat::RG16Uint),
    TextureFormatRg16Sint =
        static_cast<uint32_t>(wgpu::TextureFormat::RG16Sint),
    TextureFormatRg16Float =
        static_cast<uint32_t>(wgpu::TextureFormat::RG16Float),
    TextureFormatRgba8Unorm =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA8Unorm),
    TextureFormatRgba8UnormSrgb =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA8UnormSrgb),
    TextureFormatRgba8Snorm =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA8Snorm),
    TextureFormatRgba8Uint =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA8Uint),
    TextureFormatRgba8Sint =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA8Sint),
    TextureFormatBgra8Unorm =
        static_cast<uint32_t>(wgpu::TextureFormat::BGRA8Unorm),
    TextureFormatBgra8UnormSrgb =
        static_cast<uint32_t>(wgpu::TextureFormat::BGRA8UnormSrgb),
    TextureFormatRgb10a2Unorm =
        static_cast<uint32_t>(wgpu::TextureFormat::RGB10A2Unorm),
    TextureFormatRg11b10Ufloat =
        static_cast<uint32_t>(wgpu::TextureFormat::RG11B10Ufloat),
    TextureFormatRg32Float =
        static_cast<uint32_t>(wgpu::TextureFormat::RG32Float),
    TextureFormatRg32Uint =
        static_cast<uint32_t>(wgpu::TextureFormat::RG32Uint),
    TextureFormatRg32Sint =
        static_cast<uint32_t>(wgpu::TextureFormat::RG32Sint),
    TextureFormatRgba16Uint =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA16Uint),
    TextureFormatRgba16Sint =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA16Sint),
    TextureFormatRgba16Float =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA16Float),
    TextureFormatRgba32Float =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA32Float),
    TextureFormatRgba32Uint =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA32Uint),
    TextureFormatRgba32Sint =
        static_cast<uint32_t>(wgpu::TextureFormat::RGBA32Sint),
    TextureFormatStencil8 =
        static_cast<uint32_t>(wgpu::TextureFormat::Stencil8),
    TextureFormatDepth16Unorm =
        static_cast<uint32_t>(wgpu::TextureFormat::Depth16Unorm),
    TextureFormatDepth24Plus =
        static_cast<uint32_t>(wgpu::TextureFormat::Depth24Plus),
    TextureFormatDepth24PlusStencil8 =
        static_cast<uint32_t>(wgpu::TextureFormat::Depth24PlusStencil8),
    TextureFormatDepth32Float =
        static_cast<uint32_t>(wgpu::TextureFormat::Depth32Float),
    TextureFormatDepth32FloatStencil8 =
        static_cast<uint32_t>(wgpu::TextureFormat::Depth32FloatStencil8),
  };

  URGE_BINDING()
  enum TextureDimension {
    TextureDimension1D =
        static_cast<uint32_t>(wgpu::TextureDimension::e1D),
    TextureDimension2D =
        static_cast<uint32_t>(wgpu::TextureDimension::e2D),
    TextureDimension3D =
        static_cast<uint32_t>(wgpu::TextureDimension::e3D),
  };

  URGE_BINDING()
  enum TextureAspect {
    TextureAspectAll = static_cast<uint32_t>(wgpu::TextureAspect::All),
    TextureAspectStencilOnly =
        static_cast<uint32_t>(wgpu::TextureAspect::StencilOnly),
    TextureAspectDepthOnly =
        static_cast<uint32_t>(wgpu::TextureAspect::DepthOnly),
  };

  URGE_BINDING()
  enum TextureViewDimension {
    TextureViewDimension1D =
        static_cast<uint32_t>(wgpu::TextureViewDimension::e1D),
    TextureViewDimension2D =
        static_cast<uint32_t>(wgpu::TextureViewDimension::e2D),
    TextureViewDimension2DArray =
        static_cast<uint32_t>(wgpu::TextureViewDimension::e2DArray),
    TextureViewDimensionCube =
        static_cast<uint32_t>(wgpu::TextureViewDimension::Cube),
    TextureViewDimensionCubeArray =
        static_cast<uint32_t>(wgpu::TextureViewDimension::CubeArray),
    TextureViewDimension3D =
        static_cast<uint32_t>(wgpu::TextureViewDimension::e3D),
  };

  URGE_BINDING()
  enum TextureSampleType {
    TextureSampleTypeFloat =
        static_cast<uint32_t>(wgpu::TextureSampleType::Float),
    TextureSampleTypeUnfilterableFloat =
        static_cast<uint32_t>(wgpu::TextureSampleType::UnfilterableFloat),
    TextureSampleTypeDepth =
        static_cast<uint32_t>(wgpu::TextureSampleType::Depth),
    TextureSampleTypeSint =
        static_cast<uint32_t>(wgpu::TextureSampleType::Sint),
    TextureSampleTypeUint =
        static_cast<uint32_t>(wgpu::TextureSampleType::Uint),
  };

  URGE_BINDING()
  enum SamplerBindingType {
    SamplerBindingTypeFiltering =
        static_cast<uint32_t>(wgpu::SamplerBindingType::Filtering),
    SamplerBindingTypeNonFiltering =
        static_cast<uint32_t>(wgpu::SamplerBindingType::NonFiltering),
    SamplerBindingTypeComparison =
        static_cast<uint32_t>(wgpu::SamplerBindingType::Comparison),
  };

  URGE_BINDING()
  enum ShaderStage {
    ShaderStageNone = static_cast<uint32_t>(wgpu::ShaderStage::None),
    ShaderStageVertex = static_cast<uint32_t>(wgpu::ShaderStage::Vertex),
    ShaderStageFragment = static_cast<uint32_t>(wgpu::ShaderStage::Fragment),
    ShaderStageCompute = static_cast<uint32_t>(wgpu::ShaderStage::Compute),
  };

  URGE_BINDING()
  enum PrimitiveTopology {
    PrimitiveTopologyPointList =
        static_cast<uint32_t>(wgpu::PrimitiveTopology::PointList),
    PrimitiveTopologyLineList =
        static_cast<uint32_t>(wgpu::PrimitiveTopology::LineList),
    PrimitiveTopologyLineStrip =
        static_cast<uint32_t>(wgpu::PrimitiveTopology::LineStrip),
    PrimitiveTopologyTriangleList =
        static_cast<uint32_t>(wgpu::PrimitiveTopology::TriangleList),
    PrimitiveTopologyTriangleStrip =
        static_cast<uint32_t>(wgpu::PrimitiveTopology::TriangleStrip),
  };

  URGE_BINDING()
  enum CompareFunction {
    CompareFunctionNever =
        static_cast<uint32_t>(wgpu::CompareFunction::Never),
    CompareFunctionLess =
        static_cast<uint32_t>(wgpu::CompareFunction::Less),
    CompareFunctionEqual =
        static_cast<uint32_t>(wgpu::CompareFunction::Equal),
    CompareFunctionLessEqual =
        static_cast<uint32_t>(wgpu::CompareFunction::LessEqual),
    CompareFunctionGreater =
        static_cast<uint32_t>(wgpu::CompareFunction::Greater),
    CompareFunctionNotEqual =
        static_cast<uint32_t>(wgpu::CompareFunction::NotEqual),
    CompareFunctionGreaterEqual =
        static_cast<uint32_t>(wgpu::CompareFunction::GreaterEqual),
    CompareFunctionAlways =
        static_cast<uint32_t>(wgpu::CompareFunction::Always),
  };

  URGE_BINDING()
  enum StencilOperation {
    StencilOperationKeep =
        static_cast<uint32_t>(wgpu::StencilOperation::Keep),
    StencilOperationZero =
        static_cast<uint32_t>(wgpu::StencilOperation::Zero),
    StencilOperationReplace =
        static_cast<uint32_t>(wgpu::StencilOperation::Replace),
    StencilOperationInvert =
        static_cast<uint32_t>(wgpu::StencilOperation::Invert),
    StencilOperationIncrementClamp =
        static_cast<uint32_t>(wgpu::StencilOperation::IncrementClamp),
    StencilOperationDecrementClamp =
        static_cast<uint32_t>(wgpu::StencilOperation::DecrementClamp),
    StencilOperationIncrementWrap =
        static_cast<uint32_t>(wgpu::StencilOperation::IncrementWrap),
    StencilOperationDecrementWrap =
        static_cast<uint32_t>(wgpu::StencilOperation::DecrementWrap),
  };

  URGE_BINDING()
  enum CullMode {
    CullModeNone = static_cast<uint32_t>(wgpu::CullMode::None),
    CullModeFront = static_cast<uint32_t>(wgpu::CullMode::Front),
    CullModeBack = static_cast<uint32_t>(wgpu::CullMode::Back),
  };

  URGE_BINDING()
  enum FrontFace {
    FrontFaceCCW = static_cast<uint32_t>(wgpu::FrontFace::CCW),
    FrontFaceCW = static_cast<uint32_t>(wgpu::FrontFace::CW),
  };

  URGE_BINDING()
  enum BlendOperation {
    BlendOperationAdd = static_cast<uint32_t>(wgpu::BlendOperation::Add),
    BlendOperationSubtract =
        static_cast<uint32_t>(wgpu::BlendOperation::Subtract),
    BlendOperationReverseSubtract =
        static_cast<uint32_t>(wgpu::BlendOperation::ReverseSubtract),
    BlendOperationMin = static_cast<uint32_t>(wgpu::BlendOperation::Min),
    BlendOperationMax = static_cast<uint32_t>(wgpu::BlendOperation::Max),
  };

  URGE_BINDING()
  enum BlendFactor {
    BlendFactorZero = static_cast<uint32_t>(wgpu::BlendFactor::Zero),
    BlendFactorOne = static_cast<uint32_t>(wgpu::BlendFactor::One),
    BlendFactorSrc = static_cast<uint32_t>(wgpu::BlendFactor::Src),
    BlendFactorOneMinusSrc =
        static_cast<uint32_t>(wgpu::BlendFactor::OneMinusSrc),
    BlendFactorSrcAlpha = static_cast<uint32_t>(wgpu::BlendFactor::SrcAlpha),
    BlendFactorOneMinusSrcAlpha =
        static_cast<uint32_t>(wgpu::BlendFactor::OneMinusSrcAlpha),
    BlendFactorDst = static_cast<uint32_t>(wgpu::BlendFactor::Dst),
    BlendFactorOneMinusDst =
        static_cast<uint32_t>(wgpu::BlendFactor::OneMinusDst),
    BlendFactorDstAlpha = static_cast<uint32_t>(wgpu::BlendFactor::DstAlpha),
    BlendFactorOneMinusDstAlpha =
        static_cast<uint32_t>(wgpu::BlendFactor::OneMinusDstAlpha),
    BlendFactorSrcAlphaSaturated =
        static_cast<uint32_t>(wgpu::BlendFactor::SrcAlphaSaturated),
    BlendFactorConstant = static_cast<uint32_t>(wgpu::BlendFactor::Constant),
    BlendFactorOneMinusConstant =
        static_cast<uint32_t>(wgpu::BlendFactor::OneMinusConstant),
  };

  URGE_BINDING()
  enum ColorWriteMask {
    ColorWriteMaskNone = static_cast<uint32_t>(wgpu::ColorWriteMask::None),
    ColorWriteMaskRed = static_cast<uint32_t>(wgpu::ColorWriteMask::Red),
    ColorWriteMaskGreen = static_cast<uint32_t>(wgpu::ColorWriteMask::Green),
    ColorWriteMaskBlue = static_cast<uint32_t>(wgpu::ColorWriteMask::Blue),
    ColorWriteMaskAlpha = static_cast<uint32_t>(wgpu::ColorWriteMask::Alpha),
    ColorWriteMaskAll = static_cast<uint32_t>(wgpu::ColorWriteMask::All),
  };

  URGE_BINDING()
  enum FilterMode {
    FilterModeNearest = static_cast<uint32_t>(wgpu::FilterMode::Nearest),
    FilterModeLinear = static_cast<uint32_t>(wgpu::FilterMode::Linear),
  };

  URGE_BINDING()
  enum MipmapFilterMode {
    MipmapFilterModeNearest =
        static_cast<uint32_t>(wgpu::MipmapFilterMode::Nearest),
    MipmapFilterModeLinear =
        static_cast<uint32_t>(wgpu::MipmapFilterMode::Linear),
  };

  URGE_BINDING()
  enum AddressMode {
    AddressModeClampToEdge =
        static_cast<uint32_t>(wgpu::AddressMode::ClampToEdge),
    AddressModeRepeat = static_cast<uint32_t>(wgpu::AddressMode::Repeat),
    AddressModeMirrorRepeat =
        static_cast<uint32_t>(wgpu::AddressMode::MirrorRepeat),
  };

  URGE_BINDING()
  enum QueryType {
    QueryTypeOcclusion = static_cast<uint32_t>(wgpu::QueryType::Occlusion),
    QueryTypeTimestamp = static_cast<uint32_t>(wgpu::QueryType::Timestamp),
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

}

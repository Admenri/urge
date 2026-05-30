// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/bind/callback.h"
#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"
#include "content/gpu/gpu.h"

namespace content {

///
/// GPU Buffer
///

URGE_BINDING()
class GPUBuffer : public Object {
 public:
  GPUBuffer(wgpu::Buffer object);

  GPUBuffer(const GPUBuffer&) = delete;
  GPUBuffer& operator=(const GPUBuffer&) = delete;

  wgpu::Buffer handle() const { return object_; }

 public:
  URGE_BINDING()
  using MapAsyncCallback =
      base::RepeatingCallback<void(GPU::MapAsyncStatus status,
                                   estring message)>;

  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  void Destroy(URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t GetSize(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::BufferUsage GetUsage(URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t MapAsync(GPU::MapMode mode,
                    size_t offset,
                    size_t size,
                    MapAsyncCallback callback,
                    URGE_EXCEPTION);

  URGE_BINDING()
  void Unmap(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::BufferMapState GetMapState(URGE_EXCEPTION);

  URGE_BINDING()
  epointer GetMappedRange(size_t offset,
                          size_t size,
                          bool is_mutable,
                          URGE_EXCEPTION);

 private:
  wgpu::Buffer object_;
};

///
/// GPU Texture View
///

URGE_BINDING()
class GPUTextureView : public Object {
 public:
  GPUTextureView(wgpu::TextureView object);

  GPUTextureView(const GPUTextureView&) = delete;
  GPUTextureView& operator=(const GPUTextureView&) = delete;

  wgpu::TextureView handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

 private:
  wgpu::TextureView object_;
};

///
/// GPU Texture
///

URGE_BINDING()
struct GPUTextureViewDescriptor {
  estring label = {};
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;
  GPU::TextureViewDimension dimension = GPU::TextureViewDimension::Undefined;
  uint32_t baseMipLevel = 0;
  uint32_t mipLevelCount = WGPU_MIP_LEVEL_COUNT_UNDEFINED;
  uint32_t baseArrayLayer = 0;
  uint32_t arrayLayerCount = WGPU_ARRAY_LAYER_COUNT_UNDEFINED;
  GPU::TextureAspect aspect = GPU::TextureAspect::Undefined;
  GPU::TextureUsage usage = GPU::TextureUsage::None;
};

URGE_BINDING()
class GPUTexture : public Object {
 public:
  GPUTexture(wgpu::Texture object);

  GPUTexture(const GPUTexture&) = delete;
  GPUTexture& operator=(const GPUTexture&) = delete;

  wgpu::Texture handle() const { return object_; }

 public:
  URGE_BINDING()
  scoped_refptr<GPUTextureView> CreateView(
      estruct<GPUTextureViewDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  void Destroy(URGE_EXCEPTION);

  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetDepthOrArrayLayers(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::TextureDimension GetDimension(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::TextureFormat GetFormat(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetMipLevelCount(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetSampleCount(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::TextureViewDimension GetTextureBindingViewDimension(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::TextureUsage GetUsage(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetWidth(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetHeight(URGE_EXCEPTION);

 private:
  wgpu::Texture object_;
};

///
/// GPU Sampler
///

URGE_BINDING()
class GPUSampler : public Object {
 public:
  GPUSampler(wgpu::Sampler object);

  GPUSampler(const GPUSampler&) = delete;
  GPUSampler& operator=(const GPUSampler&) = delete;

  wgpu::Sampler handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

 private:
  wgpu::Sampler object_;
};

///
/// GPU ShaderModule
///

URGE_BINDING()
class GPUShaderModule : public Object {
 public:
  GPUShaderModule(wgpu::ShaderModule object);

  GPUShaderModule(const GPUShaderModule&) = delete;
  GPUShaderModule& operator=(const GPUShaderModule&) = delete;

  wgpu::ShaderModule handle() const { return object_; }

 public:
  URGE_BINDING()
  using CompilationInfoCallback =
      base::RepeatingCallback<void(GPU::CompilationInfoRequestStatus status,
                                   earray<estring> messages)>;

  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t GetCompilationInfo(CompilationInfoCallback callback, URGE_EXCEPTION);

 private:
  wgpu::ShaderModule object_;
};

///
/// GPU BindGroupLayout
///

URGE_BINDING()
class GPUBindGroupLayout : public Object {
 public:
  GPUBindGroupLayout(wgpu::BindGroupLayout object);

  GPUBindGroupLayout(const GPUBindGroupLayout&) = delete;
  GPUBindGroupLayout& operator=(const GPUBindGroupLayout&) = delete;

  wgpu::BindGroupLayout handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

 private:
  wgpu::BindGroupLayout object_;
};

///
/// GPU PipelineLayout
///

URGE_BINDING()
class GPUPipelineLayout : public Object {
 public:
  GPUPipelineLayout(wgpu::PipelineLayout object);

  GPUPipelineLayout(const GPUPipelineLayout&) = delete;
  GPUPipelineLayout& operator=(const GPUPipelineLayout&) = delete;

  wgpu::PipelineLayout handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

 private:
  wgpu::PipelineLayout object_;
};

///
/// GPU RenderPipeline
///

URGE_BINDING()
class GPURenderPipeline : public Object {
 public:
  GPURenderPipeline(wgpu::RenderPipeline object);

  GPURenderPipeline(const GPURenderPipeline&) = delete;
  GPURenderPipeline& operator=(const GPURenderPipeline&) = delete;

  wgpu::RenderPipeline handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBindGroupLayout> GetBindGroupLayout(uint32_t group_index,
                                                       URGE_EXCEPTION);

 private:
  wgpu::RenderPipeline object_;
};

///
/// GPU ComputePipeline
///

URGE_BINDING()
class GPUComputePipeline : public Object {
 public:
  GPUComputePipeline(wgpu::ComputePipeline object);

  GPUComputePipeline(const GPUComputePipeline&) = delete;
  GPUComputePipeline& operator=(const GPUComputePipeline&) = delete;

  wgpu::ComputePipeline handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBindGroupLayout> GetBindGroupLayout(uint32_t group_index,
                                                       URGE_EXCEPTION);

 private:
  wgpu::ComputePipeline object_;
};

///
/// GPU QuerySet
///

URGE_BINDING()
class GPUQuerySet : public Object {
 public:
  GPUQuerySet(wgpu::QuerySet object);

  GPUQuerySet(const GPUQuerySet&) = delete;
  GPUQuerySet& operator=(const GPUQuerySet&) = delete;

  wgpu::QuerySet handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  void Destroy(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetCount(URGE_EXCEPTION);

  URGE_BINDING()
  GPU::QueryType GetType(URGE_EXCEPTION);

 private:
  wgpu::QuerySet object_;
};

///
/// GPU BindGroup
///

URGE_BINDING()
class GPUBindGroup : public Object {
 public:
  GPUBindGroup(wgpu::BindGroup object);

  GPUBindGroup(const GPUBindGroup&) = delete;
  GPUBindGroup& operator=(const GPUBindGroup&) = delete;

  wgpu::BindGroup handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

 private:
  wgpu::BindGroup object_;
};

///
/// GPU CommandBuffer
///

URGE_BINDING()
class GPUCommandBuffer : public Object {
 public:
  GPUCommandBuffer(wgpu::CommandBuffer object);

  GPUCommandBuffer(const GPUCommandBuffer&) = delete;
  GPUCommandBuffer& operator=(const GPUCommandBuffer&) = delete;

  wgpu::CommandBuffer handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

 private:
  wgpu::CommandBuffer object_;
};

}  // namespace content

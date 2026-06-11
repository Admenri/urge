// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <vector>

#include "base/bind/callback.h"
#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"
#include "content/gpu/gpu_command.h"

namespace content {

///
/// GPU Queue
///

URGE_BINDING()
class GPUQueue : public Object {
 public:
  GPUQueue(wgpu::Queue object);

  GPUQueue(const GPUQueue&) = delete;
  GPUQueue& operator=(const GPUQueue&) = delete;

  wgpu::Queue handle() const { return object_; }

 public:
  URGE_BINDING()
  using QueueWorkDoneCallback =
      base::RepeatingCallback<void(GPU::QueueWorkDoneStatus status,
                                   estring message)>;

  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t OnSubmittedWorkDone(QueueWorkDoneCallback callback, URGE_EXCEPTION);

  URGE_BINDING()
  void Submit(earray<scoped_refptr<GPUCommandBuffer>> commands, URGE_EXCEPTION);

  URGE_BINDING()
  void WriteBuffer(scoped_refptr<GPUBuffer> buffer,
                   uint64_t buffer_offset,
                   epointer data,
                   size_t size,
                   URGE_EXCEPTION);

  URGE_BINDING()
  void WriteTexture(scoped_refptr<GPUTexelCopyTextureInfo> destination,
                    epointer data,
                    size_t data_size,
                    scoped_refptr<GPUTexelCopyBufferLayout> data_layout,
                    scoped_refptr<GPUExtent3D> write_size,
                    URGE_EXCEPTION);

 private:
  wgpu::Queue object_;
};

///
/// GPU Device
///

URGE_BINDING()
class GPUBindGroupEntry : public Object {
 public:
  URGE_BINDING()
  uint32_t binding = 0;

  URGE_BINDING()
  scoped_refptr<GPUBuffer> buffer = nullptr;

  URGE_BINDING()
  uint64_t offset = 0;

  URGE_BINDING()
  uint64_t size = WGPU_WHOLE_SIZE;

  URGE_BINDING()
  scoped_refptr<GPUSampler> sampler = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUTextureView> textureView = nullptr;
};

URGE_BINDING()
class GPUBindGroupDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  scoped_refptr<GPUBindGroupLayout> layout = nullptr;

  URGE_BINDING()
  earray<scoped_refptr<GPUBindGroupEntry>> entries = {};
};

URGE_BINDING()
class GPUBufferBindingLayout : public Object {
 public:
  URGE_BINDING()
  GPU::BufferBindingType type = GPU::BufferBindingType::Undefined;

  URGE_BINDING()
  bool hasDynamicOffset = false;

  URGE_BINDING()
  uint64_t minBindingSize = 0;
};

URGE_BINDING()
class GPUSamplerBindingLayout : public Object {
 public:
  URGE_BINDING()
  GPU::SamplerBindingType type = GPU::SamplerBindingType::Undefined;
};

URGE_BINDING()
class GPUTextureBindingLayout : public Object {
 public:
  URGE_BINDING()
  GPU::TextureSampleType sampleType = GPU::TextureSampleType::Undefined;

  URGE_BINDING()
  GPU::TextureViewDimension viewDimension =
      GPU::TextureViewDimension::Undefined;

  URGE_BINDING()
  bool multisampled = false;
};

URGE_BINDING()
class GPUStorageTextureBindingLayout : public Object {
 public:
  URGE_BINDING()
  GPU::StorageTextureAccess access = GPU::StorageTextureAccess::Undefined;

  URGE_BINDING()
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;

  URGE_BINDING()
  GPU::TextureViewDimension viewDimension =
      GPU::TextureViewDimension::Undefined;
};

URGE_BINDING()
class GPUBindGroupLayoutEntry : public Object {
 public:
  URGE_BINDING()
  uint32_t binding = 0;

  URGE_BINDING()
  GPU::ShaderStage visibility = GPU::ShaderStage::None;

  URGE_BINDING()
  uint32_t bindingArraySize = 0;

  URGE_BINDING()
  scoped_refptr<GPUBufferBindingLayout> buffer = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUSamplerBindingLayout> sampler = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUTextureBindingLayout> texture = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUStorageTextureBindingLayout> storageTexture = nullptr;
};

URGE_BINDING()
class GPUBindGroupLayoutDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUBindGroupLayoutEntry>> entries = {};
};

URGE_BINDING()
class GPUBufferDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  GPU::BufferUsage usage = GPU::BufferUsage::None;

  URGE_BINDING()
  uint64_t size = 0;

  URGE_BINDING()
  bool mappedAtCreation = false;
};

URGE_BINDING()
class GPUCommandEncoderDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};
};

URGE_BINDING()
class GPUConstantEntry : public Object {
 public:
  URGE_BINDING()
  estring key = {};

  URGE_BINDING()
  double value = 0;
};

URGE_BINDING()
class GPUComputeState : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUShaderModule> module = nullptr;

  URGE_BINDING()
  estring entryPoint = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUConstantEntry>> constants = {};
};

URGE_BINDING()
class GPUComputePipelineDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  scoped_refptr<GPUPipelineLayout> layout = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUComputeState> compute = nullptr;
};

URGE_BINDING()
class GPUPipelineLayoutDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUBindGroupLayout>> bindGroupLayouts = {};

  URGE_BINDING()
  uint32_t immediateSize = 0;
};

URGE_BINDING()
class GPUQuerySetDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  GPU::QueryType type = {};

  URGE_BINDING()
  uint32_t count = 0;
};

URGE_BINDING()
class GPUVertexAttribute : public Object {
 public:
  URGE_BINDING()
  GPU::VertexFormat format = {};

  URGE_BINDING()
  uint64_t offset = 0;

  URGE_BINDING()
  uint32_t shaderLocation = 0;
};

URGE_BINDING()
class GPUVertexBufferLayout : public Object {
 public:
  URGE_BINDING()
  GPU::VertexStepMode stepMode = GPU::VertexStepMode::Undefined;

  URGE_BINDING()
  uint64_t arrayStride = 0;

  URGE_BINDING()
  earray<scoped_refptr<GPUVertexAttribute>> attributes = {};
};

URGE_BINDING()
class GPUBlendComponent : public Object {
 public:
  URGE_BINDING()
  GPU::BlendOperation operation = GPU::BlendOperation::Undefined;

  URGE_BINDING()
  GPU::BlendFactor srcFactor = GPU::BlendFactor::Undefined;

  URGE_BINDING()
  GPU::BlendFactor dstFactor = GPU::BlendFactor::Undefined;
};

URGE_BINDING()
class GPUBlendState : public Object {
 public:
  URGE_BINDING()
  GPUBlendComponent color = {};

  URGE_BINDING()
  GPUBlendComponent alpha = {};
};

URGE_BINDING()
class GPUColorTargetState : public Object {
 public:
  URGE_BINDING()
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;

  URGE_BINDING()
  scoped_refptr<GPUBlendState> blend = nullptr;

  URGE_BINDING()
  GPU::ColorWriteMask writeMask = GPU::ColorWriteMask::All;
};

URGE_BINDING()
class GPUVertexState : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUShaderModule> module = nullptr;

  URGE_BINDING()
  estring entryPoint = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUConstantEntry>> constants = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUVertexBufferLayout>> buffers = {};
};

URGE_BINDING()
class GPUFragmentState : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUShaderModule> module = nullptr;

  URGE_BINDING()
  estring entryPoint = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUConstantEntry>> constants = {};

  URGE_BINDING()
  earray<scoped_refptr<GPUColorTargetState>> targets = {};
};

URGE_BINDING()
class GPUPrimitiveState : public Object {
 public:
  URGE_BINDING()
  GPU::PrimitiveTopology topology = GPU::PrimitiveTopology::Undefined;

  URGE_BINDING()
  GPU::IndexFormat stripIndexFormat = GPU::IndexFormat::Undefined;

  URGE_BINDING()
  GPU::FrontFace frontFace = GPU::FrontFace::Undefined;

  URGE_BINDING()
  GPU::CullMode cullMode = GPU::CullMode::Undefined;

  URGE_BINDING()
  bool unclippedDepth = false;
};

URGE_BINDING()
class GPUStencilFaceState : public Object {
 public:
  URGE_BINDING()
  GPU::CompareFunction compare = GPU::CompareFunction::Undefined;

  URGE_BINDING()
  GPU::StencilOperation failOp = GPU::StencilOperation::Undefined;

  URGE_BINDING()
  GPU::StencilOperation depthFailOp = GPU::StencilOperation::Undefined;

  URGE_BINDING()
  GPU::StencilOperation passOp = GPU::StencilOperation::Undefined;
};

URGE_BINDING()
class GPUDepthStencilState : public Object {
 public:
  URGE_BINDING()
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;

  URGE_BINDING()
  bool depthWriteEnabled = false;

  URGE_BINDING()
  GPU::CompareFunction depthCompare = GPU::CompareFunction::Undefined;

  URGE_BINDING()
  scoped_refptr<GPUStencilFaceState> stencilFront = {};

  URGE_BINDING()
  scoped_refptr<GPUStencilFaceState> stencilBack = {};

  URGE_BINDING()
  uint32_t stencilReadMask = 0xFFFFFFFF;

  URGE_BINDING()
  uint32_t stencilWriteMask = 0xFFFFFFFF;

  URGE_BINDING()
  int32_t depthBias = 0;

  URGE_BINDING()
  float depthBiasSlopeScale = 0.f;

  URGE_BINDING()
  float depthBiasClamp = 0.f;
};

URGE_BINDING()
class GPUMultisampleState : public Object {
 public:
  URGE_BINDING()
  uint32_t count = 1;

  URGE_BINDING()
  uint32_t mask = 0xFFFFFFFF;

  URGE_BINDING()
  bool alphaToCoverageEnabled = false;
};

URGE_BINDING()
class GPURenderPipelineDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  scoped_refptr<GPUPipelineLayout> layout = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUVertexState> vertex = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUPrimitiveState> primitive = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUDepthStencilState> depthStencil = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUMultisampleState> multisample = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUFragmentState> fragment = nullptr;
};

URGE_BINDING()
class GPUSamplerDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  GPU::AddressMode addressModeU = GPU::AddressMode::Undefined;

  URGE_BINDING()
  GPU::AddressMode addressModeV = GPU::AddressMode::Undefined;

  URGE_BINDING()
  GPU::AddressMode addressModeW = GPU::AddressMode::Undefined;

  URGE_BINDING()
  GPU::FilterMode magFilter = GPU::FilterMode::Undefined;

  URGE_BINDING()
  GPU::FilterMode minFilter = GPU::FilterMode::Undefined;

  URGE_BINDING()
  GPU::MipmapFilterMode mipmapFilter = GPU::MipmapFilterMode::Undefined;

  URGE_BINDING()
  float lodMinClamp = 0.f;

  URGE_BINDING()
  float lodMaxClamp = 32.f;

  URGE_BINDING()
  GPU::CompareFunction compare = GPU::CompareFunction::Undefined;

  URGE_BINDING()
  uint16_t maxAnisotropy = 1;
};

URGE_BINDING()
class GPUShaderModuleDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  estring wgslCode = {};
};

URGE_BINDING()
class GPUTextureDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  GPU::TextureUsage usage = GPU::TextureUsage::None;

  URGE_BINDING()
  GPU::TextureDimension dimension = GPU::TextureDimension::Undefined;

  URGE_BINDING()
  scoped_refptr<GPUExtent3D> size = nullptr;

  URGE_BINDING()
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;

  URGE_BINDING()
  uint32_t mipLevelCount = 1;

  URGE_BINDING()
  uint32_t sampleCount = 1;

  URGE_BINDING()
  earray<GPU::TextureFormat> viewFormats = {};
};

URGE_BINDING()
class GPULimits : public Object {
 public:
  URGE_BINDING()
  epointer reservedParam;

  URGE_BINDING()
  uint32_t maxTextureDimension1D;

  URGE_BINDING()
  uint32_t maxTextureDimension2D;

  URGE_BINDING()
  uint32_t maxTextureDimension3D;

  URGE_BINDING()
  uint32_t maxTextureArrayLayers;

  URGE_BINDING()
  uint32_t maxBindGroups;

  URGE_BINDING()
  uint32_t maxBindGroupsPlusVertexBuffers;

  URGE_BINDING()
  uint32_t maxBindingsPerBindGroup;

  URGE_BINDING()
  uint32_t maxDynamicUniformBuffersPerPipelineLayout;

  URGE_BINDING()
  uint32_t maxDynamicStorageBuffersPerPipelineLayout;

  URGE_BINDING()
  uint32_t maxSampledTexturesPerShaderStage;

  URGE_BINDING()
  uint32_t maxSamplersPerShaderStage;

  URGE_BINDING()
  uint32_t maxStorageBuffersPerShaderStage;

  URGE_BINDING()
  uint32_t maxStorageTexturesPerShaderStage;

  URGE_BINDING()
  uint32_t maxUniformBuffersPerShaderStage;

  URGE_BINDING()
  uint64_t maxUniformBufferBindingSize;

  URGE_BINDING()
  uint64_t maxStorageBufferBindingSize;

  URGE_BINDING()
  uint32_t minUniformBufferOffsetAlignment;

  URGE_BINDING()
  uint32_t minStorageBufferOffsetAlignment;

  URGE_BINDING()
  uint32_t maxVertexBuffers;

  URGE_BINDING()
  uint64_t maxBufferSize;

  URGE_BINDING()
  uint32_t maxVertexAttributes;

  URGE_BINDING()
  uint32_t maxVertexBufferArrayStride;

  URGE_BINDING()
  uint32_t maxInterStageShaderVariables;

  URGE_BINDING()
  uint32_t maxColorAttachments;

  URGE_BINDING()
  uint32_t maxColorAttachmentBytesPerSample;

  URGE_BINDING()
  uint32_t maxComputeWorkgroupStorageSize;

  URGE_BINDING()
  uint32_t maxComputeInvocationsPerWorkgroup;

  URGE_BINDING()
  uint32_t maxComputeWorkgroupSizeX;

  URGE_BINDING()
  uint32_t maxComputeWorkgroupSizeY;

  URGE_BINDING()
  uint32_t maxComputeWorkgroupSizeZ;

  URGE_BINDING()
  uint32_t maxComputeWorkgroupsPerDimension;

  URGE_BINDING()
  uint32_t maxImmediateSize;
};

URGE_BINDING()
class GPUDevice : public Object {
 public:
  GPUDevice(wgpu::Device object);

  GPUDevice(const GPUDevice&) = delete;
  GPUDevice& operator=(const GPUDevice&) = delete;

  wgpu::Device handle() const { return object_; }

 public:
  URGE_BINDING()
  using CreateComputePipelineAsyncCallback =
      base::RepeatingCallback<void(GPU::CreatePipelineAsyncStatus status,
                                   scoped_refptr<GPUComputePipeline> pipeline,
                                   estring message)>;

  URGE_BINDING()
  using CreateRenderPipelineAsyncCallback =
      base::RepeatingCallback<void(GPU::CreatePipelineAsyncStatus status,
                                   scoped_refptr<GPURenderPipeline> pipeline,
                                   estring message)>;

  URGE_BINDING()
  using PopErrorScopeCallback =
      base::RepeatingCallback<void(GPU::PopErrorScopeStatus status,
                                   GPU::ErrorType type,
                                   estring message)>;

  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBindGroup> CreateBindGroup(
      scoped_refptr<GPUBindGroupDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBindGroupLayout> CreateBindGroupLayout(
      scoped_refptr<GPUBindGroupLayoutDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBuffer> CreateBuffer(
      scoped_refptr<GPUBufferDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUCommandEncoder> CreateCommandEncoder(
      scoped_refptr<GPUCommandEncoderDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUComputePipeline> CreateComputePipeline(
      scoped_refptr<GPUComputePipelineDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t CreateComputePipelineAsync(
      scoped_refptr<GPUComputePipelineDescriptor> descriptor,
      CreateComputePipelineAsyncCallback callback,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUPipelineLayout> CreatePipelineLayout(
      scoped_refptr<GPUPipelineLayoutDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUQuerySet> CreateQuerySet(
      scoped_refptr<GPUQuerySetDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPURenderPipeline> CreateRenderPipeline(
      scoped_refptr<GPURenderPipelineDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t CreateRenderPipelineAsync(
      scoped_refptr<GPURenderPipelineDescriptor> descriptor,
      CreateRenderPipelineAsyncCallback callback,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUSampler> CreateSampler(
      scoped_refptr<GPUSamplerDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUShaderModule> CreateShaderModule(
      scoped_refptr<GPUShaderModuleDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUTexture> CreateTexture(
      scoped_refptr<GPUTextureDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUQueue> GetQueue(URGE_EXCEPTION);

  URGE_BINDING()
  void PushErrorScope(GPU::ErrorFilter filter, URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t PopErrorScope(PopErrorScopeCallback callback, URGE_EXCEPTION);

  URGE_BINDING()
  earray<estring> GetFeatures(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPULimits> GetLimits(URGE_EXCEPTION);

 private:
  wgpu::Device object_;
};

}  // namespace content

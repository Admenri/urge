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
  void WriteTexture(estruct<GPUTexelCopyTextureInfo> destination,
                    epointer data,
                    size_t data_size,
                    estruct<GPUTexelCopyBufferLayout> data_layout,
                    estruct<GPUExtent3D> write_size,
                    URGE_EXCEPTION);

 private:
  wgpu::Queue object_;
};

///
/// GPU Device
///

URGE_BINDING()
struct GPUBindGroupEntry {
  uint32_t binding = 0;
  scoped_refptr<GPUBuffer> buffer = nullptr;
  uint64_t offset = 0;
  uint64_t size = WGPU_WHOLE_SIZE;
  scoped_refptr<GPUSampler> sampler = nullptr;
  scoped_refptr<GPUTextureView> textureView = nullptr;
};

URGE_BINDING()
struct GPUBindGroupDescriptor {
  estring label = {};
  scoped_refptr<GPUBindGroupLayout> layout = nullptr;
  earray<estruct<GPUBindGroupEntry>> entries = {};
};

URGE_BINDING()
struct BufferBindingLayout {
  GPU::BufferBindingType type = GPU::BufferBindingType::Undefined;
  bool hasDynamicOffset = false;
  uint64_t minBindingSize = 0;
};

URGE_BINDING()
struct SamplerBindingLayout {
  GPU::SamplerBindingType type = GPU::SamplerBindingType::Undefined;
};

URGE_BINDING()
struct TextureBindingLayout {
  GPU::TextureSampleType sampleType = GPU::TextureSampleType::Undefined;
  GPU::TextureViewDimension viewDimension =
      GPU::TextureViewDimension::Undefined;
  bool multisampled = false;
};

URGE_BINDING()
struct StorageTextureBindingLayout {
  GPU::StorageTextureAccess access = GPU::StorageTextureAccess::Undefined;
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;
  GPU::TextureViewDimension viewDimension =
      GPU::TextureViewDimension::Undefined;
};

URGE_BINDING()
struct GPUBindGroupLayoutEntry {
  uint32_t binding = 0;
  GPU::ShaderStage visibility = GPU::ShaderStage::None;
  uint32_t bindingArraySize = 0;
  estruct<BufferBindingLayout> buffer = nullptr;
  estruct<SamplerBindingLayout> sampler = nullptr;
  estruct<TextureBindingLayout> texture = nullptr;
  estruct<StorageTextureBindingLayout> storageTexture = nullptr;
};

URGE_BINDING()
struct GPUBindGroupLayoutDescriptor {
  estring label = {};
  earray<estruct<GPUBindGroupLayoutEntry>> entries = {};
};

URGE_BINDING()
struct GPUBufferDescriptor {
  estring label = {};
  GPU::BufferUsage usage = GPU::BufferUsage::None;
  uint64_t size = 0;
  bool mappedAtCreation = false;
};

URGE_BINDING()
struct GPUCommandEncoderDescriptor {
  estring label = {};
};

URGE_BINDING()
struct GPUConstantEntry {
  estring key = {};
  double value = 0;
};

URGE_BINDING()
struct GPUComputeState {
  scoped_refptr<GPUShaderModule> module = nullptr;
  estring entryPoint = {};
  earray<estruct<GPUConstantEntry>> constants = {};
};

URGE_BINDING()
struct GPUComputePipelineDescriptor {
  estring label = {};
  scoped_refptr<GPUPipelineLayout> layout = nullptr;
  estruct<GPUComputeState> compute = nullptr;
};

URGE_BINDING()
struct GPUPipelineLayoutDescriptor {
  estring label = {};
  earray<scoped_refptr<GPUBindGroupLayout>> bindGroupLayouts = {};
  uint32_t immediateSize = 0;
};

URGE_BINDING()
struct GPUQuerySetDescriptor {
  estring label = {};
  GPU::QueryType type = {};
  uint32_t count = 0;
};

URGE_BINDING()
struct GPUVertexAttribute {
  GPU::VertexFormat format = {};
  uint64_t offset = 0;
  uint32_t shaderLocation = 0;
};

URGE_BINDING()
struct GPUVertexBufferLayout {
  GPU::VertexStepMode stepMode = GPU::VertexStepMode::Undefined;
  uint64_t arrayStride = 0;
  earray<estruct<GPUVertexAttribute>> attributes = {};
};

URGE_BINDING()
struct GPUBlendComponent {
  GPU::BlendOperation operation = GPU::BlendOperation::Undefined;
  GPU::BlendFactor srcFactor = GPU::BlendFactor::Undefined;
  GPU::BlendFactor dstFactor = GPU::BlendFactor::Undefined;
};

URGE_BINDING()
struct GPUBlendState {
  GPUBlendComponent color = {};
  GPUBlendComponent alpha = {};
};

URGE_BINDING()
struct GPUColorTargetState {
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;
  estruct<GPUBlendState> blend = nullptr;
  GPU::ColorWriteMask writeMask = GPU::ColorWriteMask::All;
};

URGE_BINDING()
struct GPUVertexState {
  scoped_refptr<GPUShaderModule> module = nullptr;
  estring entryPoint = {};
  earray<estruct<GPUConstantEntry>> constants = {};
  earray<estruct<GPUVertexBufferLayout>> buffers = {};
};

URGE_BINDING()
struct GPUFragmentState {
  scoped_refptr<GPUShaderModule> module = nullptr;
  estring entryPoint = {};
  earray<estruct<GPUConstantEntry>> constants = {};
  earray<estruct<GPUColorTargetState>> targets = {};
};

URGE_BINDING()
struct GPUPrimitiveState {
  GPU::PrimitiveTopology topology = GPU::PrimitiveTopology::Undefined;
  GPU::IndexFormat stripIndexFormat = GPU::IndexFormat::Undefined;
  GPU::FrontFace frontFace = GPU::FrontFace::Undefined;
  GPU::CullMode cullMode = GPU::CullMode::Undefined;
  bool unclippedDepth = false;
};

URGE_BINDING()
struct GPUStencilFaceState {
  GPU::CompareFunction compare = GPU::CompareFunction::Undefined;
  GPU::StencilOperation failOp = GPU::StencilOperation::Undefined;
  GPU::StencilOperation depthFailOp = GPU::StencilOperation::Undefined;
  GPU::StencilOperation passOp = GPU::StencilOperation::Undefined;
};

URGE_BINDING()
struct GPUDepthStencilState {
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;
  bool depthWriteEnabled = false;
  GPU::CompareFunction depthCompare = GPU::CompareFunction::Undefined;
  estruct<GPUStencilFaceState> stencilFront = {};
  estruct<GPUStencilFaceState> stencilBack = {};
  uint32_t stencilReadMask = 0xFFFFFFFF;
  uint32_t stencilWriteMask = 0xFFFFFFFF;
  int32_t depthBias = 0;
  float depthBiasSlopeScale = 0.f;
  float depthBiasClamp = 0.f;
};

URGE_BINDING()
struct GPUMultisampleState {
  uint32_t count = 1;
  uint32_t mask = 0xFFFFFFFF;
  bool alphaToCoverageEnabled = false;
};

URGE_BINDING()
struct GPURenderPipelineDescriptor {
  estring label = {};
  scoped_refptr<GPUPipelineLayout> layout = nullptr;
  estruct<GPUVertexState> vertex = nullptr;
  estruct<GPUPrimitiveState> primitive = nullptr;
  estruct<GPUDepthStencilState> depthStencil = nullptr;
  estruct<GPUMultisampleState> multisample = nullptr;
  estruct<GPUFragmentState> fragment = nullptr;
};

URGE_BINDING()
struct GPUSamplerDescriptor {
  estring label = {};
  GPU::AddressMode addressModeU = GPU::AddressMode::Undefined;
  GPU::AddressMode addressModeV = GPU::AddressMode::Undefined;
  GPU::AddressMode addressModeW = GPU::AddressMode::Undefined;
  GPU::FilterMode magFilter = GPU::FilterMode::Undefined;
  GPU::FilterMode minFilter = GPU::FilterMode::Undefined;
  GPU::MipmapFilterMode mipmapFilter = GPU::MipmapFilterMode::Undefined;
  float lodMinClamp = 0.f;
  float lodMaxClamp = 32.f;
  GPU::CompareFunction compare = GPU::CompareFunction::Undefined;
  uint16_t maxAnisotropy = 1;
};

URGE_BINDING()
struct GPUShaderModuleDescriptor {
  estring label = {};
  estring wgslCode = {};
};

URGE_BINDING()
struct GPUTextureDescriptor {
  estring label = {};
  GPU::TextureUsage usage = GPU::TextureUsage::None;
  GPU::TextureDimension dimension = GPU::TextureDimension::Undefined;
  estruct<GPUExtent3D> size = nullptr;
  GPU::TextureFormat format = GPU::TextureFormat::Undefined;
  uint32_t mipLevelCount = 1;
  uint32_t sampleCount = 1;
  earray<GPU::TextureFormat> viewFormats = {};
};

URGE_BINDING()
struct GPULimits {
  epointer reservedParam;
  uint32_t maxTextureDimension1D;
  uint32_t maxTextureDimension2D;
  uint32_t maxTextureDimension3D;
  uint32_t maxTextureArrayLayers;
  uint32_t maxBindGroups;
  uint32_t maxBindGroupsPlusVertexBuffers;
  uint32_t maxBindingsPerBindGroup;
  uint32_t maxDynamicUniformBuffersPerPipelineLayout;
  uint32_t maxDynamicStorageBuffersPerPipelineLayout;
  uint32_t maxSampledTexturesPerShaderStage;
  uint32_t maxSamplersPerShaderStage;
  uint32_t maxStorageBuffersPerShaderStage;
  uint32_t maxStorageTexturesPerShaderStage;
  uint32_t maxUniformBuffersPerShaderStage;
  uint64_t maxUniformBufferBindingSize;
  uint64_t maxStorageBufferBindingSize;
  uint32_t minUniformBufferOffsetAlignment;
  uint32_t minStorageBufferOffsetAlignment;
  uint32_t maxVertexBuffers;
  uint64_t maxBufferSize;
  uint32_t maxVertexAttributes;
  uint32_t maxVertexBufferArrayStride;
  uint32_t maxInterStageShaderVariables;
  uint32_t maxColorAttachments;
  uint32_t maxColorAttachmentBytesPerSample;
  uint32_t maxComputeWorkgroupStorageSize;
  uint32_t maxComputeInvocationsPerWorkgroup;
  uint32_t maxComputeWorkgroupSizeX;
  uint32_t maxComputeWorkgroupSizeY;
  uint32_t maxComputeWorkgroupSizeZ;
  uint32_t maxComputeWorkgroupsPerDimension;
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
      estruct<GPUBindGroupDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBindGroupLayout> CreateBindGroupLayout(
      estruct<GPUBindGroupLayoutDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUBuffer> CreateBuffer(estruct<GPUBufferDescriptor> descriptor,
                                        URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUCommandEncoder> CreateCommandEncoder(
      estruct<GPUCommandEncoderDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUComputePipeline> CreateComputePipeline(
      estruct<GPUComputePipelineDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t CreateComputePipelineAsync(
      estruct<GPUComputePipelineDescriptor> descriptor,
      CreateComputePipelineAsyncCallback callback,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUPipelineLayout> CreatePipelineLayout(
      estruct<GPUPipelineLayoutDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUQuerySet> CreateQuerySet(
      estruct<GPUQuerySetDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPURenderPipeline> CreateRenderPipeline(
      estruct<GPURenderPipelineDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  uint64_t CreateRenderPipelineAsync(
      estruct<GPURenderPipelineDescriptor> descriptor,
      CreateRenderPipelineAsyncCallback callback,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUSampler> CreateSampler(
      estruct<GPUSamplerDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUShaderModule> CreateShaderModule(
      estruct<GPUShaderModuleDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUTexture> CreateTexture(
      estruct<GPUTextureDescriptor> descriptor,
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
  estruct<GPULimits> GetLimits(URGE_EXCEPTION);

 private:
  wgpu::Device object_;

  std::vector<std::string> features_;
  GPULimits limits_;
};

}  // namespace content

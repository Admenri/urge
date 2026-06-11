// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_resource.h"

namespace content {

///
/// GPU Buffer
///

GPUBuffer::GPUBuffer(wgpu::Buffer object) : object_(object) {}

void GPUBuffer::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

void GPUBuffer::Destroy(URGE_EXCEPTION) {
  object_.Destroy();
}

uint64_t GPUBuffer::GetSize(URGE_EXCEPTION) {
  return object_.GetSize();
}

GPU::BufferUsage GPUBuffer::GetUsage(URGE_EXCEPTION) {
  return static_cast<GPU::BufferUsage>(object_.GetUsage());
}

uint64_t GPUBuffer::MapAsync(GPU::MapMode mode,
                             size_t offset,
                             size_t size,
                             MapAsyncCallback callback,
                             URGE_EXCEPTION) {
  WGPUBufferMapCallbackInfo callback_info = {};
  callback_info.userdata1 = new MapAsyncCallback(std::move(callback));
  callback_info.callback = [](WGPUMapAsyncStatus status, WGPUStringView message,
                              void* userdata1, void* userdata2) {
    auto* callback = static_cast<MapAsyncCallback*>(userdata1);
    callback->Run(static_cast<GPU::MapAsyncStatus>(status),
                  std::string(message.data, message.length));
    delete callback;
  };

  auto future = object_.MapAsync(static_cast<wgpu::MapMode>(mode), offset, size,
                                 callback_info);
  return future.id;
}

void GPUBuffer::Unmap(URGE_EXCEPTION) {
  object_.Unmap();
}

GPU::BufferMapState GPUBuffer::GetMapState(URGE_EXCEPTION) {
  return static_cast<GPU::BufferMapState>(object_.GetMapState());
}

epointer GPUBuffer::GetMappedRange(size_t offset,
                                   size_t size,
                                   bool is_mutable,
                                   URGE_EXCEPTION) {
  const void* ptr = is_mutable ? object_.GetMappedRange(offset, size)
                               : object_.GetConstMappedRange(offset, size);
  return const_cast<epointer>(ptr);
}

///
/// GPU Texture View
///

GPUTextureView::GPUTextureView(wgpu::TextureView object) : object_(object) {}

void GPUTextureView::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

///
/// GPU Texture
///

GPUTexture::GPUTexture(wgpu::Texture object) : object_(object) {}

scoped_refptr<GPUTextureView> GPUTexture::CreateView(
    scoped_refptr<GPUTextureViewDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::TextureViewDescriptor create_desc;
  if (descriptor) {
    create_desc.label = std::string_view(descriptor->label);
    create_desc.format = static_cast<wgpu::TextureFormat>(descriptor->format);
    create_desc.dimension =
        static_cast<wgpu::TextureViewDimension>(descriptor->dimension);
    create_desc.baseMipLevel = descriptor->baseMipLevel;
    create_desc.mipLevelCount = descriptor->mipLevelCount;
    create_desc.baseArrayLayer = descriptor->baseArrayLayer;
    create_desc.arrayLayerCount = descriptor->arrayLayerCount;
    create_desc.aspect = static_cast<wgpu::TextureAspect>(descriptor->aspect);
    create_desc.usage = static_cast<wgpu::TextureUsage>(descriptor->usage);
  }

  auto view = object_.CreateView(&create_desc);
  if (!view)
    return nullptr;
  return Object::Create<GPUTextureView>(view);
}

void GPUTexture::Destroy(URGE_EXCEPTION) {
  object_.Destroy();
}

void GPUTexture::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

uint32_t GPUTexture::GetDepthOrArrayLayers(URGE_EXCEPTION) {
  return object_.GetDepthOrArrayLayers();
}

GPU::TextureDimension GPUTexture::GetDimension(URGE_EXCEPTION) {
  return static_cast<GPU::TextureDimension>(object_.GetDimension());
}

GPU::TextureFormat GPUTexture::GetFormat(URGE_EXCEPTION) {
  return static_cast<GPU::TextureFormat>(object_.GetFormat());
}

uint32_t GPUTexture::GetMipLevelCount(URGE_EXCEPTION) {
  return object_.GetMipLevelCount();
}

uint32_t GPUTexture::GetSampleCount(URGE_EXCEPTION) {
  return object_.GetSampleCount();
}

GPU::TextureViewDimension GPUTexture::GetTextureBindingViewDimension(
    URGE_EXCEPTION) {
  return static_cast<GPU::TextureViewDimension>(
      object_.GetTextureBindingViewDimension());
}

GPU::TextureUsage GPUTexture::GetUsage(URGE_EXCEPTION) {
  return static_cast<GPU::TextureUsage>(object_.GetUsage());
}

uint32_t GPUTexture::GetWidth(URGE_EXCEPTION) {
  return object_.GetWidth();
}

uint32_t GPUTexture::GetHeight(URGE_EXCEPTION) {
  return object_.GetHeight();
}

///
/// GPU Sampler
///

GPUSampler::GPUSampler(wgpu::Sampler object) : object_(object) {}

void GPUSampler::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

///
/// GPU ShaderModule
///

GPUShaderModule::GPUShaderModule(wgpu::ShaderModule object) : object_(object) {}

void GPUShaderModule::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

uint64_t GPUShaderModule::GetCompilationInfo(CompilationInfoCallback callback,
                                             URGE_EXCEPTION) {
  WGPUCompilationInfoCallbackInfo callback_info = {};
  callback_info.userdata1 = new CompilationInfoCallback(std::move(callback));
  callback_info.callback = [](WGPUCompilationInfoRequestStatus status,
                              WGPUCompilationInfo const* compilationInfo,
                              void* userdata1, void* userdata2) {
    std::vector<estring> messages;
    for (size_t i = 0; i < compilationInfo->messageCount; ++i) {
      auto raw_message = compilationInfo->messages[i].message;
      std::string out_message(raw_message.data, raw_message.length);
      messages.push_back(std::move(out_message));
    }

    auto* callback = static_cast<CompilationInfoCallback*>(userdata1);
    callback->Run(static_cast<GPU::CompilationInfoRequestStatus>(status),
                  messages);
    delete callback;
  };

  auto future = object_.GetCompilationInfo(callback_info);
  return future.id;
}

///
/// GPU BindGroupLayout
///

GPUBindGroupLayout::GPUBindGroupLayout(wgpu::BindGroupLayout object)
    : object_(object) {}

void GPUBindGroupLayout::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

///
/// GPU PipelineLayout
///

GPUPipelineLayout::GPUPipelineLayout(wgpu::PipelineLayout object)
    : object_(object) {}

void GPUPipelineLayout::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

///
/// GPU RenderPipeline
///

GPURenderPipeline::GPURenderPipeline(wgpu::RenderPipeline object)
    : object_(object) {}

void GPURenderPipeline::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

scoped_refptr<GPUBindGroupLayout> GPURenderPipeline::GetBindGroupLayout(
    uint32_t group_index,
    URGE_EXCEPTION) {
  auto result = object_.GetBindGroupLayout(group_index);
  if (!result)
    return nullptr;
  return Object::Create<GPUBindGroupLayout>(result);
}

///
/// GPU ComputePipeline
///

GPUComputePipeline::GPUComputePipeline(wgpu::ComputePipeline object)
    : object_(object) {}

void GPUComputePipeline::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

scoped_refptr<GPUBindGroupLayout> GPUComputePipeline::GetBindGroupLayout(
    uint32_t group_index,
    URGE_EXCEPTION) {
  auto result = object_.GetBindGroupLayout(group_index);
  if (!result)
    return nullptr;
  return Object::Create<GPUBindGroupLayout>(result);
}

///
/// GPU QuerySet
///

GPUQuerySet::GPUQuerySet(wgpu::QuerySet object) : object_(object) {}

void GPUQuerySet::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

void GPUQuerySet::Destroy(URGE_EXCEPTION) {
  object_.Destroy();
}

uint32_t GPUQuerySet::GetCount(URGE_EXCEPTION) {
  return object_.GetCount();
}

GPU::QueryType GPUQuerySet::GetType(URGE_EXCEPTION) {
  return static_cast<GPU::QueryType>(object_.GetType());
}

///
/// GPU BindGroup
///

GPUBindGroup::GPUBindGroup(wgpu::BindGroup object) : object_(object) {}

void GPUBindGroup::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

///
/// GPU CommandBuffer
///

GPUCommandBuffer::GPUCommandBuffer(wgpu::CommandBuffer object)
    : object_(object) {}

void GPUCommandBuffer::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(std::string_view(label));
}

}  // namespace content

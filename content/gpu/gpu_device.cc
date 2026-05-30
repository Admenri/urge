// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_device.h"

#include "magic_enum/magic_enum.hpp"

namespace content {

///
/// GPU Queue
///

GPUQueue::GPUQueue(wgpu::Queue object) : object_(object) {}

void GPUQueue::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(label);
}

uint64_t GPUQueue::OnSubmittedWorkDone(QueueWorkDoneCallback callback,
                                       URGE_EXCEPTION) {
  WGPUQueueWorkDoneCallbackInfo callback_info = {};
  callback_info.userdata1 = new QueueWorkDoneCallback(std::move(callback));
  callback_info.callback = [](WGPUQueueWorkDoneStatus status,
                              WGPUStringView message, void* userdata1,
                              void* userdata2) {
    auto* callback = static_cast<QueueWorkDoneCallback*>(userdata1);
    callback->Run(static_cast<GPU::QueueWorkDoneStatus>(status),
                  std::string_view(message.data, message.length));
    delete callback;
  };

  auto future = object_.OnSubmittedWorkDone(callback_info);
  return future.id;
}

void GPUQueue::Submit(earray<scoped_refptr<GPUCommandBuffer>> commands,
                      URGE_EXCEPTION) {
  std::vector<wgpu::CommandBuffer> buffers;
  for (auto& it : commands)
    buffers.push_back(WGPU_PTR(it));
  object_.Submit(buffers.size(), buffers.data());
}

void GPUQueue::WriteBuffer(scoped_refptr<GPUBuffer> buffer,
                           uint64_t buffer_offset,
                           epointer data,
                           size_t size,
                           URGE_EXCEPTION) {
  object_.WriteBuffer(WGPU_PTR(buffer), buffer_offset, data, size);
}

URGE_BINDING()
void GPUQueue::WriteTexture(estruct<GPUTexelCopyTextureInfo> destination,
                            epointer data,
                            size_t data_size,
                            estruct<GPUTexelCopyBufferLayout> data_layout,
                            estruct<GPUExtent3D> write_size,
                            URGE_EXCEPTION) {
  wgpu::TexelCopyTextureInfo raw_destination;
  if (auto data = destination; data) {
    raw_destination.texture = WGPU_PTR(data->texture);
    raw_destination.mipLevel = data->mipLevel;
    if (data->origin)
      raw_destination.origin = {data->origin->x, data->origin->y,
                                data->origin->z};
    raw_destination.aspect = static_cast<wgpu::TextureAspect>(data->aspect);
  }

  wgpu::TexelCopyBufferLayout raw_data_layout;
  if (data_layout) {
    raw_data_layout.offset = data_layout->offset;
    raw_data_layout.bytesPerRow = data_layout->bytesPerRow;
    raw_data_layout.rowsPerImage = data_layout->rowsPerImage;
  }

  wgpu::Extent3D raw_write_size;
  if (write_size)
    raw_write_size = {write_size->width, write_size->height,
                      write_size->depthOrArrayLayers};

  object_.WriteTexture(&raw_destination, data, data_size, &raw_data_layout,
                       &raw_write_size);
}

///
/// GPU Device
///

GPUDevice::GPUDevice(wgpu::Device object) : object_(object) {}

void GPUDevice::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(label);
}

scoped_refptr<GPUBindGroup> GPUDevice::CreateBindGroup(
    estruct<GPUBindGroupDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUBindGroupLayout> GPUDevice::CreateBindGroupLayout(
    estruct<GPUBindGroupLayoutDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUBuffer> GPUDevice::CreateBuffer(
    estruct<GPUBufferDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUCommandEncoder> GPUDevice::CreateCommandEncoder(
    estruct<GPUCommandEncoderDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUComputePipeline> GPUDevice::CreateComputePipeline(
    estruct<GPUComputePipelineDescriptor> descriptor,
    URGE_EXCEPTION) {}

uint64_t GPUDevice::CreateComputePipelineAsync(
    estruct<GPUComputePipelineDescriptor> descriptor,
    CreateComputePipelineAsyncCallback callback,
    URGE_EXCEPTION) {}

scoped_refptr<GPUPipelineLayout> GPUDevice::CreatePipelineLayout(
    estruct<GPUPipelineLayoutDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUQuerySet> GPUDevice::CreateQuerySet(
    estruct<GPUQuerySetDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPURenderPipeline> GPUDevice::CreateRenderPipeline(
    estruct<GPURenderPipelineDescriptor> descriptor,
    URGE_EXCEPTION) {}

uint64_t GPUDevice::CreateRenderPipelineAsync(
    estruct<GPURenderPipelineDescriptor> descriptor,
    CreateRenderPipelineAsyncCallback callback,
    URGE_EXCEPTION) {}

scoped_refptr<GPUSampler> GPUDevice::CreateSampler(
    estruct<GPUSamplerDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUShaderModule> GPUDevice::CreateShaderModule(
    estruct<GPUShaderModuleDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUTexture> GPUDevice::CreateTexture(
    estruct<GPUTextureDescriptor> descriptor,
    URGE_EXCEPTION) {}

scoped_refptr<GPUQueue> GPUDevice::GetQueue(URGE_EXCEPTION) {
  auto queue = object_.GetQueue();
  return Object::Create<GPUQueue>(queue);
}

void GPUDevice::PushErrorScope(GPU::ErrorFilter filter, URGE_EXCEPTION) {
  object_.PushErrorScope(static_cast<wgpu::ErrorFilter>(filter));
}

uint64_t GPUDevice::PopErrorScope(PopErrorScopeCallback callback,
                                  URGE_EXCEPTION) {
  WGPUPopErrorScopeCallbackInfo callback_info = {};
  callback_info.userdata1 = new PopErrorScopeCallback(std::move(callback));
  callback_info.callback = [](WGPUPopErrorScopeStatus status,
                              WGPUErrorType type, WGPUStringView message,
                              void* userdata1, void* userdata2) {
    auto* callback = static_cast<PopErrorScopeCallback*>(userdata1);
    callback->Run(static_cast<GPU::PopErrorScopeStatus>(status),
                  static_cast<GPU::ErrorType>(type),
                  std::string_view(message.data, message.length));
    delete callback;
  };

  auto future = object_.PopErrorScope(callback_info);
  return future.id;
}

earray<estring> GPUDevice::GetFeatures(URGE_EXCEPTION) {
  wgpu::SupportedFeatures support_features;
  object_.GetFeatures(&support_features);

  features_.clear();
  std::vector<std::string_view> out;
  for (size_t i = 0; i < support_features.featureCount; ++i) {
    std::string feature_string(
        magic_enum::enum_name(support_features.features[i]));
    auto it = features_.emplace_back(std::move(feature_string));
    out.push_back(it);
  }

  return out;
}

estruct<GPULimits> GPUDevice::GetLimits(URGE_EXCEPTION) {
  static_assert(sizeof(GPULimits) == sizeof(WGPULimits),
                "limit structure updated required.");
  object_.GetLimits(reinterpret_cast<wgpu::Limits*>(&limits_));
  return &limits_;
}

}  // namespace content

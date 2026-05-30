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
    URGE_EXCEPTION) {
  std::vector<wgpu::BindGroupEntry> entries;
  wgpu::BindGroupDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.layout = WGPU_PTR(descriptor->layout);

    for (auto& it : descriptor->entries) {
      wgpu::BindGroupEntry entry;
      entry.binding = it->binding;
      entry.buffer = WGPU_PTR(it->buffer);
      entry.offset = it->offset;
      entry.size = it->size;
      entry.sampler = WGPU_PTR(it->sampler);
      entry.textureView = WGPU_PTR(it->textureView);
      entries.push_back(std::move(entry));
    }
    create_desc.entryCount = entries.size();
    create_desc.entries = entries.data();
  }

  auto result = object_.CreateBindGroup(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUBindGroup>(result);
}

scoped_refptr<GPUBindGroupLayout> GPUDevice::CreateBindGroupLayout(
    estruct<GPUBindGroupLayoutDescriptor> descriptor,
    URGE_EXCEPTION) {
  std::vector<wgpu::BindGroupLayoutEntry> entries;
  wgpu::BindGroupLayoutDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;

    for (auto& it : descriptor->entries) {
      wgpu::BindGroupLayoutEntry entry;
      entry.binding = it->binding;
      entry.visibility = static_cast<wgpu::ShaderStage>(it->visibility);
      entry.bindingArraySize = it->bindingArraySize;

      if (it->buffer) {
        entry.buffer.type =
            static_cast<wgpu::BufferBindingType>(it->buffer->type);
        entry.buffer.hasDynamicOffset = it->buffer->hasDynamicOffset;
        entry.buffer.minBindingSize = it->buffer->minBindingSize;
      }

      if (it->sampler) {
        entry.sampler.type =
            static_cast<wgpu::SamplerBindingType>(it->sampler->type);
      }

      if (it->texture) {
        entry.texture.sampleType =
            static_cast<wgpu::TextureSampleType>(it->texture->sampleType);
        entry.texture.viewDimension =
            static_cast<wgpu::TextureViewDimension>(it->texture->viewDimension);
        entry.texture.multisampled = it->texture->multisampled;
      }

      if (it->storageTexture) {
        entry.storageTexture.access =
            static_cast<wgpu::StorageTextureAccess>(it->storageTexture->access);
        entry.storageTexture.format =
            static_cast<wgpu::TextureFormat>(it->storageTexture->format);
        entry.storageTexture.viewDimension =
            static_cast<wgpu::TextureViewDimension>(
                it->storageTexture->viewDimension);
      }

      entries.push_back(std::move(entry));
    }
    create_desc.entryCount = entries.size();
    create_desc.entries = entries.data();
  }

  auto result = object_.CreateBindGroupLayout(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUBindGroupLayout>(result);
}

scoped_refptr<GPUBuffer> GPUDevice::CreateBuffer(
    estruct<GPUBufferDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::BufferDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.usage = static_cast<wgpu::BufferUsage>(descriptor->usage);
    create_desc.size = descriptor->size;
    create_desc.mappedAtCreation = descriptor->mappedAtCreation;
  }

  auto result = object_.CreateBuffer(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUBuffer>(result);
}

scoped_refptr<GPUCommandEncoder> GPUDevice::CreateCommandEncoder(
    estruct<GPUCommandEncoderDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::CommandEncoderDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
  }

  auto result =
      object_.CreateCommandEncoder(descriptor ? &create_desc : nullptr);
  if (!result)
    return nullptr;
  return Object::Create<GPUCommandEncoder>(result);
}

scoped_refptr<GPUComputePipeline> GPUDevice::CreateComputePipeline(
    estruct<GPUComputePipelineDescriptor> descriptor,
    URGE_EXCEPTION) {
  std::vector<wgpu::ConstantEntry> constants;
  wgpu::ComputePipelineDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.layout = WGPU_PTR(descriptor->layout);

    if (descriptor->compute) {
      create_desc.compute.module = WGPU_PTR(descriptor->compute->module);
      create_desc.compute.entryPoint = descriptor->compute->entryPoint;

      for (auto& it : descriptor->compute->constants) {
        wgpu::ConstantEntry constant;
        constant.key = it->key;
        constant.value = it->value;
        constants.push_back(std::move(constant));
      }
      create_desc.compute.constantCount = constants.size();
      create_desc.compute.constants = constants.data();
    }
  }

  auto result = object_.CreateComputePipeline(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUComputePipeline>(result);
}

uint64_t GPUDevice::CreateComputePipelineAsync(
    estruct<GPUComputePipelineDescriptor> descriptor,
    CreateComputePipelineAsyncCallback callback,
    URGE_EXCEPTION) {
  std::vector<wgpu::ConstantEntry> constants;
  wgpu::ComputePipelineDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.layout = WGPU_PTR(descriptor->layout);

    if (descriptor->compute) {
      create_desc.compute.module = WGPU_PTR(descriptor->compute->module);
      create_desc.compute.entryPoint = descriptor->compute->entryPoint;

      for (auto& it : descriptor->compute->constants) {
        wgpu::ConstantEntry constant;
        constant.key = it->key;
        constant.value = it->value;
        constants.push_back(std::move(constant));
      }
      create_desc.compute.constantCount = constants.size();
      create_desc.compute.constants = constants.data();
    }
  }

  WGPUCreateComputePipelineAsyncCallbackInfo callback_info = {};
  callback_info.userdata1 =
      new CreateComputePipelineAsyncCallback(std::move(callback));
  callback_info.callback =
      [](WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline,
         WGPUStringView message, void* userdata1, void* userdata2) {
        auto* callback =
            static_cast<CreateComputePipelineAsyncCallback*>(userdata1);
        auto wrapped_pipeline =
            pipeline ? Object::Create<GPUComputePipeline>(pipeline) : nullptr;
        callback->Run(static_cast<GPU::CreatePipelineAsyncStatus>(status),
                      wrapped_pipeline,
                      std::string_view(message.data, message.length));
        delete callback;
      };

  auto future = object_.CreateComputePipelineAsync(&create_desc, callback_info);
  return future.id;
}

scoped_refptr<GPUPipelineLayout> GPUDevice::CreatePipelineLayout(
    estruct<GPUPipelineLayoutDescriptor> descriptor,
    URGE_EXCEPTION) {
  std::vector<wgpu::BindGroupLayout> layouts;
  wgpu::PipelineLayoutDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;

    for (auto& it : descriptor->bindGroupLayouts)
      layouts.push_back(WGPU_PTR(it));
    create_desc.bindGroupLayoutCount = layouts.size();
    create_desc.bindGroupLayouts = layouts.data();
    create_desc.immediateSize = descriptor->immediateSize;
  }

  auto result = object_.CreatePipelineLayout(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUPipelineLayout>(result);
}

scoped_refptr<GPUQuerySet> GPUDevice::CreateQuerySet(
    estruct<GPUQuerySetDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::QuerySetDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.type = static_cast<wgpu::QueryType>(descriptor->type);
    create_desc.count = descriptor->count;
  }

  auto result = object_.CreateQuerySet(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUQuerySet>(result);
}

scoped_refptr<GPURenderPipeline> GPUDevice::CreateRenderPipeline(
    estruct<GPURenderPipelineDescriptor> descriptor,
    URGE_EXCEPTION) {
  std::vector<wgpu::ConstantEntry> vertex_constants;
  std::vector<wgpu::VertexAttribute> vertex_attributes;
  std::vector<wgpu::VertexBufferLayout> vertex_buffers;
  wgpu::DepthStencilState depth_stencil;
  std::vector<wgpu::ConstantEntry> fragment_constants;
  std::vector<wgpu::BlendState> blend_states;
  std::vector<wgpu::ColorTargetState> color_targets;
  wgpu::FragmentState fragment_state;

  wgpu::RenderPipelineDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.layout = WGPU_PTR(descriptor->layout);

    if (descriptor->vertex) {
      create_desc.vertex.module = WGPU_PTR(descriptor->vertex->module);
      create_desc.vertex.entryPoint = descriptor->vertex->entryPoint;

      for (auto& it : descriptor->vertex->constants) {
        wgpu::ConstantEntry constant;
        constant.key = it->key;
        constant.value = it->value;
        vertex_constants.push_back(std::move(constant));
      }
      create_desc.vertex.constantCount = vertex_constants.size();
      create_desc.vertex.constants = vertex_constants.data();

      for (auto& buf : descriptor->vertex->buffers) {
        wgpu::VertexBufferLayout layout;
        layout.arrayStride = buf->arrayStride;
        layout.stepMode = static_cast<wgpu::VertexStepMode>(buf->stepMode);

        size_t base = vertex_attributes.size();
        for (auto& attr : buf->attributes) {
          wgpu::VertexAttribute attribute;
          attribute.format = static_cast<wgpu::VertexFormat>(attr->format);
          attribute.offset = attr->offset;
          attribute.shaderLocation = attr->shaderLocation;
          vertex_attributes.push_back(std::move(attribute));
        }
        layout.attributeCount = buf->attributes.size();
        layout.attributes =
            layout.attributeCount ? &vertex_attributes[base] : nullptr;
        vertex_buffers.push_back(std::move(layout));
      }
      create_desc.vertex.bufferCount = vertex_buffers.size();
      create_desc.vertex.buffers =
          vertex_buffers.empty() ? nullptr : vertex_buffers.data();
    }

    if (descriptor->primitive) {
      create_desc.primitive.topology =
          static_cast<wgpu::PrimitiveTopology>(descriptor->primitive->topology);
      create_desc.primitive.stripIndexFormat = static_cast<wgpu::IndexFormat>(
          descriptor->primitive->stripIndexFormat);
      create_desc.primitive.frontFace =
          static_cast<wgpu::FrontFace>(descriptor->primitive->frontFace);
      create_desc.primitive.cullMode =
          static_cast<wgpu::CullMode>(descriptor->primitive->cullMode);
      create_desc.primitive.unclippedDepth =
          descriptor->primitive->unclippedDepth;
    }

    if (descriptor->depthStencil) {
      depth_stencil.format =
          static_cast<wgpu::TextureFormat>(descriptor->depthStencil->format);
      depth_stencil.depthWriteEnabled =
          descriptor->depthStencil->depthWriteEnabled;
      depth_stencil.depthCompare = static_cast<wgpu::CompareFunction>(
          descriptor->depthStencil->depthCompare);
      if (descriptor->depthStencil->stencilFront)
        depth_stencil.stencilFront = {
            static_cast<wgpu::CompareFunction>(
                descriptor->depthStencil->stencilFront->compare),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilFront->failOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilFront->depthFailOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilFront->passOp)};
      if (descriptor->depthStencil->stencilBack)
        depth_stencil.stencilBack = {
            static_cast<wgpu::CompareFunction>(
                descriptor->depthStencil->stencilBack->compare),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilBack->failOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilBack->depthFailOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilBack->passOp)};
      depth_stencil.stencilReadMask = descriptor->depthStencil->stencilReadMask;
      depth_stencil.stencilWriteMask =
          descriptor->depthStencil->stencilWriteMask;
      depth_stencil.depthBias = descriptor->depthStencil->depthBias;
      depth_stencil.depthBiasSlopeScale =
          descriptor->depthStencil->depthBiasSlopeScale;
      depth_stencil.depthBiasClamp = descriptor->depthStencil->depthBiasClamp;
      create_desc.depthStencil = &depth_stencil;
    }

    if (descriptor->multisample) {
      create_desc.multisample.count = descriptor->multisample->count;
      create_desc.multisample.mask = descriptor->multisample->mask;
      create_desc.multisample.alphaToCoverageEnabled =
          descriptor->multisample->alphaToCoverageEnabled;
    }

    if (descriptor->fragment) {
      fragment_state.module = WGPU_PTR(descriptor->fragment->module);
      fragment_state.entryPoint = descriptor->fragment->entryPoint;

      for (auto& it : descriptor->fragment->constants) {
        wgpu::ConstantEntry constant;
        constant.key = it->key;
        constant.value = it->value;
        fragment_constants.push_back(std::move(constant));
      }
      fragment_state.constantCount = fragment_constants.size();
      fragment_state.constants = fragment_constants.data();

      for (auto& it : descriptor->fragment->targets) {
        wgpu::ColorTargetState target;
        target.format = static_cast<wgpu::TextureFormat>(it->format);
        target.writeMask = static_cast<wgpu::ColorWriteMask>(it->writeMask);

        if (it->blend) {
          wgpu::BlendState blend;
          blend.color.operation =
              static_cast<wgpu::BlendOperation>(it->blend->color.operation);
          blend.color.srcFactor =
              static_cast<wgpu::BlendFactor>(it->blend->color.srcFactor);
          blend.color.dstFactor =
              static_cast<wgpu::BlendFactor>(it->blend->color.dstFactor);
          blend.alpha.operation =
              static_cast<wgpu::BlendOperation>(it->blend->alpha.operation);
          blend.alpha.srcFactor =
              static_cast<wgpu::BlendFactor>(it->blend->alpha.srcFactor);
          blend.alpha.dstFactor =
              static_cast<wgpu::BlendFactor>(it->blend->alpha.dstFactor);
          blend_states.push_back(std::move(blend));
          target.blend = &blend_states.back();
        }

        color_targets.push_back(std::move(target));
      }
      fragment_state.targetCount = color_targets.size();
      fragment_state.targets = color_targets.data();
      create_desc.fragment = &fragment_state;
    }
  }

  auto result = object_.CreateRenderPipeline(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPURenderPipeline>(result);
}

uint64_t GPUDevice::CreateRenderPipelineAsync(
    estruct<GPURenderPipelineDescriptor> descriptor,
    CreateRenderPipelineAsyncCallback callback,
    URGE_EXCEPTION) {
  std::vector<wgpu::ConstantEntry> vertex_constants;
  std::vector<wgpu::VertexAttribute> vertex_attributes;
  std::vector<wgpu::VertexBufferLayout> vertex_buffers;
  wgpu::DepthStencilState depth_stencil;
  std::vector<wgpu::ConstantEntry> fragment_constants;
  std::vector<wgpu::BlendState> blend_states;
  std::vector<wgpu::ColorTargetState> color_targets;
  wgpu::FragmentState fragment_state;

  wgpu::RenderPipelineDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.layout = WGPU_PTR(descriptor->layout);

    if (descriptor->vertex) {
      create_desc.vertex.module = WGPU_PTR(descriptor->vertex->module);
      create_desc.vertex.entryPoint = descriptor->vertex->entryPoint;

      for (auto& it : descriptor->vertex->constants) {
        wgpu::ConstantEntry constant;
        constant.key = it->key;
        constant.value = it->value;
        vertex_constants.push_back(std::move(constant));
      }
      create_desc.vertex.constantCount = vertex_constants.size();
      create_desc.vertex.constants = vertex_constants.data();

      for (auto& buf : descriptor->vertex->buffers) {
        wgpu::VertexBufferLayout layout;
        layout.arrayStride = buf->arrayStride;
        layout.stepMode = static_cast<wgpu::VertexStepMode>(buf->stepMode);

        size_t base = vertex_attributes.size();
        for (auto& attr : buf->attributes) {
          wgpu::VertexAttribute attribute;
          attribute.format = static_cast<wgpu::VertexFormat>(attr->format);
          attribute.offset = attr->offset;
          attribute.shaderLocation = attr->shaderLocation;
          vertex_attributes.push_back(std::move(attribute));
        }
        layout.attributeCount = buf->attributes.size();
        layout.attributes =
            layout.attributeCount ? &vertex_attributes[base] : nullptr;
        vertex_buffers.push_back(std::move(layout));
      }
      create_desc.vertex.bufferCount = vertex_buffers.size();
      create_desc.vertex.buffers =
          vertex_buffers.empty() ? nullptr : vertex_buffers.data();
    }

    if (descriptor->primitive) {
      create_desc.primitive.topology =
          static_cast<wgpu::PrimitiveTopology>(descriptor->primitive->topology);
      create_desc.primitive.stripIndexFormat = static_cast<wgpu::IndexFormat>(
          descriptor->primitive->stripIndexFormat);
      create_desc.primitive.frontFace =
          static_cast<wgpu::FrontFace>(descriptor->primitive->frontFace);
      create_desc.primitive.cullMode =
          static_cast<wgpu::CullMode>(descriptor->primitive->cullMode);
      create_desc.primitive.unclippedDepth =
          descriptor->primitive->unclippedDepth;
    }

    if (descriptor->depthStencil) {
      depth_stencil.format =
          static_cast<wgpu::TextureFormat>(descriptor->depthStencil->format);
      depth_stencil.depthWriteEnabled =
          descriptor->depthStencil->depthWriteEnabled;
      depth_stencil.depthCompare = static_cast<wgpu::CompareFunction>(
          descriptor->depthStencil->depthCompare);
      if (descriptor->depthStencil->stencilFront)
        depth_stencil.stencilFront = {
            static_cast<wgpu::CompareFunction>(
                descriptor->depthStencil->stencilFront->compare),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilFront->failOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilFront->depthFailOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilFront->passOp)};
      if (descriptor->depthStencil->stencilBack)
        depth_stencil.stencilBack = {
            static_cast<wgpu::CompareFunction>(
                descriptor->depthStencil->stencilBack->compare),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilBack->failOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilBack->depthFailOp),
            static_cast<wgpu::StencilOperation>(
                descriptor->depthStencil->stencilBack->passOp)};
      depth_stencil.stencilReadMask = descriptor->depthStencil->stencilReadMask;
      depth_stencil.stencilWriteMask =
          descriptor->depthStencil->stencilWriteMask;
      depth_stencil.depthBias = descriptor->depthStencil->depthBias;
      depth_stencil.depthBiasSlopeScale =
          descriptor->depthStencil->depthBiasSlopeScale;
      depth_stencil.depthBiasClamp = descriptor->depthStencil->depthBiasClamp;
      create_desc.depthStencil = &depth_stencil;
    }

    if (descriptor->multisample) {
      create_desc.multisample.count = descriptor->multisample->count;
      create_desc.multisample.mask = descriptor->multisample->mask;
      create_desc.multisample.alphaToCoverageEnabled =
          descriptor->multisample->alphaToCoverageEnabled;
    }

    if (descriptor->fragment) {
      fragment_state.module = WGPU_PTR(descriptor->fragment->module);
      fragment_state.entryPoint = descriptor->fragment->entryPoint;

      for (auto& it : descriptor->fragment->constants) {
        wgpu::ConstantEntry constant;
        constant.key = it->key;
        constant.value = it->value;
        fragment_constants.push_back(std::move(constant));
      }
      fragment_state.constantCount = fragment_constants.size();
      fragment_state.constants = fragment_constants.data();

      for (auto& it : descriptor->fragment->targets) {
        wgpu::ColorTargetState target;
        target.format = static_cast<wgpu::TextureFormat>(it->format);
        target.writeMask = static_cast<wgpu::ColorWriteMask>(it->writeMask);

        if (it->blend) {
          wgpu::BlendState blend;
          blend.color.operation =
              static_cast<wgpu::BlendOperation>(it->blend->color.operation);
          blend.color.srcFactor =
              static_cast<wgpu::BlendFactor>(it->blend->color.srcFactor);
          blend.color.dstFactor =
              static_cast<wgpu::BlendFactor>(it->blend->color.dstFactor);
          blend.alpha.operation =
              static_cast<wgpu::BlendOperation>(it->blend->alpha.operation);
          blend.alpha.srcFactor =
              static_cast<wgpu::BlendFactor>(it->blend->alpha.srcFactor);
          blend.alpha.dstFactor =
              static_cast<wgpu::BlendFactor>(it->blend->alpha.dstFactor);
          blend_states.push_back(std::move(blend));
          target.blend = &blend_states.back();
        }

        color_targets.push_back(std::move(target));
      }
      fragment_state.targetCount = color_targets.size();
      fragment_state.targets = color_targets.data();
      create_desc.fragment = &fragment_state;
    }
  }

  WGPUCreateRenderPipelineAsyncCallbackInfo callback_info = {};
  callback_info.userdata1 =
      new CreateRenderPipelineAsyncCallback(std::move(callback));
  callback_info.callback = [](WGPUCreatePipelineAsyncStatus status,
                              WGPURenderPipeline pipeline,
                              WGPUStringView message, void* userdata1,
                              void* userdata2) {
    auto* callback = static_cast<CreateRenderPipelineAsyncCallback*>(userdata1);
    auto wrapped_pipeline =
        pipeline ? Object::Create<GPURenderPipeline>(pipeline) : nullptr;
    callback->Run(static_cast<GPU::CreatePipelineAsyncStatus>(status),
                  wrapped_pipeline,
                  std::string_view(message.data, message.length));
    delete callback;
  };

  auto future = object_.CreateRenderPipelineAsync(&create_desc, callback_info);
  return future.id;
}

scoped_refptr<GPUSampler> GPUDevice::CreateSampler(
    estruct<GPUSamplerDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::SamplerDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.addressModeU =
        static_cast<wgpu::AddressMode>(descriptor->addressModeU);
    create_desc.addressModeV =
        static_cast<wgpu::AddressMode>(descriptor->addressModeV);
    create_desc.addressModeW =
        static_cast<wgpu::AddressMode>(descriptor->addressModeW);
    create_desc.magFilter =
        static_cast<wgpu::FilterMode>(descriptor->magFilter);
    create_desc.minFilter =
        static_cast<wgpu::FilterMode>(descriptor->minFilter);
    create_desc.mipmapFilter =
        static_cast<wgpu::MipmapFilterMode>(descriptor->mipmapFilter);
    create_desc.lodMinClamp = descriptor->lodMinClamp;
    create_desc.lodMaxClamp = descriptor->lodMaxClamp;
    create_desc.compare =
        static_cast<wgpu::CompareFunction>(descriptor->compare);
    create_desc.maxAnisotropy = descriptor->maxAnisotropy;
  }

  auto result = object_.CreateSampler(descriptor ? &create_desc : nullptr);
  if (!result)
    return nullptr;
  return Object::Create<GPUSampler>(result);
}

scoped_refptr<GPUShaderModule> GPUDevice::CreateShaderModule(
    estruct<GPUShaderModuleDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::ShaderSourceWGSL wgsl_desc;
  wgpu::ShaderModuleDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    wgsl_desc.code = descriptor->wgslCode;
    create_desc.nextInChain = &wgsl_desc;
  }

  auto result = object_.CreateShaderModule(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUShaderModule>(result);
}

scoped_refptr<GPUTexture> GPUDevice::CreateTexture(
    estruct<GPUTextureDescriptor> descriptor,
    URGE_EXCEPTION) {
  std::vector<wgpu::TextureFormat> view_formats;
  wgpu::TextureDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
    create_desc.usage = static_cast<wgpu::TextureUsage>(descriptor->usage);
    create_desc.dimension =
        static_cast<wgpu::TextureDimension>(descriptor->dimension);
    if (descriptor->size)
      create_desc.size = {descriptor->size->width, descriptor->size->height,
                          descriptor->size->depthOrArrayLayers};
    create_desc.format = static_cast<wgpu::TextureFormat>(descriptor->format);
    create_desc.mipLevelCount = descriptor->mipLevelCount;
    create_desc.sampleCount = descriptor->sampleCount;

    for (auto& it : descriptor->viewFormats)
      view_formats.push_back(static_cast<wgpu::TextureFormat>(it));
    create_desc.viewFormatCount = view_formats.size();
    create_desc.viewFormats = view_formats.data();
  }

  auto result = object_.CreateTexture(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUTexture>(result);
}

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

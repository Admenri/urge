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

#include "core/gpu.h"

#include <string>
#include <vector>

#include "core/exception.h"
#include "core/shader.h"

namespace urge {

namespace {

wgpu::StringView View(const std::string& text) {
  return wgpu::StringView(text);
}

}  // namespace

void GPUBindGroupLayoutDescriptor::AddUniform(uint32_t binding,
                                              uint32_t visibility) {
  wgpu::BindGroupLayoutEntry entry = {};
  entry.binding = binding;
  entry.visibility = static_cast<wgpu::ShaderStage>(visibility);
  entry.buffer.type = wgpu::BufferBindingType::Uniform;
  entries.push_back(entry);
}

void GPUBindGroupLayoutDescriptor::AddStorage(uint32_t binding,
                                              uint32_t visibility,
                                              bool read_only) {
  wgpu::BindGroupLayoutEntry entry = {};
  entry.binding = binding;
  entry.visibility = static_cast<wgpu::ShaderStage>(visibility);
  entry.buffer.type = read_only ? wgpu::BufferBindingType::ReadOnlyStorage
                                : wgpu::BufferBindingType::Storage;
  entries.push_back(entry);
}

void GPUBindGroupLayoutDescriptor::AddTexture(uint32_t binding,
                                              uint32_t visibility) {
  wgpu::BindGroupLayoutEntry entry = {};
  entry.binding = binding;
  entry.visibility = static_cast<wgpu::ShaderStage>(visibility);
  entry.texture.sampleType = wgpu::TextureSampleType::Float;
  entry.texture.viewDimension = wgpu::TextureViewDimension::e2D;
  entry.texture.multisampled = false;
  entries.push_back(entry);
}

void GPUBindGroupLayoutDescriptor::AddSampler(uint32_t binding,
                                              uint32_t visibility) {
  wgpu::BindGroupLayoutEntry entry = {};
  entry.binding = binding;
  entry.visibility = static_cast<wgpu::ShaderStage>(visibility);
  entry.sampler.type = wgpu::SamplerBindingType::Filtering;
  entries.push_back(entry);
}

void GPUPipelineLayoutDescriptor::AddBindGroupLayout(
    RefPtr<GPUBindGroupLayout> layout) {
  if (!layout)
    throw Exception(Exception::kGPUError, "nil bind group layout");
  layouts.push_back(layout->handle());
}

void GPUBindGroupDescriptor::AddBuffer(uint32_t binding,
                                       RefPtr<GPUBuffer> buffer) {
  if (!buffer)
    throw Exception(Exception::kGPUError, "nil bind group buffer");
  wgpu::BindGroupEntry entry = {};
  entry.binding = binding;
  entry.buffer = buffer->handle();
  entry.offset = 0;
  entry.size = WGPU_WHOLE_SIZE;
  entries.push_back(entry);
}

void GPUBindGroupDescriptor::AddTexture(uint32_t binding,
                                        RefPtr<GPUTextureView> view) {
  if (!view)
    throw Exception(Exception::kGPUError, "nil bind group texture");
  wgpu::BindGroupEntry entry = {};
  entry.binding = binding;
  entry.textureView = view->handle();
  entries.push_back(entry);
}

void GPUBindGroupDescriptor::AddSampler(uint32_t binding,
                                        RefPtr<GPUSampler> sampler) {
  if (!sampler)
    throw Exception(Exception::kGPUError, "nil bind group sampler");
  wgpu::BindGroupEntry entry = {};
  entry.binding = binding;
  entry.sampler = sampler->handle();
  entries.push_back(entry);
}

void GPURenderPipelineDescriptor::AddVertexAttribute(uint32_t location,
                                                     uint32_t format,
                                                     uint64_t offset) {
  wgpu::VertexAttribute attribute = {};
  attribute.shaderLocation = location;
  attribute.format = static_cast<wgpu::VertexFormat>(format);
  attribute.offset = offset;
  attributes.push_back(attribute);
}

bool GPUBuffer::MapAsync(uint32_t mode, uint64_t offset, uint64_t size) {
  bool success = false;
  WGPUBufferMapCallbackInfo info = {};
  info.mode = WGPUCallbackMode_AllowProcessEvents;
  info.callback = [](WGPUMapAsyncStatus status, WGPUStringView message,
                     void* userdata1, void* userdata2) {
    *static_cast<bool*>(userdata1) = (status == WGPUMapAsyncStatus_Success);
  };
  info.userdata1 = &success;
  wgpu::Future future =
      buffer_.MapAsync(static_cast<wgpu::MapMode>(mode), offset, size, info);
  GPUDevice::Get().WaitAny(future);
  return success;
}

std::string GPUBuffer::Read(uint64_t offset, uint64_t size) {
  if (!MapAsync(static_cast<uint32_t>(wgpu::MapMode::Read), offset, size))
    throw Exception(Exception::kGPUError, "failed to map buffer for read");
  std::string result;
  const void* data = buffer_.GetConstMappedRange(offset, size);
  if (data)
    result.assign(static_cast<const char*>(data), static_cast<size_t>(size));
  buffer_.Unmap();
  return result;
}

void GPUBuffer::Write(uint64_t offset, std::string data) {
  if (!MapAsync(static_cast<uint32_t>(wgpu::MapMode::Write), offset,
                static_cast<uint64_t>(data.size())))
    throw Exception(Exception::kGPUError, "failed to map buffer for write");
  buffer_.WriteMappedRange(offset, data.data(), data.size());
  buffer_.Unmap();
}

RefPtr<GPUTextureView> GPUTexture::CreateView(
    RefPtr<GPUTextureViewDescriptor> descriptor) {
  wgpu::TextureViewDescriptor desc;
  if (descriptor) {
    desc.label = View(descriptor->label);
    desc.format = static_cast<wgpu::TextureFormat>(descriptor->format);
    desc.dimension =
        static_cast<wgpu::TextureViewDimension>(descriptor->dimension);
    desc.baseMipLevel = descriptor->base_mip_level;
    desc.mipLevelCount = descriptor->mip_level_count;
    desc.baseArrayLayer = descriptor->base_array_layer;
    desc.arrayLayerCount = descriptor->array_layer_count;
    desc.aspect = static_cast<wgpu::TextureAspect>(descriptor->aspect);
  }
  wgpu::TextureView view = texture_.CreateView(&desc);
  if (!view)
    throw Exception(Exception::kGPUError, "failed to create texture view");
  return MakeRefCounted<GPUTextureView>(view);
}

RefPtr<GPUBindGroupLayout> GPURenderPipeline::GetBindGroupLayout(
    uint32_t group_index) {
  return MakeRefCounted<GPUBindGroupLayout>(
      pipeline_.GetBindGroupLayout(group_index));
}

RefPtr<GPUBindGroupLayout> GPUComputePipeline::GetBindGroupLayout(
    uint32_t group_index) {
  return MakeRefCounted<GPUBindGroupLayout>(
      pipeline_.GetBindGroupLayout(group_index));
}

RefPtr<GPURenderPassEncoder> GPUCommandEncoder::BeginRenderPass(
    RefPtr<GPURenderPassDescriptor> descriptor) {
  if (!descriptor || !descriptor->color_view)
    throw Exception(Exception::kGPUError, "missing render pass color view");
  wgpu::RenderPassColorAttachment attachment;
  attachment.view = descriptor->color_view->handle();
  attachment.loadOp = static_cast<wgpu::LoadOp>(descriptor->load_op);
  attachment.storeOp = static_cast<wgpu::StoreOp>(descriptor->store_op);
  attachment.clearValue.r = descriptor->clear_r;
  attachment.clearValue.g = descriptor->clear_g;
  attachment.clearValue.b = descriptor->clear_b;
  attachment.clearValue.a = descriptor->clear_a;
  wgpu::RenderPassDescriptor desc;
  desc.label = View(descriptor->label);
  desc.colorAttachmentCount = 1;
  desc.colorAttachments = &attachment;
  wgpu::RenderPassEncoder encoder = encoder_.BeginRenderPass(&desc);
  if (!encoder)
    throw Exception(Exception::kGPUError, "failed to begin render pass");
  return MakeRefCounted<GPURenderPassEncoder>(encoder);
}

RefPtr<GPUComputePassEncoder> GPUCommandEncoder::BeginComputePass(
    RefPtr<GPUComputePassDescriptor> descriptor) {
  wgpu::ComputePassDescriptor desc;
  if (descriptor)
    desc.label = View(descriptor->label);
  wgpu::ComputePassEncoder encoder = encoder_.BeginComputePass(&desc);
  if (!encoder)
    throw Exception(Exception::kGPUError, "failed to begin compute pass");
  return MakeRefCounted<GPUComputePassEncoder>(encoder);
}

void GPUCommandEncoder::CopyBufferToBuffer(RefPtr<GPUBuffer> source,
                                           uint64_t source_offset,
                                           RefPtr<GPUBuffer> destination,
                                           uint64_t destination_offset,
                                           uint64_t size) {
  if (!source || !destination)
    throw Exception(Exception::kGPUError, "missing copy buffer");
  encoder_.CopyBufferToBuffer(source->handle(), source_offset,
                              destination->handle(), destination_offset, size);
}

void GPUCommandEncoder::ClearBuffer(RefPtr<GPUBuffer> buffer,
                                    uint64_t offset,
                                    uint64_t size) {
  if (!buffer)
    throw Exception(Exception::kGPUError, "missing clear buffer");
  encoder_.ClearBuffer(buffer->handle(), offset, size);
}

RefPtr<GPUCommandBuffer> GPUCommandEncoder::Finish() {
  wgpu::CommandBufferDescriptor desc;
  wgpu::CommandBuffer command_buffer = encoder_.Finish(&desc);
  if (!command_buffer)
    throw Exception(Exception::kGPUError, "failed to finish command encoder");
  return MakeRefCounted<GPUCommandBuffer>(command_buffer);
}

void GPURenderPassEncoder::SetPipeline(RefPtr<GPURenderPipeline> pipeline) {
  if (!pipeline)
    throw Exception(Exception::kGPUError, "missing render pipeline");
  encoder_.SetPipeline(pipeline->handle());
}

void GPURenderPassEncoder::SetBindGroup(uint32_t index,
                                        RefPtr<GPUBindGroup> group) {
  if (!group)
    throw Exception(Exception::kGPUError, "missing bind group");
  encoder_.SetBindGroup(index, group->handle(), 0, nullptr);
}

void GPURenderPassEncoder::SetVertexBuffer(uint32_t slot,
                                           RefPtr<GPUBuffer> buffer,
                                           uint64_t offset) {
  if (!buffer)
    throw Exception(Exception::kGPUError, "missing vertex buffer");
  encoder_.SetVertexBuffer(slot, buffer->handle(), offset, WGPU_WHOLE_SIZE);
}

void GPURenderPassEncoder::SetIndexBuffer(RefPtr<GPUBuffer> buffer,
                                          uint32_t format,
                                          uint64_t offset) {
  if (!buffer)
    throw Exception(Exception::kGPUError, "missing index buffer");
  encoder_.SetIndexBuffer(buffer->handle(),
                          static_cast<wgpu::IndexFormat>(format), offset,
                          WGPU_WHOLE_SIZE);
}

void GPURenderPassEncoder::Draw(uint32_t vertex_count,
                                uint32_t instance_count,
                                uint32_t first_vertex,
                                uint32_t first_instance) {
  encoder_.Draw(vertex_count, instance_count, first_vertex, first_instance);
}

void GPURenderPassEncoder::DrawIndexed(uint32_t index_count,
                                       uint32_t instance_count,
                                       uint32_t first_index,
                                       int32_t base_vertex,
                                       uint32_t first_instance) {
  encoder_.DrawIndexed(index_count, instance_count, first_index, base_vertex,
                       first_instance);
}

void GPURenderPassEncoder::SetViewport(float x,
                                       float y,
                                       float width,
                                       float height,
                                       float min_depth,
                                       float max_depth) {
  encoder_.SetViewport(x, y, width, height, min_depth, max_depth);
}

void GPURenderPassEncoder::SetScissorRect(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height) {
  encoder_.SetScissorRect(x, y, width, height);
}

void GPUComputePassEncoder::SetPipeline(RefPtr<GPUComputePipeline> pipeline) {
  if (!pipeline)
    throw Exception(Exception::kGPUError, "missing compute pipeline");
  encoder_.SetPipeline(pipeline->handle());
}

void GPUComputePassEncoder::SetBindGroup(uint32_t index,
                                         RefPtr<GPUBindGroup> group) {
  if (!group)
    throw Exception(Exception::kGPUError, "missing bind group");
  encoder_.SetBindGroup(index, group->handle(), 0, nullptr);
}

void GPUComputePassEncoder::DispatchWorkgroups(uint32_t x,
                                               uint32_t y,
                                               uint32_t z) {
  encoder_.DispatchWorkgroups(x, y, z);
}

void GPUQueue::Submit(RefPtr<GPUCommandBuffer> command_buffer) {
  if (!command_buffer)
    throw Exception(Exception::kGPUError, "missing command buffer");
  wgpu::CommandBuffer commands[] = {command_buffer->handle()};
  queue_.Submit(1, commands);
}

void GPUQueue::WriteBuffer(RefPtr<GPUBuffer> buffer,
                           uint64_t offset,
                           std::string data) {
  if (!buffer)
    throw Exception(Exception::kGPUError, "missing write buffer");
  queue_.WriteBuffer(buffer->handle(), offset, data.data(), data.size());
}

RefPtr<GPUQueue> GPU::queue() {
  return MakeRefCounted<GPUQueue>(GPUDevice::Get().queue());
}

RefPtr<GPUBuffer> GPU::CreateBuffer(RefPtr<GPUBufferDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil buffer descriptor");
  wgpu::BufferDescriptor desc;
  desc.label = View(descriptor->label);
  desc.size = descriptor->size;
  desc.usage = static_cast<wgpu::BufferUsage>(descriptor->usage);
  desc.mappedAtCreation = descriptor->mapped_at_creation;
  wgpu::Buffer buffer = GPUDevice::Get().device().CreateBuffer(&desc);
  if (!buffer)
    throw Exception(Exception::kGPUError, "failed to create buffer");
  return MakeRefCounted<GPUBuffer>(buffer);
}

RefPtr<GPUTexture> GPU::CreateTexture(RefPtr<GPUTextureDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil texture descriptor");
  wgpu::TextureDescriptor desc;
  desc.label = View(descriptor->label);
  desc.usage = static_cast<wgpu::TextureUsage>(descriptor->usage);
  desc.dimension = static_cast<wgpu::TextureDimension>(descriptor->dimension);
  desc.size.width = descriptor->width;
  desc.size.height = descriptor->height;
  desc.size.depthOrArrayLayers = descriptor->depth_or_array_layers;
  desc.format = static_cast<wgpu::TextureFormat>(descriptor->format);
  desc.mipLevelCount = descriptor->mip_level_count;
  desc.sampleCount = descriptor->sample_count;
  wgpu::Texture texture = GPUDevice::Get().device().CreateTexture(&desc);
  if (!texture)
    throw Exception(Exception::kGPUError, "failed to create texture");
  return MakeRefCounted<GPUTexture>(texture);
}

RefPtr<GPUSampler> GPU::CreateSampler(RefPtr<GPUSamplerDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil sampler descriptor");
  wgpu::SamplerDescriptor desc;
  desc.label = View(descriptor->label);
  desc.addressModeU =
      static_cast<wgpu::AddressMode>(descriptor->address_mode_u);
  desc.addressModeV =
      static_cast<wgpu::AddressMode>(descriptor->address_mode_v);
  desc.addressModeW =
      static_cast<wgpu::AddressMode>(descriptor->address_mode_w);
  desc.magFilter = static_cast<wgpu::FilterMode>(descriptor->mag_filter);
  desc.minFilter = static_cast<wgpu::FilterMode>(descriptor->min_filter);
  desc.mipmapFilter =
      static_cast<wgpu::MipmapFilterMode>(descriptor->mipmap_filter);
  desc.lodMinClamp = descriptor->lod_min_clamp;
  desc.lodMaxClamp = descriptor->lod_max_clamp;
  desc.compare = static_cast<wgpu::CompareFunction>(descriptor->compare);
  desc.maxAnisotropy = static_cast<uint16_t>(descriptor->max_anisotropy);
  wgpu::Sampler sampler = GPUDevice::Get().device().CreateSampler(&desc);
  if (!sampler)
    throw Exception(Exception::kGPUError, "failed to create sampler");
  return MakeRefCounted<GPUSampler>(sampler);
}

RefPtr<GPUShaderModule> GPU::CreateShaderModule(
    RefPtr<GPUShaderModuleDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil shader module descriptor");
  wgpu::ShaderModule module;
  if (descriptor->language == ShaderLanguageGlsl) {
    wgpu::ShaderStage stage = descriptor->stage == ShaderStageFragment
                                  ? wgpu::ShaderStage::Fragment
                                  : wgpu::ShaderStage::Vertex;
    Shader shader = Shader::Compile(stage, descriptor->code);
    wgpu::ShaderSourceSPIRV spirv;
    spirv.code = shader.spirv().data();
    spirv.codeSize = shader.spirv().size();
    wgpu::ShaderModuleDescriptor desc;
    desc.label = View(descriptor->label);
    desc.nextInChain = &spirv;
    module = GPUDevice::Get().device().CreateShaderModule(&desc);
  } else {
    wgpu::ShaderSourceWGSL source;
    source.code = View(descriptor->code);
    wgpu::ShaderModuleDescriptor desc;
    desc.label = View(descriptor->label);
    desc.nextInChain = &source;
    module = GPUDevice::Get().device().CreateShaderModule(&desc);
  }
  if (!module)
    throw Exception(Exception::kGPUError, "failed to create shader module");
  return MakeRefCounted<GPUShaderModule>(module);
}

RefPtr<GPUBindGroupLayout> GPU::CreateBindGroupLayout(
    RefPtr<GPUBindGroupLayoutDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil bind group layout descriptor");
  wgpu::BindGroupLayoutDescriptor desc;
  desc.label = View(descriptor->label);
  desc.entryCount = descriptor->entries.size();
  desc.entries =
      descriptor->entries.empty() ? nullptr : descriptor->entries.data();
  wgpu::BindGroupLayout layout =
      GPUDevice::Get().device().CreateBindGroupLayout(&desc);
  if (!layout)
    throw Exception(Exception::kGPUError, "failed to create bind group layout");
  return MakeRefCounted<GPUBindGroupLayout>(layout);
}

RefPtr<GPUPipelineLayout> GPU::CreatePipelineLayout(
    RefPtr<GPUPipelineLayoutDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil pipeline layout descriptor");
  wgpu::PipelineLayoutDescriptor desc;
  desc.label = View(descriptor->label);
  desc.bindGroupLayoutCount = descriptor->layouts.size();
  desc.bindGroupLayouts =
      descriptor->layouts.empty() ? nullptr : descriptor->layouts.data();
  desc.immediateSize = descriptor->immediate_size;
  wgpu::PipelineLayout layout =
      GPUDevice::Get().device().CreatePipelineLayout(&desc);
  if (!layout)
    throw Exception(Exception::kGPUError, "failed to create pipeline layout");
  return MakeRefCounted<GPUPipelineLayout>(layout);
}

RefPtr<GPUBindGroup> GPU::CreateBindGroup(
    RefPtr<GPUBindGroupDescriptor> descriptor) {
  if (!descriptor || !descriptor->layout)
    throw Exception(Exception::kGPUError, "missing bind group layout");
  wgpu::BindGroupDescriptor desc;
  desc.label = View(descriptor->label);
  desc.layout = descriptor->layout->handle();
  desc.entryCount = descriptor->entries.size();
  desc.entries =
      descriptor->entries.empty() ? nullptr : descriptor->entries.data();
  wgpu::BindGroup group = GPUDevice::Get().device().CreateBindGroup(&desc);
  if (!group)
    throw Exception(Exception::kGPUError, "failed to create bind group");
  return MakeRefCounted<GPUBindGroup>(group);
}

RefPtr<GPURenderPipeline> GPU::CreateRenderPipeline(
    RefPtr<GPURenderPipelineDescriptor> descriptor) {
  if (!descriptor || !descriptor->layout || !descriptor->vertex_module)
    throw Exception(Exception::kGPUError, "incomplete render pipeline");
  wgpu::RenderPipelineDescriptor desc;
  desc.label = View(descriptor->label);
  desc.layout = descriptor->layout->handle();
  desc.vertex.module = descriptor->vertex_module->handle();
  desc.vertex.entryPoint = View(descriptor->vertex_entry_point);
  wgpu::VertexBufferLayout vertex_buffer;
  vertex_buffer.stepMode = wgpu::VertexStepMode::Vertex;
  vertex_buffer.arrayStride = descriptor->vertex_stride;
  vertex_buffer.attributeCount = descriptor->attributes.size();
  vertex_buffer.attributes =
      descriptor->attributes.empty() ? nullptr : descriptor->attributes.data();
  if (!descriptor->attributes.empty()) {
    desc.vertex.bufferCount = 1;
    desc.vertex.buffers = &vertex_buffer;
  }
  desc.primitive.topology =
      static_cast<wgpu::PrimitiveTopology>(descriptor->topology);
  wgpu::ColorTargetState target;
  target.format = static_cast<wgpu::TextureFormat>(descriptor->format);
  wgpu::FragmentState fragment;
  if (descriptor->fragment_module) {
    fragment.module = descriptor->fragment_module->handle();
    fragment.entryPoint = View(descriptor->fragment_entry_point);
    fragment.targetCount = 1;
    fragment.targets = &target;
    desc.fragment = &fragment;
  }
  wgpu::RenderPipeline pipeline =
      GPUDevice::Get().device().CreateRenderPipeline(&desc);
  if (!pipeline)
    throw Exception(Exception::kGPUError, "failed to create render pipeline");
  return MakeRefCounted<GPURenderPipeline>(pipeline);
}

RefPtr<GPUComputePipeline> GPU::CreateComputePipeline(
    RefPtr<GPUComputePipelineDescriptor> descriptor) {
  if (!descriptor || !descriptor->layout || !descriptor->module)
    throw Exception(Exception::kGPUError, "incomplete compute pipeline");
  wgpu::ComputePipelineDescriptor desc;
  desc.label = View(descriptor->label);
  desc.layout = descriptor->layout->handle();
  desc.compute.module = descriptor->module->handle();
  desc.compute.entryPoint = View(descriptor->entry_point);
  wgpu::ComputePipeline pipeline =
      GPUDevice::Get().device().CreateComputePipeline(&desc);
  if (!pipeline)
    throw Exception(Exception::kGPUError, "failed to create compute pipeline");
  return MakeRefCounted<GPUComputePipeline>(pipeline);
}

RefPtr<GPUQuerySet> GPU::CreateQuerySet(
    RefPtr<GPUQuerySetDescriptor> descriptor) {
  if (!descriptor)
    throw Exception(Exception::kGPUError, "nil query set descriptor");
  wgpu::QuerySetDescriptor desc;
  desc.label = View(descriptor->label);
  desc.type = static_cast<wgpu::QueryType>(descriptor->type);
  desc.count = descriptor->count;
  wgpu::QuerySet query_set = GPUDevice::Get().device().CreateQuerySet(&desc);
  if (!query_set)
    throw Exception(Exception::kGPUError, "failed to create query set");
  return MakeRefCounted<GPUQuerySet>(query_set);
}

RefPtr<GPUCommandEncoder> GPU::CreateCommandEncoder() {
  wgpu::CommandEncoderDescriptor desc;
  wgpu::CommandEncoder encoder =
      GPUDevice::Get().device().CreateCommandEncoder(&desc);
  if (!encoder)
    throw Exception(Exception::kGPUError, "failed to create command encoder");
  return MakeRefCounted<GPUCommandEncoder>(encoder);
}

}  // namespace urge

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

#include "core/graphics/gpu.h"

#include <algorithm>
#include <string>
#include <vector>

#include "core/common/logger.h"

namespace urge {

namespace {

wgpu::StringView View(const std::string& text) {
  return wgpu::StringView(text);
}

wgpu::StencilFaceState DefaultStencilFace() {
  wgpu::StencilFaceState face;
  face.compare = wgpu::CompareFunction::Always;
  face.failOp = wgpu::StencilOperation::Keep;
  face.depthFailOp = wgpu::StencilOperation::Keep;
  face.passOp = wgpu::StencilOperation::Keep;
  return face;
}

wgpu::DepthStencilState DefaultDepthStencil() {
  wgpu::DepthStencilState state;
  state.format = wgpu::TextureFormat::Depth24PlusStencil8;
  state.depthWriteEnabled = wgpu::OptionalBool::False;
  state.depthCompare = wgpu::CompareFunction::Always;
  state.stencilFront = DefaultStencilFace();
  state.stencilBack = DefaultStencilFace();
  state.stencilReadMask = 0xFF;
  state.stencilWriteMask = 0xFF;
  return state;
}

wgpu::TexelCopyTextureInfo CopyTextureInfo(RefPtr<GPUTexture> texture,
                                           uint32_t mip_level,
                                           uint32_t x,
                                           uint32_t y) {
  wgpu::TexelCopyTextureInfo info;
  info.texture = texture->handle();
  info.mipLevel = mip_level;
  info.origin.x = x;
  info.origin.y = y;
  info.aspect = wgpu::TextureAspect::All;
  return info;
}

}

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
                                              uint32_t visibility,
                                              uint32_t sample_type,
                                              uint32_t view_dimension) {
  wgpu::BindGroupLayoutEntry entry = {};
  entry.binding = binding;
  entry.visibility = static_cast<wgpu::ShaderStage>(visibility);
  entry.texture.sampleType =
      sample_type ? static_cast<wgpu::TextureSampleType>(sample_type)
                  : wgpu::TextureSampleType::Float;
  entry.texture.viewDimension =
      view_dimension ? static_cast<wgpu::TextureViewDimension>(view_dimension)
                     : wgpu::TextureViewDimension::e2D;
  entry.texture.multisampled = false;
  entries.push_back(entry);
}

void GPUBindGroupLayoutDescriptor::AddSampler(uint32_t binding,
                                              uint32_t visibility,
                                              uint32_t sampler_type) {
  wgpu::BindGroupLayoutEntry entry = {};
  entry.binding = binding;
  entry.visibility = static_cast<wgpu::ShaderStage>(visibility);
  entry.sampler.type = sampler_type
                           ? static_cast<wgpu::SamplerBindingType>(sampler_type)
                           : wgpu::SamplerBindingType::Filtering;
  entries.push_back(entry);
}

void GPUPipelineLayoutDescriptor::AddBindGroupLayout(
    RefPtr<GPUBindGroupLayout> layout) {
  layouts.push_back(layout->handle());
}

void GPUBindGroupDescriptor::AddBuffer(uint32_t binding,
                                       RefPtr<GPUBuffer> buffer) {
  AddBufferRange(binding, buffer, 0, WGPU_WHOLE_SIZE);
}

void GPUBindGroupDescriptor::AddBufferRange(uint32_t binding,
                                            RefPtr<GPUBuffer> buffer,
                                            uint64_t offset,
                                            uint64_t size) {
  wgpu::BindGroupEntry entry = {};
  entry.binding = binding;
  entry.buffer = buffer->handle();
  entry.offset = offset;
  entry.size = size;
  entries.push_back(entry);
}

void GPUBindGroupDescriptor::AddTexture(uint32_t binding,
                                        RefPtr<GPUTextureView> view) {
  wgpu::BindGroupEntry entry = {};
  entry.binding = binding;
  entry.textureView = view->handle();
  entries.push_back(entry);
}

void GPUBindGroupDescriptor::AddSampler(uint32_t binding,
                                        RefPtr<GPUSampler> sampler) {
  wgpu::BindGroupEntry entry = {};
  entry.binding = binding;
  entry.sampler = sampler->handle();
  entries.push_back(entry);
}

void GPURenderPipelineDescriptor::AddVertexAttribute(uint32_t slot,
                                                     uint32_t location,
                                                     uint32_t format,
                                                     uint64_t offset) {
  if (slot >= vertex_buffers.size())
    vertex_buffers.resize(slot + 1);

  wgpu::VertexAttribute attribute = {};
  attribute.shaderLocation = location;
  attribute.format = static_cast<wgpu::VertexFormat>(format);
  attribute.offset = offset;
  vertex_buffers[slot].attributes.push_back(attribute);
}

void GPURenderPipelineDescriptor::SetVertexBuffer(uint32_t slot,
                                                  uint64_t stride,
                                                  uint32_t step_mode) {
  if (slot >= vertex_buffers.size())
    vertex_buffers.resize(slot + 1);
  vertex_buffers[slot].stride = stride;
  vertex_buffers[slot].step_mode = step_mode;
}

void GPURenderPipelineDescriptor::AddColorTarget(uint32_t target_format,
                                                 uint32_t write_mask) {
  ColorTarget target;
  target.format = target_format;
  target.write_mask = write_mask;
  color_targets.push_back(target);
}

void GPURenderPipelineDescriptor::SetColorBlend(uint32_t index,
                                                uint32_t color_op,
                                                uint32_t src_factor,
                                                uint32_t dst_factor,
                                                uint32_t alpha_op,
                                                uint32_t alpha_src,
                                                uint32_t alpha_dst) {
  if (index >= color_targets.size())
    color_targets.resize(index + 1);

  wgpu::BlendComponent color;
  color.operation = static_cast<wgpu::BlendOperation>(color_op);
  color.srcFactor = static_cast<wgpu::BlendFactor>(src_factor);
  color.dstFactor = static_cast<wgpu::BlendFactor>(dst_factor);

  wgpu::BlendComponent alpha;
  alpha.operation =
      static_cast<wgpu::BlendOperation>(alpha_op ? alpha_op : color_op);
  alpha.srcFactor =
      static_cast<wgpu::BlendFactor>(alpha_src ? alpha_src : src_factor);
  alpha.dstFactor =
      static_cast<wgpu::BlendFactor>(alpha_dst ? alpha_dst : dst_factor);

  wgpu::BlendState blend;
  blend.color = color;
  blend.alpha = alpha;
  color_targets[index].blend = blend;
}

void GPURenderPipelineDescriptor::SetDepthStencil(uint32_t depth_format,
                                                  bool depth_write,
                                                  uint32_t depth_compare,
                                                  int32_t depth_bias,
                                                  float bias_slope_scale,
                                                  float bias_clamp) {
  wgpu::DepthStencilState state = DefaultDepthStencil();
  state.format = static_cast<wgpu::TextureFormat>(depth_format);
  state.depthWriteEnabled =
      depth_write ? wgpu::OptionalBool::True : wgpu::OptionalBool::False;
  state.depthCompare = static_cast<wgpu::CompareFunction>(depth_compare);
  state.depthBias = depth_bias;
  state.depthBiasSlopeScale = bias_slope_scale;
  state.depthBiasClamp = bias_clamp;
  depth_stencil = state;
}

void GPURenderPipelineDescriptor::SetStencilFront(uint32_t compare,
                                                  uint32_t fail_op,
                                                  uint32_t depth_fail_op,
                                                  uint32_t pass_op) {
  if (!depth_stencil)
    depth_stencil = DefaultDepthStencil();
  wgpu::StencilFaceState face;
  face.compare = static_cast<wgpu::CompareFunction>(compare);
  face.failOp = static_cast<wgpu::StencilOperation>(fail_op);
  face.depthFailOp = static_cast<wgpu::StencilOperation>(depth_fail_op);
  face.passOp = static_cast<wgpu::StencilOperation>(pass_op);
  depth_stencil->stencilFront = face;
}

void GPURenderPipelineDescriptor::SetStencilBack(uint32_t compare,
                                                 uint32_t fail_op,
                                                 uint32_t depth_fail_op,
                                                 uint32_t pass_op) {
  if (!depth_stencil)
    depth_stencil = DefaultDepthStencil();
  wgpu::StencilFaceState face;
  face.compare = static_cast<wgpu::CompareFunction>(compare);
  face.failOp = static_cast<wgpu::StencilOperation>(fail_op);
  face.depthFailOp = static_cast<wgpu::StencilOperation>(depth_fail_op);
  face.passOp = static_cast<wgpu::StencilOperation>(pass_op);
  depth_stencil->stencilBack = face;
}

void GPURenderPipelineDescriptor::SetStencilMasks(uint32_t read_mask,
                                                  uint32_t write_mask) {
  if (!depth_stencil)
    depth_stencil = DefaultDepthStencil();
  depth_stencil->stencilReadMask = read_mask;
  depth_stencil->stencilWriteMask = write_mask;
}

void GPURenderPipelineDescriptor::SetPrimitive(uint32_t primitive_topology,
                                               uint32_t cull,
                                               uint32_t face,
                                               uint32_t strip_index) {
  topology = primitive_topology;
  if (cull)
    cull_mode = cull;
  if (face)
    front_face = face;
  if (strip_index)
    strip_index_format = strip_index;
}

void GPURenderPipelineDescriptor::SetMultisample(
    uint32_t count,
    uint32_t mask,
    bool alpha_to_coverage_enabled) {
  sample_count = std::max(1u, count);
  sample_mask = mask;
  alpha_to_coverage = alpha_to_coverage_enabled;
}

void GPURenderPassDescriptor::AddColorAttachment(RefPtr<GPUTextureView> view,
                                                 uint32_t attachment_load_op,
                                                 uint32_t attachment_store_op,
                                                 float r,
                                                 float g,
                                                 float b,
                                                 float a) {
  ColorAttachment attachment;
  attachment.view = view;
  attachment.load_op = attachment_load_op;
  attachment.store_op = attachment_store_op;
  attachment.clear_r = r;
  attachment.clear_g = g;
  attachment.clear_b = b;
  attachment.clear_a = a;
  color_attachments.push_back(attachment);
}

void GPURenderPassDescriptor::SetColorResolve(RefPtr<GPUTextureView> resolve) {
  if (color_attachments.empty()) {
    LOGGER_ERROR("[GPU] set_color_resolve: add a color attachment first");
    return;
  }
  color_attachments.back().resolve = resolve;
}

void GPURenderPassDescriptor::SetDepthStencilAttachment(
    RefPtr<GPUTextureView> view,
    uint32_t depth_load,
    uint32_t depth_store,
    float depth_clear,
    uint32_t stencil_load,
    uint32_t stencil_store,
    uint32_t stencil_clear,
    bool read_only_depth,
    bool read_only_stencil) {
  depth_view = view;
  depth_load_op = depth_load;
  depth_store_op = depth_store;
  depth_clear_value = depth_clear;
  stencil_load_op = stencil_load;
  stencil_store_op = stencil_store;
  stencil_clear_value = stencil_clear;
  depth_read_only = read_only_depth;
  stencil_read_only = read_only_stencil;
}

void GPURenderPassDescriptor::SetOcclusionQuerySet(
    RefPtr<GPUQuerySet> query_set) {
  occlusion_query_set = query_set;
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
  if (!MapAsync(static_cast<uint32_t>(wgpu::MapMode::Read), offset, size)) {
    LOGGER_ERROR("[GPU] buffer read: mapping {} bytes at {} failed", size,
                 offset);
    return {};
  }
  std::string result;
  const void* data = buffer_.GetConstMappedRange(offset, size);
  if (data)
    result.assign(static_cast<const char*>(data), static_cast<size_t>(size));
  buffer_.Unmap();
  return result;
}

void GPUBuffer::Write(uint64_t offset, std::string data) {
  if (!MapAsync(static_cast<uint32_t>(wgpu::MapMode::Write), offset,
                static_cast<uint64_t>(data.size()))) {
    LOGGER_ERROR("[GPU] buffer write: mapping {} bytes at {} failed",
                 data.size(), offset);
    return;
  }
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
  std::vector<wgpu::RenderPassColorAttachment> attachments;
  attachments.reserve(descriptor->color_attachments.size());
  for (const auto& source : descriptor->color_attachments) {
    wgpu::RenderPassColorAttachment attachment;
    attachment.view = source.view->handle();
    attachment.resolveTarget =
        source.resolve ? source.resolve->handle() : nullptr;
    attachment.loadOp = static_cast<wgpu::LoadOp>(source.load_op);
    attachment.storeOp = static_cast<wgpu::StoreOp>(source.store_op);
    attachment.clearValue.r = source.clear_r;
    attachment.clearValue.g = source.clear_g;
    attachment.clearValue.b = source.clear_b;
    attachment.clearValue.a = source.clear_a;
    attachments.push_back(attachment);
  }

  if (attachments.empty() && !descriptor->depth_view) {
    LOGGER_ERROR("[GPU] begin_render_pass: the descriptor has no attachment");
    return nullptr;
  }

  wgpu::RenderPassDepthStencilAttachment depth_stencil;
  if (descriptor->depth_view) {
    depth_stencil.view = descriptor->depth_view->handle();
    depth_stencil.depthLoadOp =
        static_cast<wgpu::LoadOp>(descriptor->depth_load_op);
    depth_stencil.depthStoreOp =
        static_cast<wgpu::StoreOp>(descriptor->depth_store_op);
    depth_stencil.depthClearValue = descriptor->depth_clear_value;
    depth_stencil.depthReadOnly = descriptor->depth_read_only;
    depth_stencil.stencilLoadOp =
        static_cast<wgpu::LoadOp>(descriptor->stencil_load_op);
    depth_stencil.stencilStoreOp =
        static_cast<wgpu::StoreOp>(descriptor->stencil_store_op);
    depth_stencil.stencilClearValue = descriptor->stencil_clear_value;
    depth_stencil.stencilReadOnly = descriptor->stencil_read_only;
  }

  wgpu::RenderPassDescriptor desc;
  desc.label = View(descriptor->label);
  desc.colorAttachmentCount = attachments.size();
  desc.colorAttachments = attachments.data();
  desc.depthStencilAttachment =
      descriptor->depth_view ? &depth_stencil : nullptr;
  desc.occlusionQuerySet = descriptor->occlusion_query_set
                               ? descriptor->occlusion_query_set->handle()
                               : nullptr;

  wgpu::RenderPassEncoder encoder = encoder_.BeginRenderPass(&desc);
  return MakeRefCounted<GPURenderPassEncoder>(encoder);
}

RefPtr<GPUComputePassEncoder> GPUCommandEncoder::BeginComputePass(
    RefPtr<GPUComputePassDescriptor> descriptor) {
  wgpu::ComputePassDescriptor desc;
  if (descriptor)
    desc.label = View(descriptor->label);
  wgpu::ComputePassEncoder encoder = encoder_.BeginComputePass(&desc);
  return MakeRefCounted<GPUComputePassEncoder>(encoder);
}

void GPUCommandEncoder::CopyBufferToBuffer(RefPtr<GPUBuffer> source,
                                           uint64_t source_offset,
                                           RefPtr<GPUBuffer> destination,
                                           uint64_t destination_offset,
                                           uint64_t size) {
  encoder_.CopyBufferToBuffer(source->handle(), source_offset,
                              destination->handle(), destination_offset, size);
}

void GPUCommandEncoder::CopyBufferToTexture(RefPtr<GPUBuffer> source,
                                            uint32_t bytes_per_row,
                                            uint32_t rows_per_image,
                                            RefPtr<GPUTexture> destination,
                                            uint32_t destination_mip_level,
                                            uint32_t destination_x,
                                            uint32_t destination_y,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t depth_or_array_layers) {
  wgpu::TexelCopyBufferInfo source_info;
  source_info.buffer = source->handle();
  source_info.layout.offset = 0;
  source_info.layout.bytesPerRow = bytes_per_row;
  source_info.layout.rowsPerImage = rows_per_image;

  const wgpu::TexelCopyTextureInfo destination_info = CopyTextureInfo(
      destination, destination_mip_level, destination_x, destination_y);

  wgpu::Extent3D copy_size;
  copy_size.width = width;
  copy_size.height = height;
  copy_size.depthOrArrayLayers = depth_or_array_layers;

  encoder_.CopyBufferToTexture(&source_info, &destination_info, &copy_size);
}

void GPUCommandEncoder::CopyTextureToBuffer(RefPtr<GPUTexture> source,
                                            uint32_t source_mip_level,
                                            uint32_t source_x,
                                            uint32_t source_y,
                                            RefPtr<GPUBuffer> destination,
                                            uint32_t bytes_per_row,
                                            uint32_t rows_per_image,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t depth_or_array_layers) {
  const wgpu::TexelCopyTextureInfo source_info =
      CopyTextureInfo(source, source_mip_level, source_x, source_y);

  wgpu::TexelCopyBufferInfo destination_info;
  destination_info.buffer = destination->handle();
  destination_info.layout.offset = 0;
  destination_info.layout.bytesPerRow = bytes_per_row;
  destination_info.layout.rowsPerImage = rows_per_image;

  wgpu::Extent3D copy_size;
  copy_size.width = width;
  copy_size.height = height;
  copy_size.depthOrArrayLayers = depth_or_array_layers;

  encoder_.CopyTextureToBuffer(&source_info, &destination_info, &copy_size);
}

void GPUCommandEncoder::CopyTextureToTexture(RefPtr<GPUTexture> source,
                                             uint32_t source_mip_level,
                                             uint32_t source_x,
                                             uint32_t source_y,
                                             RefPtr<GPUTexture> destination,
                                             uint32_t destination_mip_level,
                                             uint32_t destination_x,
                                             uint32_t destination_y,
                                             uint32_t width,
                                             uint32_t height,
                                             uint32_t depth_or_array_layers) {
  if (source->handle().Get() == destination->handle().Get()) {
    LOGGER_ERROR(
        "[GPU] copy_texture_to_texture: source and destination are the same");
    return;
  }

  const wgpu::TexelCopyTextureInfo source_info =
      CopyTextureInfo(source, source_mip_level, source_x, source_y);
  const wgpu::TexelCopyTextureInfo destination_info = CopyTextureInfo(
      destination, destination_mip_level, destination_x, destination_y);

  wgpu::Extent3D copy_size;
  copy_size.width = width;
  copy_size.height = height;
  copy_size.depthOrArrayLayers = depth_or_array_layers;

  encoder_.CopyTextureToTexture(&source_info, &destination_info, &copy_size);
}

void GPUCommandEncoder::ResolveQuerySet(RefPtr<GPUQuerySet> query_set,
                                        uint32_t first_query,
                                        uint32_t query_count,
                                        RefPtr<GPUBuffer> destination,
                                        uint64_t destination_offset) {
  encoder_.ResolveQuerySet(query_set->handle(), first_query, query_count,
                           destination->handle(), destination_offset);
}

void GPUCommandEncoder::ClearBuffer(RefPtr<GPUBuffer> buffer,
                                    uint64_t offset,
                                    uint64_t size) {
  encoder_.ClearBuffer(buffer->handle(), offset, size);
}

void GPUCommandEncoder::InsertDebugMarker(std::string marker_label) {
  encoder_.InsertDebugMarker(View(marker_label));
}

void GPUCommandEncoder::PushDebugGroup(std::string group_label) {
  encoder_.PushDebugGroup(View(group_label));
}

void GPUCommandEncoder::PopDebugGroup() {
  encoder_.PopDebugGroup();
}

RefPtr<GPUCommandBuffer> GPUCommandEncoder::Finish() {
  wgpu::CommandBufferDescriptor desc;
  wgpu::CommandBuffer command_buffer = encoder_.Finish(&desc);
  return MakeRefCounted<GPUCommandBuffer>(command_buffer);
}

void GPURenderPassEncoder::SetPipeline(RefPtr<GPURenderPipeline> pipeline) {
  encoder_.SetPipeline(pipeline->handle());
}

void GPURenderPassEncoder::SetBindGroup(uint32_t index,
                                        RefPtr<GPUBindGroup> group) {
  encoder_.SetBindGroup(index, group->handle(), 0, nullptr);
}

void GPURenderPassEncoder::SetVertexBuffer(uint32_t slot,
                                           RefPtr<GPUBuffer> buffer,
                                           uint64_t offset) {
  encoder_.SetVertexBuffer(slot, buffer->handle(), offset, WGPU_WHOLE_SIZE);
}

void GPURenderPassEncoder::SetIndexBuffer(RefPtr<GPUBuffer> buffer,
                                          uint32_t format,
                                          uint64_t offset) {
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

void GPURenderPassEncoder::DrawIndirect(RefPtr<GPUBuffer> indirect_buffer,
                                        uint64_t indirect_offset) {
  encoder_.DrawIndirect(indirect_buffer->handle(), indirect_offset);
}

void GPURenderPassEncoder::DrawIndexedIndirect(
    RefPtr<GPUBuffer> indirect_buffer,
    uint64_t indirect_offset) {
  encoder_.DrawIndexedIndirect(indirect_buffer->handle(), indirect_offset);
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

void GPURenderPassEncoder::SetStencilReference(uint32_t reference) {
  encoder_.SetStencilReference(reference);
}

void GPURenderPassEncoder::SetBlendConstant(float r,
                                            float g,
                                            float b,
                                            float a) {
  wgpu::Color color;
  color.r = r;
  color.g = g;
  color.b = b;
  color.a = a;
  encoder_.SetBlendConstant(&color);
}

void GPURenderPassEncoder::InsertDebugMarker(std::string marker_label) {
  encoder_.InsertDebugMarker(View(marker_label));
}

void GPURenderPassEncoder::PushDebugGroup(std::string group_label) {
  encoder_.PushDebugGroup(View(group_label));
}

void GPURenderPassEncoder::PopDebugGroup() {
  encoder_.PopDebugGroup();
}

void GPUComputePassEncoder::SetPipeline(RefPtr<GPUComputePipeline> pipeline) {
  encoder_.SetPipeline(pipeline->handle());
}

void GPUComputePassEncoder::SetBindGroup(uint32_t index,
                                         RefPtr<GPUBindGroup> group) {
  encoder_.SetBindGroup(index, group->handle(), 0, nullptr);
}

void GPUComputePassEncoder::DispatchWorkgroups(uint32_t x,
                                               uint32_t y,
                                               uint32_t z) {
  encoder_.DispatchWorkgroups(x, y, z);
}

void GPUComputePassEncoder::InsertDebugMarker(std::string marker_label) {
  encoder_.InsertDebugMarker(View(marker_label));
}

void GPUComputePassEncoder::PushDebugGroup(std::string group_label) {
  encoder_.PushDebugGroup(View(group_label));
}

void GPUComputePassEncoder::PopDebugGroup() {
  encoder_.PopDebugGroup();
}

void GPUQueue::Submit(RefPtr<GPUCommandBuffer> command_buffer) {
  wgpu::CommandBuffer commands[] = {command_buffer->handle()};
  queue_.Submit(1, commands);
}

void GPUQueue::WriteBuffer(RefPtr<GPUBuffer> buffer,
                           uint64_t offset,
                           std::string data) {
  queue_.WriteBuffer(buffer->handle(), offset, data.data(), data.size());
}

void GPUQueue::WriteTexture(RefPtr<GPUTexture> texture,
                            uint32_t mip_level,
                            uint32_t x,
                            uint32_t y,
                            uint32_t width,
                            uint32_t height,
                            std::string data,
                            uint32_t bytes_per_row,
                            uint32_t rows_per_image) {
  const wgpu::TexelCopyTextureInfo destination =
      CopyTextureInfo(texture, mip_level, x, y);

  wgpu::TexelCopyBufferLayout layout;
  layout.offset = 0;
  layout.bytesPerRow = bytes_per_row ? bytes_per_row : width * 4;
  layout.rowsPerImage = rows_per_image;

  wgpu::Extent3D write_size;
  write_size.width = width;
  write_size.height = height;
  write_size.depthOrArrayLayers = 1;

  queue_.WriteTexture(&destination, data.data(), data.size(), &layout,
                      &write_size);
}

RefPtr<GPUQueue> GPU::queue() {
  return MakeRefCounted<GPUQueue>(GPUDevice::Get().queue());
}

RefPtr<GPUBuffer> GPU::CreateBuffer(RefPtr<GPUBufferDescriptor> descriptor) {
  wgpu::BufferDescriptor desc;
  desc.label = View(descriptor->label);
  desc.size = descriptor->size;
  desc.usage = static_cast<wgpu::BufferUsage>(descriptor->usage);
  desc.mappedAtCreation = descriptor->mapped_at_creation;
  wgpu::Buffer buffer = GPUDevice::Get().device().CreateBuffer(&desc);
  return MakeRefCounted<GPUBuffer>(buffer);
}

RefPtr<GPUTexture> GPU::CreateTexture(RefPtr<GPUTextureDescriptor> descriptor) {
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
  return MakeRefCounted<GPUTexture>(texture);
}

RefPtr<GPUSampler> GPU::CreateSampler(RefPtr<GPUSamplerDescriptor> descriptor) {
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
  return MakeRefCounted<GPUSampler>(sampler);
}

RefPtr<GPUShaderModule> GPU::CreateShaderModule(
    RefPtr<GPUShaderModuleDescriptor> descriptor) {
  if (descriptor->code.empty()) {
    LOGGER_ERROR("[GPU] create_shader_module: the descriptor has no source");
    return nullptr;
  }
  wgpu::ShaderSourceWGSL source;
  source.code = View(descriptor->code);
  wgpu::ShaderModuleDescriptor desc;
  desc.label = View(descriptor->label);
  desc.nextInChain = &source;
  wgpu::ShaderModule module =
      GPUDevice::Get().device().CreateShaderModule(&desc);
  return MakeRefCounted<GPUShaderModule>(module);
}

RefPtr<GPUBindGroupLayout> GPU::CreateBindGroupLayout(
    RefPtr<GPUBindGroupLayoutDescriptor> descriptor) {
  wgpu::BindGroupLayoutDescriptor desc;
  desc.label = View(descriptor->label);
  desc.entryCount = descriptor->entries.size();
  desc.entries =
      descriptor->entries.empty() ? nullptr : descriptor->entries.data();
  wgpu::BindGroupLayout layout =
      GPUDevice::Get().device().CreateBindGroupLayout(&desc);
  return MakeRefCounted<GPUBindGroupLayout>(layout);
}

RefPtr<GPUPipelineLayout> GPU::CreatePipelineLayout(
    RefPtr<GPUPipelineLayoutDescriptor> descriptor) {
  wgpu::PipelineLayoutDescriptor desc;
  desc.label = View(descriptor->label);
  desc.bindGroupLayoutCount = descriptor->layouts.size();
  desc.bindGroupLayouts =
      descriptor->layouts.empty() ? nullptr : descriptor->layouts.data();
  desc.immediateSize = descriptor->immediate_size;
  wgpu::PipelineLayout layout =
      GPUDevice::Get().device().CreatePipelineLayout(&desc);
  return MakeRefCounted<GPUPipelineLayout>(layout);
}

RefPtr<GPUBindGroup> GPU::CreateBindGroup(
    RefPtr<GPUBindGroupDescriptor> descriptor) {
  if (!descriptor->layout) {
    LOGGER_ERROR("[GPU] create_bind_group: the descriptor has no layout");
    return nullptr;
  }
  wgpu::BindGroupDescriptor desc;
  desc.label = View(descriptor->label);
  desc.layout = descriptor->layout->handle();
  desc.entryCount = descriptor->entries.size();
  desc.entries =
      descriptor->entries.empty() ? nullptr : descriptor->entries.data();
  wgpu::BindGroup group = GPUDevice::Get().device().CreateBindGroup(&desc);
  return MakeRefCounted<GPUBindGroup>(group);
}

RefPtr<GPURenderPipeline> GPU::CreateRenderPipeline(
    RefPtr<GPURenderPipelineDescriptor> descriptor) {
  if (!descriptor->layout || !descriptor->vertex_module) {
    LOGGER_ERROR(
        "[GPU] create_render_pipeline: the descriptor needs a layout and a "
        "vertex module");
    return nullptr;
  }

  wgpu::RenderPipelineDescriptor desc;
  desc.label = View(descriptor->label);
  desc.layout = descriptor->layout->handle();
  desc.vertex.module = descriptor->vertex_module->handle();
  desc.vertex.entryPoint = View(descriptor->vertex_entry_point);

  std::vector<wgpu::VertexBufferLayout> vertex_buffers;
  vertex_buffers.reserve(descriptor->vertex_buffers.size());
  for (const auto& source : descriptor->vertex_buffers) {
    wgpu::VertexBufferLayout layout;
    layout.stepMode = static_cast<wgpu::VertexStepMode>(source.step_mode);
    layout.arrayStride = source.stride;
    layout.attributeCount = source.attributes.size();
    layout.attributes =
        source.attributes.empty() ? nullptr : source.attributes.data();
    vertex_buffers.push_back(layout);
  }
  if (!vertex_buffers.empty()) {
    desc.vertex.bufferCount = vertex_buffers.size();
    desc.vertex.buffers = vertex_buffers.data();
  }

  desc.primitive.topology =
      static_cast<wgpu::PrimitiveTopology>(descriptor->topology);
  desc.primitive.cullMode = static_cast<wgpu::CullMode>(descriptor->cull_mode);
  desc.primitive.frontFace =
      static_cast<wgpu::FrontFace>(descriptor->front_face);
  desc.primitive.stripIndexFormat =
      static_cast<wgpu::IndexFormat>(descriptor->strip_index_format);

  desc.multisample.count = descriptor->sample_count;
  desc.multisample.mask = descriptor->sample_mask;
  desc.multisample.alphaToCoverageEnabled = descriptor->alpha_to_coverage;

  std::vector<wgpu::ColorTargetState> targets;
  targets.reserve(descriptor->color_targets.size());
  for (const auto& source : descriptor->color_targets) {
    wgpu::ColorTargetState target;
    target.format = static_cast<wgpu::TextureFormat>(source.format);
    target.blend = source.blend ? &*source.blend : nullptr;
    target.writeMask = static_cast<wgpu::ColorWriteMask>(source.write_mask);
    targets.push_back(target);
  }

  wgpu::FragmentState fragment;
  if (descriptor->fragment_module) {
    if (targets.empty()) {
      LOGGER_ERROR(
          "[GPU] create_render_pipeline: a fragment stage needs a color "
          "target");
      return nullptr;
    }
    fragment.module = descriptor->fragment_module->handle();
    fragment.entryPoint = View(descriptor->fragment_entry_point);
    fragment.targetCount = targets.size();
    fragment.targets = targets.data();
    desc.fragment = &fragment;
  }

  wgpu::DepthStencilState depth_stencil;
  if (descriptor->depth_stencil) {
    depth_stencil = *descriptor->depth_stencil;
    desc.depthStencil = &depth_stencil;
  }

  wgpu::RenderPipeline pipeline =
      GPUDevice::Get().device().CreateRenderPipeline(&desc);
  return MakeRefCounted<GPURenderPipeline>(pipeline);
}

RefPtr<GPUComputePipeline> GPU::CreateComputePipeline(
    RefPtr<GPUComputePipelineDescriptor> descriptor) {
  if (!descriptor->layout || !descriptor->module) {
    LOGGER_ERROR(
        "[GPU] create_compute_pipeline: the descriptor needs a layout and a "
        "module");
    return nullptr;
  }
  wgpu::ComputePipelineDescriptor desc;
  desc.label = View(descriptor->label);
  desc.layout = descriptor->layout->handle();
  desc.compute.module = descriptor->module->handle();
  desc.compute.entryPoint = View(descriptor->entry_point);
  wgpu::ComputePipeline pipeline =
      GPUDevice::Get().device().CreateComputePipeline(&desc);
  return MakeRefCounted<GPUComputePipeline>(pipeline);
}

RefPtr<GPUQuerySet> GPU::CreateQuerySet(
    RefPtr<GPUQuerySetDescriptor> descriptor) {
  wgpu::QuerySetDescriptor desc;
  desc.label = View(descriptor->label);
  desc.type = static_cast<wgpu::QueryType>(descriptor->type);
  desc.count = descriptor->count;
  wgpu::QuerySet query_set = GPUDevice::Get().device().CreateQuerySet(&desc);
  return MakeRefCounted<GPUQuerySet>(query_set);
}

RefPtr<GPUCommandEncoder> GPU::CreateCommandEncoder() {
  wgpu::CommandEncoderDescriptor desc;
  wgpu::CommandEncoder encoder =
      GPUDevice::Get().device().CreateCommandEncoder(&desc);
  return MakeRefCounted<GPUCommandEncoder>(encoder);
}

}

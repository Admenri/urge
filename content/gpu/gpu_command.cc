// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_command.h"

namespace content {

///
/// GPU ComputePassEncoder
///

GPUComputePassEncoder::GPUComputePassEncoder(wgpu::ComputePassEncoder object)
    : object_(object) {}

void GPUComputePassEncoder::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(label);
}

void GPUComputePassEncoder::SetBindGroup(uint32_t group_index,
                                         scoped_refptr<GPUBindGroup> group,
                                         earray<uint32_t> dynamic_offsets,
                                         URGE_EXCEPTION) {
  object_.SetBindGroup(group_index, WGPU_PTR(group), dynamic_offsets.size(),
                       dynamic_offsets.data());
}

void GPUComputePassEncoder::SetPipeline(
    scoped_refptr<GPUComputePipeline> pipeline,
    URGE_EXCEPTION) {
  object_.SetPipeline(WGPU_PTR(pipeline));
}

void GPUComputePassEncoder::DispatchWorkgroups(uint32_t workgroup_count_x,
                                               uint32_t workgroup_count_y,
                                               uint32_t workgroup_count_z,
                                               URGE_EXCEPTION) {
  object_.DispatchWorkgroups(workgroup_count_x, workgroup_count_y,
                             workgroup_count_z);
}

void GPUComputePassEncoder::DispatchWorkgroupsIndirect(
    scoped_refptr<GPUBuffer> indirect_buffer,
    uint64_t indirect_offset,
    URGE_EXCEPTION) {
  object_.DispatchWorkgroupsIndirect(WGPU_PTR(indirect_buffer),
                                     indirect_offset);
}

void GPUComputePassEncoder::PushDebugGroup(estring group_label,
                                           URGE_EXCEPTION) {
  object_.PushDebugGroup(group_label);
}

void GPUComputePassEncoder::InsertDebugMarker(estring marker_label,
                                              URGE_EXCEPTION) {
  object_.InsertDebugMarker(marker_label);
}

void GPUComputePassEncoder::PopDebugGroup(URGE_EXCEPTION) {
  object_.PopDebugGroup();
}

void GPUComputePassEncoder::End(URGE_EXCEPTION) {
  object_.End();
}

///
/// GPU RenderPassEncoder
///

GPURenderPassEncoder::GPURenderPassEncoder(wgpu::RenderPassEncoder object)
    : object_(object) {}

void GPURenderPassEncoder::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(label);
}

void GPURenderPassEncoder::BeginOcclusionQuery(uint32_t query_index,
                                               URGE_EXCEPTION) {
  object_.BeginOcclusionQuery(query_index);
}

void GPURenderPassEncoder::EndOcclusionQuery(URGE_EXCEPTION) {
  object_.EndOcclusionQuery();
}

void GPURenderPassEncoder::Draw(uint32_t vertex_count,
                                uint32_t instance_count,
                                uint32_t first_vertex,
                                uint32_t first_instance,
                                URGE_EXCEPTION) {
  object_.Draw(vertex_count, instance_count, first_vertex, first_instance);
}

void GPURenderPassEncoder::DrawIndexed(uint32_t index_count,
                                       uint32_t instance_count,
                                       uint32_t first_index,
                                       int32_t base_vertex,
                                       uint32_t first_instance,
                                       URGE_EXCEPTION) {
  object_.DrawIndexed(index_count, instance_count, first_index, base_vertex,
                      first_instance);
}

void GPURenderPassEncoder::DrawIndexedIndirect(
    scoped_refptr<GPUBuffer> indirect_buffer,
    uint64_t indirect_offset,
    URGE_EXCEPTION) {
  object_.DrawIndexedIndirect(WGPU_PTR(indirect_buffer), indirect_offset);
}

void GPURenderPassEncoder::DrawIndirect(
    scoped_refptr<GPUBuffer> indirect_buffer,
    uint64_t indirect_offset,
    URGE_EXCEPTION) {
  object_.DrawIndirect(WGPU_PTR(indirect_buffer), indirect_offset);
}

void GPURenderPassEncoder::PushDebugGroup(estring group_label, URGE_EXCEPTION) {
  object_.PushDebugGroup(group_label);
}

void GPURenderPassEncoder::InsertDebugMarker(estring marker_label,
                                             URGE_EXCEPTION) {
  object_.InsertDebugMarker(marker_label);
}

void GPURenderPassEncoder::PopDebugGroup(URGE_EXCEPTION) {
  object_.PopDebugGroup();
}

void GPURenderPassEncoder::SetBindGroup(uint32_t group_index,
                                        scoped_refptr<GPUBindGroup> group,
                                        earray<uint32_t> dynamic_offsets,
                                        URGE_EXCEPTION) {
  object_.SetBindGroup(group_index, WGPU_PTR(group), dynamic_offsets.size(),
                       dynamic_offsets.data());
}

void GPURenderPassEncoder::SetBlendConstant(estruct<GPUColor> color,
                                            URGE_EXCEPTION) {
  wgpu::Color raw_color;
  if (color)
    raw_color = {color->r, color->g, color->b, color->a};
  object_.SetBlendConstant(color ? &raw_color : nullptr);
}

void GPURenderPassEncoder::SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                                          GPU::IndexFormat format,
                                          uint64_t offset,
                                          uint64_t size,
                                          URGE_EXCEPTION) {
  object_.SetIndexBuffer(WGPU_PTR(buffer),
                         static_cast<wgpu::IndexFormat>(format), offset, size);
}

void GPURenderPassEncoder::SetPipeline(
    scoped_refptr<GPURenderPipeline> pipeline,
    URGE_EXCEPTION) {
  object_.SetPipeline(WGPU_PTR(pipeline));
}

void GPURenderPassEncoder::SetScissorRect(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height,
                                          URGE_EXCEPTION) {
  object_.SetScissorRect(x, y, width, height);
}

void GPURenderPassEncoder::SetStencilReference(uint32_t reference,
                                               URGE_EXCEPTION) {
  object_.SetStencilReference(reference);
}

void GPURenderPassEncoder::SetVertexBuffer(uint32_t slot,
                                           scoped_refptr<GPUBuffer> buffer,
                                           uint64_t offset,
                                           uint64_t size,
                                           URGE_EXCEPTION) {
  object_.SetVertexBuffer(slot, WGPU_PTR(buffer), offset, size);
}

void GPURenderPassEncoder::SetViewport(float x,
                                       float y,
                                       float width,
                                       float height,
                                       float min_depth,
                                       float max_depth,
                                       URGE_EXCEPTION) {
  object_.SetViewport(x, y, width, height, min_depth, max_depth);
}

void GPURenderPassEncoder::End(URGE_EXCEPTION) {
  object_.End();
}

///
/// GPU CommandEncoder
///

GPUCommandEncoder::GPUCommandEncoder(wgpu::CommandEncoder object)
    : object_(object) {}

void GPUCommandEncoder::SetLabel(estring label, URGE_EXCEPTION) {
  object_.SetLabel(label);
}

scoped_refptr<GPUComputePassEncoder> GPUCommandEncoder::BeginComputePass(
    estruct<GPUComputePassDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::PassTimestampWrites timestamp_desc;
  wgpu::ComputePassDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;

    if (descriptor->timestampWrites) {
      create_desc.timestampWrites = &timestamp_desc;
      timestamp_desc.querySet = WGPU_PTR(descriptor->timestampWrites->querySet);
      timestamp_desc.beginningOfPassWriteIndex =
          descriptor->timestampWrites->beginningOfPassWriteIndex;
      timestamp_desc.endOfPassWriteIndex =
          descriptor->timestampWrites->endOfPassWriteIndex;
    }
  }

  auto result = object_.BeginComputePass(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPUComputePassEncoder>(result);
}

scoped_refptr<GPURenderPassEncoder> GPUCommandEncoder::BeginRenderPass(
    estruct<GPURenderPassDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::PassTimestampWrites timestamp_desc;
  std::vector<wgpu::RenderPassColorAttachment> color_attachments;
  wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment;
  wgpu::RenderPassDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;

    for (auto& it : descriptor->colorAttachments) {
      wgpu::RenderPassColorAttachment attachment;
      attachment.view = WGPU_PTR(it->view);
      attachment.depthSlice = it->depthSlice;
      attachment.resolveTarget = WGPU_PTR(it->resolveTarget);
      attachment.loadOp = static_cast<wgpu::LoadOp>(it->loadOp);
      attachment.storeOp = static_cast<wgpu::StoreOp>(it->storeOp);
      if (auto color = it->clearValue; color)
        attachment.clearValue = {color->r, color->g, color->b, color->a};

      color_attachments.push_back(std::move(attachment));
    }

    if (descriptor->depthStencilAttachment) {
      depth_stencil_attachment.view =
          WGPU_PTR(descriptor->depthStencilAttachment->view);
      depth_stencil_attachment.depthLoadOp = static_cast<wgpu::LoadOp>(
          descriptor->depthStencilAttachment->depthLoadOp);
      depth_stencil_attachment.depthStoreOp = static_cast<wgpu::StoreOp>(
          descriptor->depthStencilAttachment->depthStoreOp);
      depth_stencil_attachment.depthClearValue =
          descriptor->depthStencilAttachment->depthClearValue;
      depth_stencil_attachment.depthReadOnly =
          descriptor->depthStencilAttachment->depthReadOnly;
      depth_stencil_attachment.stencilLoadOp = static_cast<wgpu::LoadOp>(
          descriptor->depthStencilAttachment->stencilLoadOp);
      depth_stencil_attachment.stencilStoreOp = static_cast<wgpu::StoreOp>(
          descriptor->depthStencilAttachment->stencilStoreOp);
      depth_stencil_attachment.stencilClearValue =
          descriptor->depthStencilAttachment->stencilClearValue;
      depth_stencil_attachment.stencilReadOnly =
          descriptor->depthStencilAttachment->stencilReadOnly;
    }

    create_desc.occlusionQuerySet = WGPU_PTR(descriptor->occlusionQuerySet);

    if (descriptor->timestampWrites) {
      create_desc.timestampWrites = &timestamp_desc;
      timestamp_desc.querySet = WGPU_PTR(descriptor->timestampWrites->querySet);
      timestamp_desc.beginningOfPassWriteIndex =
          descriptor->timestampWrites->beginningOfPassWriteIndex;
      timestamp_desc.endOfPassWriteIndex =
          descriptor->timestampWrites->endOfPassWriteIndex;
    }
  }

  auto result = object_.BeginRenderPass(&create_desc);
  if (!result)
    return nullptr;
  return Object::Create<GPURenderPassEncoder>(result);
}

void GPUCommandEncoder::ClearBuffer(scoped_refptr<GPUBuffer> buffer,
                                    uint64_t offset,
                                    uint64_t size,
                                    URGE_EXCEPTION) {
  object_.ClearBuffer(WGPU_PTR(buffer), offset, size);
}

void GPUCommandEncoder::CopyBufferToBuffer(scoped_refptr<GPUBuffer> source,
                                           uint64_t source_offset,
                                           scoped_refptr<GPUBuffer> destination,
                                           uint64_t destination_offset,
                                           uint64_t size,
                                           URGE_EXCEPTION) {
  object_.CopyBufferToBuffer(WGPU_PTR(source), source_offset,
                             WGPU_PTR(destination), destination_offset, size);
}

void GPUCommandEncoder::CopyBufferToTexture(
    estruct<GPUTexelCopyBufferInfo> source,
    estruct<GPUTexelCopyTextureInfo> destination,
    estruct<GPUExtent3D> copy_size,
    URGE_EXCEPTION) {
  wgpu::TexelCopyBufferInfo raw_source;
  if (auto data = source; data) {
    if (data->layout) {
      raw_source.layout.offset = data->layout->offset;
      raw_source.layout.bytesPerRow = data->layout->bytesPerRow;
      raw_source.layout.rowsPerImage = data->layout->rowsPerImage;
    }

    raw_source.buffer = WGPU_PTR(data->buffer);
  }

  wgpu::TexelCopyTextureInfo raw_destination;
  if (auto data = destination; data) {
    raw_destination.texture = WGPU_PTR(data->texture);
    raw_destination.mipLevel = data->mipLevel;
    if (data->origin)
      raw_destination.origin = {data->origin->x, data->origin->y,
                                data->origin->z};
    raw_destination.aspect = static_cast<wgpu::TextureAspect>(data->aspect);
  }

  wgpu::Extent3D raw_copy_size;
  if (copy_size)
    raw_copy_size = {copy_size->width, copy_size->height,
                     copy_size->depthOrArrayLayers};

  object_.CopyBufferToTexture(&raw_source, &raw_destination,
                              copy_size ? &raw_copy_size : nullptr);
}

void GPUCommandEncoder::CopyTextureToBuffer(
    estruct<GPUTexelCopyTextureInfo> source,
    estruct<GPUTexelCopyBufferInfo> destination,
    estruct<GPUExtent3D> copy_size,
    URGE_EXCEPTION) {
  wgpu::TexelCopyTextureInfo raw_source;
  if (auto data = source; data) {
    raw_source.texture = WGPU_PTR(data->texture);
    raw_source.mipLevel = data->mipLevel;
    if (data->origin)
      raw_source.origin = {data->origin->x, data->origin->y, data->origin->z};
    raw_source.aspect = static_cast<wgpu::TextureAspect>(data->aspect);
  }

  wgpu::TexelCopyBufferInfo raw_destination;
  if (auto data = destination; data) {
    if (data->layout) {
      raw_destination.layout.offset = data->layout->offset;
      raw_destination.layout.bytesPerRow = data->layout->bytesPerRow;
      raw_destination.layout.rowsPerImage = data->layout->rowsPerImage;
    }

    raw_destination.buffer = WGPU_PTR(data->buffer);
  }

  wgpu::Extent3D raw_copy_size;
  if (copy_size)
    raw_copy_size = {copy_size->width, copy_size->height,
                     copy_size->depthOrArrayLayers};

  object_.CopyTextureToBuffer(&raw_source, &raw_destination,
                              copy_size ? &raw_copy_size : nullptr);
}

void GPUCommandEncoder::CopyTextureToTexture(
    estruct<GPUTexelCopyTextureInfo> source,
    estruct<GPUTexelCopyTextureInfo> destination,
    estruct<GPUExtent3D> copy_size,
    URGE_EXCEPTION) {
  wgpu::TexelCopyTextureInfo raw_source;
  if (auto data = source; data) {
    raw_source.texture = WGPU_PTR(data->texture);
    raw_source.mipLevel = data->mipLevel;
    if (data->origin)
      raw_source.origin = {data->origin->x, data->origin->y, data->origin->z};
    raw_source.aspect = static_cast<wgpu::TextureAspect>(data->aspect);
  }

  wgpu::TexelCopyTextureInfo raw_destination;
  if (auto data = destination; data) {
    raw_destination.texture = WGPU_PTR(data->texture);
    raw_destination.mipLevel = data->mipLevel;
    if (data->origin)
      raw_destination.origin = {data->origin->x, data->origin->y,
                                data->origin->z};
    raw_destination.aspect = static_cast<wgpu::TextureAspect>(data->aspect);
  }

  wgpu::Extent3D raw_copy_size;
  if (copy_size)
    raw_copy_size = {copy_size->width, copy_size->height,
                     copy_size->depthOrArrayLayers};

  object_.CopyTextureToTexture(&raw_source, &raw_destination,
                               copy_size ? &raw_copy_size : nullptr);
}

void GPUCommandEncoder::PushDebugGroup(estring group_label, URGE_EXCEPTION) {
  object_.PushDebugGroup(group_label);
}

void GPUCommandEncoder::InsertDebugMarker(estring marker_label,
                                          URGE_EXCEPTION) {
  object_.InsertDebugMarker(marker_label);
}

void GPUCommandEncoder::PopDebugGroup(URGE_EXCEPTION) {
  object_.PopDebugGroup();
}

void GPUCommandEncoder::ResolveQuerySet(scoped_refptr<GPUQuerySet> query_set,
                                        uint32_t first_query,
                                        uint32_t query_count,
                                        scoped_refptr<GPUBuffer> destination,
                                        uint64_t destination_offset,
                                        URGE_EXCEPTION) {
  object_.ResolveQuerySet(WGPU_PTR(query_set), first_query, query_count,
                          WGPU_PTR(destination), destination_offset);
}

void GPUCommandEncoder::WriteTimestamp(scoped_refptr<GPUQuerySet> query_set,
                                       uint32_t query_index,
                                       URGE_EXCEPTION) {
  object_.WriteTimestamp(WGPU_PTR(query_set), query_index);
}

scoped_refptr<GPUCommandBuffer> GPUCommandEncoder::Finish(
    estruct<GPUCommandBufferDescriptor> descriptor,
    URGE_EXCEPTION) {
  wgpu::CommandBufferDescriptor create_desc;
  if (descriptor) {
    create_desc.label = descriptor->label;
  }

  auto result = object_.Finish(descriptor ? &create_desc : nullptr);
  if (!result)
    return nullptr;
  return Object::Create<GPUCommandBuffer>(result);
}

}  // namespace content

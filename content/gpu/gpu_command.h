// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/bind/callback.h"
#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"
#include "content/gpu/gpu_resource.h"

namespace content {

///
/// GPU ComputePassEncoder
///

URGE_BINDING()
class GPUComputePassEncoder : public Object {
 public:
  GPUComputePassEncoder(wgpu::ComputePassEncoder object);

  GPUComputePassEncoder(const GPUComputePassEncoder&) = delete;
  GPUComputePassEncoder& operator=(const GPUComputePassEncoder&) = delete;

  wgpu::ComputePassEncoder handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  void SetBindGroup(uint32_t group_index,
                    scoped_refptr<GPUBindGroup> group,
                    earray<uint32_t> dynamic_offsets,
                    URGE_EXCEPTION);

  URGE_BINDING()
  void SetImmediates(uint32_t offset,
                     epointer data,
                     size_t size,
                     URGE_EXCEPTION);

  URGE_BINDING()
  void SetPipeline(scoped_refptr<GPUComputePipeline> pipeline, URGE_EXCEPTION);

  URGE_BINDING()
  void DispatchWorkgroups(uint32_t workgroup_count_x,
                          uint32_t workgroup_count_y,
                          uint32_t workgroup_count_z,
                          URGE_EXCEPTION);

  URGE_BINDING()
  void DispatchWorkgroupsIndirect(scoped_refptr<GPUBuffer> indirect_buffer,
                                  uint64_t indirect_offset,
                                  URGE_EXCEPTION);

  URGE_BINDING()
  void PushDebugGroup(estring group_label, URGE_EXCEPTION);

  URGE_BINDING()
  void InsertDebugMarker(estring marker_label, URGE_EXCEPTION);

  URGE_BINDING()
  void PopDebugGroup(URGE_EXCEPTION);

  URGE_BINDING()
  void End(URGE_EXCEPTION);

 private:
  wgpu::ComputePassEncoder object_;
};

///
/// GPU RenderPassEncoder
///

URGE_BINDING()
class GPURenderPassEncoder : public Object {
 public:
  GPURenderPassEncoder(wgpu::RenderPassEncoder object);

  GPURenderPassEncoder(const GPURenderPassEncoder&) = delete;
  GPURenderPassEncoder& operator=(const GPURenderPassEncoder&) = delete;

  wgpu::RenderPassEncoder handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  void BeginOcclusionQuery(uint32_t query_index, URGE_EXCEPTION);

  URGE_BINDING()
  void EndOcclusionQuery(URGE_EXCEPTION);

  URGE_BINDING()
  void Draw(uint32_t vertex_count,
            uint32_t instance_count,
            uint32_t first_vertex,
            uint32_t first_instance,
            URGE_EXCEPTION);

  URGE_BINDING()
  void DrawIndexed(uint32_t index_count,
                   uint32_t instance_count,
                   uint32_t first_index,
                   int32_t base_vertex,
                   uint32_t first_instance,
                   URGE_EXCEPTION);

  URGE_BINDING()
  void DrawIndexedIndirect(scoped_refptr<GPUBuffer> indirect_buffer,
                           uint64_t indirect_offset,
                           URGE_EXCEPTION);

  URGE_BINDING()
  void DrawIndirect(scoped_refptr<GPUBuffer> indirect_buffer,
                    uint64_t indirect_offset,
                    URGE_EXCEPTION);

  URGE_BINDING()
  void PushDebugGroup(estring group_label, URGE_EXCEPTION);

  URGE_BINDING()
  void InsertDebugMarker(estring marker_label, URGE_EXCEPTION);

  URGE_BINDING()
  void PopDebugGroup(URGE_EXCEPTION);

  URGE_BINDING()
  void SetBindGroup(uint32_t group_index,
                    scoped_refptr<GPUBindGroup> group,
                    earray<uint32_t> dynamic_offsets,
                    URGE_EXCEPTION);

  URGE_BINDING()
  void SetBlendConstant(scoped_refptr<GPUColor> color, URGE_EXCEPTION);

  URGE_BINDING()
  void SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                      GPU::IndexFormat format,
                      uint64_t offset,
                      uint64_t size,
                      URGE_EXCEPTION);

  URGE_BINDING()
  void SetImmediates(uint32_t offset,
                     epointer data,
                     size_t size,
                     URGE_EXCEPTION);

  URGE_BINDING()
  void SetPipeline(scoped_refptr<GPURenderPipeline> pipeline, URGE_EXCEPTION);

  URGE_BINDING()
  void SetScissorRect(uint32_t x,
                      uint32_t y,
                      uint32_t width,
                      uint32_t height,
                      URGE_EXCEPTION);

  URGE_BINDING()
  void SetStencilReference(uint32_t reference, URGE_EXCEPTION);

  URGE_BINDING()
  void SetVertexBuffer(uint32_t slot,
                       scoped_refptr<GPUBuffer> buffer,
                       uint64_t offset,
                       uint64_t size,
                       URGE_EXCEPTION);

  URGE_BINDING()
  void SetViewport(float x,
                   float y,
                   float width,
                   float height,
                   float min_depth,
                   float max_depth,
                   URGE_EXCEPTION);

  URGE_BINDING()
  void End(URGE_EXCEPTION);

 private:
  wgpu::RenderPassEncoder object_;
};

///
/// GPU CommandEncoder
///

URGE_BINDING()
class GPUTexelCopyBufferLayout : public Object {
 public:
  URGE_BINDING()
  uint64_t offset = 0;

  URGE_BINDING()
  uint32_t bytesPerRow = WGPU_COPY_STRIDE_UNDEFINED;

  URGE_BINDING()
  uint32_t rowsPerImage = WGPU_COPY_STRIDE_UNDEFINED;
};

URGE_BINDING()
class GPUTexelCopyBufferInfo : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUTexelCopyBufferLayout> layout = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUBuffer> buffer = nullptr;
};

URGE_BINDING()
class GPUTexelCopyTextureInfo : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUTexture> texture = nullptr;

  URGE_BINDING()
  uint32_t mipLevel = 0;

  URGE_BINDING()
  scoped_refptr<GPUOrigin3D> origin = nullptr;

  URGE_BINDING()
  GPU::TextureAspect aspect = GPU::TextureAspect::Undefined;
};

URGE_BINDING()
class GPUPassTimestampWrites : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUQuerySet> querySet = nullptr;

  URGE_BINDING()
  uint32_t beginningOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED;

  URGE_BINDING()
  uint32_t endOfPassWriteIndex = WGPU_QUERY_SET_INDEX_UNDEFINED;
};

URGE_BINDING()
class GPUComputePassDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  scoped_refptr<GPUPassTimestampWrites> timestampWrites = nullptr;
};

URGE_BINDING()
class GPURenderPassColorAttachment : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUTextureView> view = nullptr;

  URGE_BINDING()
  uint32_t depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;

  URGE_BINDING()
  scoped_refptr<GPUTextureView> resolveTarget = nullptr;

  URGE_BINDING()
  GPU::LoadOp loadOp = GPU::LoadOp::Undefined;

  URGE_BINDING()
  GPU::StoreOp storeOp = GPU::StoreOp::Undefined;

  URGE_BINDING()
  scoped_refptr<GPUColor> clearValue = nullptr;
};

URGE_BINDING()
class GPURenderPassDepthStencilAttachment : public Object {
 public:
  URGE_BINDING()
  scoped_refptr<GPUTextureView> view = nullptr;

  URGE_BINDING()
  GPU::LoadOp depthLoadOp = GPU::LoadOp::Undefined;

  URGE_BINDING()
  GPU::StoreOp depthStoreOp = GPU::StoreOp::Undefined;

  URGE_BINDING()
  float depthClearValue = WGPU_DEPTH_CLEAR_VALUE_UNDEFINED;

  URGE_BINDING()
  bool depthReadOnly = false;

  URGE_BINDING()
  GPU::LoadOp stencilLoadOp = GPU::LoadOp::Undefined;

  URGE_BINDING()
  GPU::StoreOp stencilStoreOp = GPU::StoreOp::Undefined;

  URGE_BINDING()
  uint32_t stencilClearValue = 0;

  URGE_BINDING()
  bool stencilReadOnly = false;
};

URGE_BINDING()
class GPURenderPassDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};

  URGE_BINDING()
  earray<scoped_refptr<GPURenderPassColorAttachment>> colorAttachments = {};

  URGE_BINDING()
  scoped_refptr<GPURenderPassDepthStencilAttachment> depthStencilAttachment =
      nullptr;

  URGE_BINDING()
  scoped_refptr<GPUQuerySet> occlusionQuerySet = nullptr;

  URGE_BINDING()
  scoped_refptr<GPUPassTimestampWrites> timestampWrites = nullptr;
};

URGE_BINDING()
class GPUCommandBufferDescriptor : public Object {
 public:
  URGE_BINDING()
  estring label = {};
};

URGE_BINDING()
class GPUCommandEncoder : public Object {
 public:
  GPUCommandEncoder(wgpu::CommandEncoder object);

  GPUCommandEncoder(const GPUCommandEncoder&) = delete;
  GPUCommandEncoder& operator=(const GPUCommandEncoder&) = delete;

  wgpu::CommandEncoder handle() const { return object_; }

 public:
  URGE_BINDING()
  void SetLabel(estring label, URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUComputePassEncoder> BeginComputePass(
      scoped_refptr<GPUComputePassDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPURenderPassEncoder> BeginRenderPass(
      scoped_refptr<GPURenderPassDescriptor> descriptor,
      URGE_EXCEPTION);

  URGE_BINDING()
  void ClearBuffer(scoped_refptr<GPUBuffer> buffer,
                   uint64_t offset,
                   uint64_t size,
                   URGE_EXCEPTION);

  URGE_BINDING()
  void CopyBufferToBuffer(scoped_refptr<GPUBuffer> source,
                          uint64_t source_offset,
                          scoped_refptr<GPUBuffer> destination,
                          uint64_t destination_offset,
                          uint64_t size,
                          URGE_EXCEPTION);

  URGE_BINDING()
  void CopyBufferToTexture(scoped_refptr<GPUTexelCopyBufferInfo> source,
                           scoped_refptr<GPUTexelCopyTextureInfo> destination,
                           scoped_refptr<GPUExtent3D> copy_size,
                           URGE_EXCEPTION);

  URGE_BINDING()
  void CopyTextureToBuffer(scoped_refptr<GPUTexelCopyTextureInfo> source,
                           scoped_refptr<GPUTexelCopyBufferInfo> destination,
                           scoped_refptr<GPUExtent3D> copy_size,
                           URGE_EXCEPTION);

  URGE_BINDING()
  void CopyTextureToTexture(scoped_refptr<GPUTexelCopyTextureInfo> source,
                            scoped_refptr<GPUTexelCopyTextureInfo> destination,
                            scoped_refptr<GPUExtent3D> copy_size,
                            URGE_EXCEPTION);

  URGE_BINDING()
  void PushDebugGroup(estring group_label, URGE_EXCEPTION);

  URGE_BINDING()
  void InsertDebugMarker(estring marker_label, URGE_EXCEPTION);

  URGE_BINDING()
  void PopDebugGroup(URGE_EXCEPTION);

  URGE_BINDING()
  void ResolveQuerySet(scoped_refptr<GPUQuerySet> query_set,
                       uint32_t first_query,
                       uint32_t query_count,
                       scoped_refptr<GPUBuffer> destination,
                       uint64_t destination_offset,
                       URGE_EXCEPTION);

  URGE_BINDING()
  void WriteTimestamp(scoped_refptr<GPUQuerySet> query_set,
                      uint32_t query_index,
                      URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<GPUCommandBuffer> Finish(
      scoped_refptr<GPUCommandBufferDescriptor> descriptor,
      URGE_EXCEPTION);

 private:
  wgpu::CommandEncoder object_;
};

}  // namespace content

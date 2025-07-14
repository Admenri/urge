// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/device_context_impl.h"

#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"
#include "content/gpu/buffer_impl.h"
#include "content/gpu/command_list_impl.h"
#include "content/gpu/command_queue_impl.h"
#include "content/gpu/fence_impl.h"
#include "content/gpu/pipeline_state_impl.h"
#include "content/gpu/query_impl.h"
#include "content/gpu/resource_binding_impl.h"
#include "content/gpu/texture_impl.h"

namespace content {

DeviceContextImpl::DeviceContextImpl(ExecutionContext* context,
                                     Diligent::IDeviceContext* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(DeviceContextImpl);

scoped_refptr<GPUDeviceContextDesc> DeviceContextImpl::GetDesc(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  const auto& desc = object_->GetDesc();
  auto object_desc = base::MakeRefCounted<GPUDeviceContextDesc>();

  object_desc->name = desc.Name;
  object_desc->queue_type = static_cast<GPU::CommandQueueType>(desc.QueueType);
  object_desc->is_deferred = desc.IsDeferred;
  object_desc->context_id = desc.ContextId;
  object_desc->queue_id = desc.QueueId;
  object_desc->texture_copy_granularity.insert(
      object_desc->texture_copy_granularity.end(),
      std::begin(desc.TextureCopyGranularity),
      std::end(desc.TextureCopyGranularity));

  return object_desc;
}

void DeviceContextImpl::Begin(uint32_t immediate_context_id,
                              ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->Begin(immediate_context_id);
}

void DeviceContextImpl::SetPipelineState(
    scoped_refptr<GPUPipelineState> pipeline,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_pipeline = static_cast<PipelineStateImpl*>(pipeline.get());
  auto* object_pipeline = raw_pipeline ? raw_pipeline->AsRawPtr() : nullptr;

  object_->SetPipelineState(object_pipeline);
}

void DeviceContextImpl::TransitionShaderResources(
    scoped_refptr<GPUResourceBinding> binding,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_binding = static_cast<ResourceBindingImpl*>(binding.get());
  auto* object_binding = raw_binding ? raw_binding->AsRawPtr() : nullptr;

  object_->TransitionShaderResources(object_binding);
}

void DeviceContextImpl::CommitShaderResources(
    scoped_refptr<GPUResourceBinding> binding,
    GPU::ResourceStateTransitionMode mode,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_binding = static_cast<ResourceBindingImpl*>(binding.get());
  auto* object_binding = raw_binding ? raw_binding->AsRawPtr() : nullptr;

  object_->CommitShaderResources(
      object_binding,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode));
}

void DeviceContextImpl::SetStencilRef(uint32_t ref,
                                      ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->SetStencilRef(ref);
}

void DeviceContextImpl::SetBlendFactors(const base::Vector<float>& factors,
                                        ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->SetBlendFactors(factors.data());
}

void DeviceContextImpl::SetVertexBuffers(
    uint32_t start_slot,
    const base::Vector<scoped_refptr<GPUBuffer>>& buffers,
    const base::Vector<uint64_t>& offsets,
    GPU::ResourceStateTransitionMode mode,
    GPU::SetVertexBuffersFlags flags,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::IBuffer*> objects;
  for (auto& it : buffers) {
    auto* impl = static_cast<BufferImpl*>(it.get());
    objects.push_back(impl ? impl->AsRawPtr() : nullptr);
  }

  object_->SetVertexBuffers(
      start_slot, objects.size(), objects.data(), offsets.data(),
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode),
      static_cast<Diligent::SET_VERTEX_BUFFERS_FLAGS>(flags));
}

void DeviceContextImpl::InvalidateState(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->InvalidateState();
}

void DeviceContextImpl::SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                                       uint64_t byte_offset,
                                       GPU::ResourceStateTransitionMode mode,
                                       ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_buffer = static_cast<BufferImpl*>(buffer.get());
  auto* object_buffer = raw_buffer ? raw_buffer->AsRawPtr() : nullptr;

  object_->SetIndexBuffer(
      object_buffer, byte_offset,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode));
}

void DeviceContextImpl::SetViewports(
    const base::Vector<scoped_refptr<GPUViewport>>& viewports,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::Viewport> objects;
  for (auto& it : viewports) {
    Diligent::Viewport element;
    element.TopLeftX = it->top_left_x;
    element.TopLeftY = it->top_left_y;
    element.Width = it->width;
    element.Height = it->height;
    element.MinDepth = it->min_depth;
    element.MaxDepth = it->max_depth;
    objects.push_back(std::move(element));
  }

  object_->SetViewports(objects.size(), objects.data(), UINT32_MAX, UINT32_MAX);
}

void DeviceContextImpl::SetScissorRects(
    const base::Vector<scoped_refptr<Rect>>& rects,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::Rect> objects;
  for (auto& it : rects) {
    auto rect = RectImpl::From(it)->AsBaseRect();
    Diligent::Rect element;
    element.left = rect.x;
    element.top = rect.y;
    element.right = rect.x + rect.width;
    element.bottom = rect.y + rect.height;
    objects.push_back(std::move(element));
  }

  object_->SetScissorRects(objects.size(), objects.data(), UINT32_MAX,
                           UINT32_MAX);
}

void DeviceContextImpl::SetRenderTargets(
    const base::Vector<scoped_refptr<GPUTextureView>>& render_targets,
    scoped_refptr<GPUTextureView> depth_stencil,
    GPU::ResourceStateTransitionMode mode,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::ITextureView*> objects;
  for (auto& it : render_targets) {
    auto* impl = static_cast<TextureViewImpl*>(it.get());
    objects.push_back(impl ? impl->AsRawPtr() : nullptr);
  }

  auto* raw_depth_stencil = static_cast<TextureViewImpl*>(depth_stencil.get());
  auto* object_depth_stencil =
      raw_depth_stencil ? raw_depth_stencil->AsRawPtr() : nullptr;

  object_->SetRenderTargets(
      objects.size(), objects.data(), object_depth_stencil,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode));
}

void DeviceContextImpl::Draw(uint32_t num_vertices,
                             uint32_t num_instances,
                             uint32_t first_vertex,
                             uint32_t first_instance,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  Diligent::DrawAttribs attrib;
  attrib.NumVertices = num_vertices;
  attrib.NumInstances = num_instances;
  attrib.StartVertexLocation = first_vertex;
  attrib.FirstInstanceLocation = first_instance;
  object_->Draw(attrib);
}

void DeviceContextImpl::DrawIndexed(uint32_t num_indices,
                                    uint32_t num_instances,
                                    uint32_t first_index,
                                    uint32_t base_vertex,
                                    uint32_t first_instance,
                                    GPU::ValueType index_type,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  Diligent::DrawIndexedAttribs attrib;
  attrib.NumIndices = num_indices;
  attrib.NumInstances = num_instances;
  attrib.FirstIndexLocation = first_index;
  attrib.BaseVertex = base_vertex;
  attrib.FirstInstanceLocation = first_instance;
  attrib.IndexType = static_cast<Diligent::VALUE_TYPE>(index_type);
  object_->DrawIndexed(attrib);
}

void DeviceContextImpl::DrawIndirect(
    scoped_refptr<GPUBuffer> attribs_buffer,
    uint64_t draw_args_offset,
    uint32_t draw_count,
    uint32_t draw_args_stride,
    GPU::ResourceStateTransitionMode attribs_buffer_mode,
    scoped_refptr<GPUBuffer> counter_buffer,
    uint64_t counter_offset,
    GPU::ResourceStateTransitionMode counter_buffer_mode,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_attribs_buffer = static_cast<BufferImpl*>(attribs_buffer.get());
  auto* raw_counter_buffer = static_cast<BufferImpl*>(counter_buffer.get());

  Diligent::DrawIndirectAttribs attrib;
  attrib.pAttribsBuffer =
      raw_attribs_buffer ? raw_attribs_buffer->AsRawPtr() : nullptr;
  attrib.DrawArgsOffset = draw_args_offset;
  attrib.DrawCount = draw_count;
  attrib.DrawArgsStride = draw_args_stride;
  attrib.AttribsBufferStateTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          attribs_buffer_mode);
  attrib.pCounterBuffer =
      raw_counter_buffer ? raw_counter_buffer->AsRawPtr() : nullptr;
  attrib.CounterOffset = counter_offset;
  attrib.CounterBufferStateTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          counter_buffer_mode);
  object_->DrawIndirect(attrib);
}

void DeviceContextImpl::DrawIndexedIndirect(
    scoped_refptr<GPUBuffer> attribs_buffer,
    uint64_t draw_args_offset,
    uint32_t draw_count,
    uint32_t draw_args_stride,
    GPU::ResourceStateTransitionMode attribs_buffer_mode,
    scoped_refptr<GPUBuffer> counter_buffer,
    uint64_t counter_offset,
    GPU::ResourceStateTransitionMode counter_buffer_mode,
    GPU::ValueType index_type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_attribs_buffer = static_cast<BufferImpl*>(attribs_buffer.get());
  auto* raw_counter_buffer = static_cast<BufferImpl*>(counter_buffer.get());

  Diligent::DrawIndexedIndirectAttribs attrib;
  attrib.pAttribsBuffer =
      raw_attribs_buffer ? raw_attribs_buffer->AsRawPtr() : nullptr;
  attrib.DrawArgsOffset = draw_args_offset;
  attrib.DrawCount = draw_count;
  attrib.DrawArgsStride = draw_args_stride;
  attrib.AttribsBufferStateTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          attribs_buffer_mode);
  attrib.pCounterBuffer =
      raw_counter_buffer ? raw_counter_buffer->AsRawPtr() : nullptr;
  attrib.CounterOffset = counter_offset;
  attrib.CounterBufferStateTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          counter_buffer_mode);
  attrib.IndexType = static_cast<Diligent::VALUE_TYPE>(index_type);
  object_->DrawIndexedIndirect(attrib);
}

void DeviceContextImpl::MultiDraw(
    const base::Vector<scoped_refptr<GPUMultiDrawItem>>& items,
    uint32_t num_instances,
    uint32_t first_instance,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::MultiDrawItem> object_items;
  for (auto& item : items) {
    Diligent::MultiDrawItem result;
    result.NumVertices = item->num_vertices;
    result.StartVertexLocation = item->start_vertex;
    object_items.push_back(std::move(result));
  }

  Diligent::MultiDrawAttribs attrib;
  attrib.DrawCount = object_items.size();
  attrib.pDrawItems = object_items.data();
  attrib.NumInstances = num_instances;
  attrib.FirstInstanceLocation = first_instance;
  object_->MultiDraw(attrib);
}

void DeviceContextImpl::MultiDrawIndexed(
    const base::Vector<scoped_refptr<GPUMultiDrawIndexedItem>>& items,
    GPU::ValueType index_type,
    uint32_t num_instances,
    uint32_t first_instance,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::MultiDrawIndexedItem> object_items;
  for (auto& item : items) {
    Diligent::MultiDrawIndexedItem result;
    result.NumIndices = item->num_indices;
    result.FirstIndexLocation = item->start_index;
    result.BaseVertex = item->base_vertex;
    object_items.push_back(std::move(result));
  }

  Diligent::MultiDrawIndexedAttribs attrib;
  attrib.DrawCount = object_items.size();
  attrib.pDrawItems = object_items.data();
  attrib.IndexType = static_cast<Diligent::VALUE_TYPE>(index_type);
  attrib.NumInstances = num_instances;
  attrib.FirstInstanceLocation = first_instance;
  object_->MultiDrawIndexed(attrib);
}

void DeviceContextImpl::DispatchCompute(uint32_t thread_group_count_x,
                                        uint32_t thread_group_count_y,
                                        uint32_t thread_group_count_z,
                                        ExceptionState& exception_state) {
  DISPOSE_CHECK;

  Diligent::DispatchComputeAttribs attrib;
  attrib.ThreadGroupCountX = thread_group_count_x;
  attrib.ThreadGroupCountY = thread_group_count_y;
  attrib.ThreadGroupCountZ = thread_group_count_z;
  object_->DispatchCompute(attrib);
}

void DeviceContextImpl::DispatchComputeIndirect(
    scoped_refptr<GPUBuffer> attribs_buffer,
    GPU::ResourceStateTransitionMode attribs_buffer_mode,
    uint64_t dispatch_args_byte_offset,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_attribs_buffer = static_cast<BufferImpl*>(attribs_buffer.get());

  Diligent::DispatchComputeIndirectAttribs attrib;
  attrib.pAttribsBuffer =
      raw_attribs_buffer ? raw_attribs_buffer->AsRawPtr() : nullptr;
  attrib.AttribsBufferStateTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(
          attribs_buffer_mode);
  attrib.DispatchArgsByteOffset = dispatch_args_byte_offset;
  object_->DispatchComputeIndirect(attrib);
}

void DeviceContextImpl::ClearDepthStencil(
    scoped_refptr<GPUTextureView> view,
    GPU::ClearDepthStencilFlags clear_flags,
    float depth,
    uint32_t stencil,
    GPU::ResourceStateTransitionMode mode,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_view = static_cast<TextureViewImpl*>(view.get());
  auto* object_view = raw_view ? raw_view->AsRawPtr() : nullptr;

  object_->ClearDepthStencil(
      object_view,
      static_cast<Diligent::CLEAR_DEPTH_STENCIL_FLAGS>(clear_flags), depth,
      stencil, static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode));
}

void DeviceContextImpl::ClearRenderTarget(scoped_refptr<GPUTextureView> view,
                                          scoped_refptr<Color> color,
                                          GPU::ResourceStateTransitionMode mode,
                                          ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_view = static_cast<TextureViewImpl*>(view.get());
  auto* object_view = raw_view ? raw_view->AsRawPtr() : nullptr;

  auto color_impl = ColorImpl::From(color);
  base::Vec4 clear_color =
      color_impl ? color_impl->AsNormColor() : base::Vec4();

  object_->ClearRenderTarget(
      object_view, &clear_color,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode));
}

scoped_refptr<GPUCommandList> DeviceContextImpl::FinishCommandList(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::RefCntAutoPtr<Diligent::ICommandList> result;
  object_->FinishCommandList(&result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<CommandListImpl>(context(), result);
}

void DeviceContextImpl::ExecuteCommandLists(
    const base::Vector<scoped_refptr<GPUCommandList>>& command_lists,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::ICommandList*> objects;
  for (auto& it : command_lists) {
    auto* impl = static_cast<CommandListImpl*>(it.get());
    objects.push_back(impl ? impl->AsRawPtr() : nullptr);
  }

  object_->ExecuteCommandLists(objects.size(), objects.data());
}

void DeviceContextImpl::EnqueueSignal(scoped_refptr<GPUFence> fence,
                                      uint64_t value,
                                      ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_fence = static_cast<FenceImpl*>(fence.get());
  auto* object_fence = raw_fence ? raw_fence->AsRawPtr() : nullptr;

  object_->EnqueueSignal(object_fence, value);
}

void DeviceContextImpl::DeviceWaitForFence(scoped_refptr<GPUFence> fence,
                                           uint64_t value,
                                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_fence = static_cast<FenceImpl*>(fence.get());
  auto* object_fence = raw_fence ? raw_fence->AsRawPtr() : nullptr;

  object_->DeviceWaitForFence(object_fence, value);
}

void DeviceContextImpl::WaitForIdle(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->WaitForIdle();
}

void DeviceContextImpl::BeginQuery(scoped_refptr<GPUQuery> query,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_query = static_cast<QueryImpl*>(query.get());
  auto* object_query = raw_query ? raw_query->AsRawPtr() : nullptr;

  object_->BeginQuery(object_query);
}

void DeviceContextImpl::EndQuery(scoped_refptr<GPUQuery> query,
                                 ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_query = static_cast<QueryImpl*>(query.get());
  auto* object_query = raw_query ? raw_query->AsRawPtr() : nullptr;

  object_->EndQuery(object_query);
}

void DeviceContextImpl::Flush(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->Flush();
}

void DeviceContextImpl::UpdateBuffer(scoped_refptr<GPUBuffer> buffer,
                                     uint64_t offset,
                                     const void* data,
                                     uint64_t size,
                                     GPU::ResourceStateTransitionMode mode,
                                     ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_buffer = static_cast<BufferImpl*>(buffer.get());
  auto* object_buffer = raw_buffer ? raw_buffer->AsRawPtr() : nullptr;

  object_->UpdateBuffer(
      object_buffer, offset, size, data,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(mode));
}

void DeviceContextImpl::CopyBuffer(scoped_refptr<GPUBuffer> src_buffer,
                                   uint64_t src_offset,
                                   GPU::ResourceStateTransitionMode src_mode,
                                   scoped_refptr<GPUBuffer> dst_buffer,
                                   uint64_t dst_offset,
                                   GPU::ResourceStateTransitionMode dst_mode,
                                   uint64_t copy_size,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_src_buffer = static_cast<BufferImpl*>(src_buffer.get());
  auto* object_src_buffer =
      raw_src_buffer ? raw_src_buffer->AsRawPtr() : nullptr;
  auto* raw_dst_buffer = static_cast<BufferImpl*>(dst_buffer.get());
  auto* object_dst_buffer =
      raw_dst_buffer ? raw_dst_buffer->AsRawPtr() : nullptr;
  object_->CopyBuffer(
      object_src_buffer, src_offset,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(src_mode),
      object_dst_buffer, dst_offset, copy_size,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(dst_mode));
}

uint64_t DeviceContextImpl::MapBuffer(scoped_refptr<GPUBuffer> buffer,
                                      GPU::MapType map_type,
                                      GPU::MapFlags map_flags,
                                      ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  auto* raw_buffer = static_cast<BufferImpl*>(buffer.get());
  auto* object_buffer = raw_buffer ? raw_buffer->AsRawPtr() : nullptr;

  void* mapping_buffer;
  object_->MapBuffer(object_buffer, static_cast<Diligent::MAP_TYPE>(map_type),
                     static_cast<Diligent::MAP_FLAGS>(map_flags),
                     mapping_buffer);

  return reinterpret_cast<uint64_t>(mapping_buffer);
}

void DeviceContextImpl::UnmapBuffer(scoped_refptr<GPUBuffer> buffer,
                                    GPU::MapType map_type,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_buffer = static_cast<BufferImpl*>(buffer.get());
  auto* object_buffer = raw_buffer ? raw_buffer->AsRawPtr() : nullptr;

  object_->UnmapBuffer(object_buffer,
                       static_cast<Diligent::MAP_TYPE>(map_type));
}

void DeviceContextImpl::UpdateTexture(
    scoped_refptr<GPUTexture> texture,
    uint32_t mip_level,
    uint32_t slice,
    scoped_refptr<GPUBox> box,
    scoped_refptr<GPUTextureSubResData> data,
    GPU::ResourceStateTransitionMode src_buffer_mode,
    GPU::ResourceStateTransitionMode texture_mode,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_texture = static_cast<TextureImpl*>(texture.get());
  auto* object_texture = raw_texture ? raw_texture->AsRawPtr() : nullptr;

  Diligent::Box object_box;
  if (box) {
    object_box.MinX = box->min_x;
    object_box.MaxX = box->max_x;
    object_box.MinY = box->min_y;
    object_box.MaxY = box->max_y;
    object_box.MinZ = box->min_z;
    object_box.MaxZ = box->max_z;
  }

  Diligent::TextureSubResData sub_res;
  if (data) {
    auto* raw_buffer = static_cast<BufferImpl*>(data->src_buffer.get());

    sub_res.pData = reinterpret_cast<void*>(data->data_ptr);
    sub_res.pSrcBuffer = raw_buffer ? raw_buffer->AsRawPtr() : nullptr;
    sub_res.SrcOffset = data->src_offset;
    sub_res.Stride = data->stride;
    sub_res.DepthStride = data->depth_stride;
  }

  object_->UpdateTexture(
      object_texture, mip_level, slice, object_box, sub_res,
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(src_buffer_mode),
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(texture_mode));
}

void DeviceContextImpl::CopyTexture(scoped_refptr<GPUTexture> src_texture,
                                    uint32_t src_mip_level,
                                    uint32_t src_slice,
                                    scoped_refptr<GPUBox> src_box,
                                    GPU::ResourceStateTransitionMode src_mode,
                                    scoped_refptr<GPUTexture> dst_texture,
                                    uint32_t dst_mip_level,
                                    uint32_t dst_slice,
                                    uint32_t dst_x,
                                    uint32_t dst_y,
                                    uint32_t dst_z,
                                    GPU::ResourceStateTransitionMode dst_mode,
                                    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_src_texture = static_cast<TextureImpl*>(src_texture.get());
  auto* raw_dst_texture = static_cast<TextureImpl*>(dst_texture.get());

  Diligent::Box object_src_box;
  if (src_box) {
    object_src_box.MinX = src_box->min_x;
    object_src_box.MaxX = src_box->max_x;
    object_src_box.MinY = src_box->min_y;
    object_src_box.MaxY = src_box->max_y;
    object_src_box.MinZ = src_box->min_z;
    object_src_box.MaxZ = src_box->max_z;
  }

  Diligent::CopyTextureAttribs attrib;
  attrib.pSrcTexture = raw_src_texture ? raw_src_texture->AsRawPtr() : nullptr;
  attrib.SrcMipLevel = src_mip_level;
  attrib.SrcSlice = src_slice;
  attrib.pSrcBox = &object_src_box;
  attrib.SrcTextureTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(src_mode);
  attrib.pDstTexture = raw_dst_texture ? raw_dst_texture->AsRawPtr() : nullptr;
  attrib.DstMipLevel = dst_mip_level;
  attrib.DstSlice = dst_slice;
  attrib.DstX = dst_x;
  attrib.DstY = dst_y;
  attrib.DstZ = dst_z;
  attrib.DstTextureTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(dst_mode);

  object_->CopyTexture(attrib);
}

scoped_refptr<GPUMappedTextureSubresource>
DeviceContextImpl::MapTextureSubresource(scoped_refptr<GPUTexture> texture,
                                         uint32_t mip_level,
                                         uint32_t array_slice,
                                         GPU::MapType map_type,
                                         GPU::MapFlags map_flags,
                                         scoped_refptr<GPUBox> map_region,
                                         ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* raw_texture = static_cast<TextureImpl*>(texture.get());
  auto* object_texture = raw_texture ? raw_texture->AsRawPtr() : nullptr;

  Diligent::Box object_box;
  if (map_region) {
    object_box.MinX = map_region->min_x;
    object_box.MaxX = map_region->max_x;
    object_box.MinY = map_region->min_y;
    object_box.MaxY = map_region->max_y;
    object_box.MinZ = map_region->min_z;
    object_box.MaxZ = map_region->max_z;
  }

  Diligent::MappedTextureSubresource mapped;
  object_->MapTextureSubresource(object_texture, mip_level, array_slice,
                                 static_cast<Diligent::MAP_TYPE>(map_type),
                                 static_cast<Diligent::MAP_FLAGS>(map_flags),
                                 &object_box, mapped);

  auto result = base::MakeRefCounted<GPUMappedTextureSubresource>();
  result->data_ptr = reinterpret_cast<uint64_t>(mapped.pData);
  result->stride = mapped.Stride;
  result->depth_stride = mapped.DepthStride;

  return result;
}

void DeviceContextImpl::UnmapTextureSubresource(
    scoped_refptr<GPUTexture> texture,
    uint32_t mip_level,
    uint32_t array_slice,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_texture = static_cast<TextureImpl*>(texture.get());
  auto* object_texture = raw_texture ? raw_texture->AsRawPtr() : nullptr;

  object_->UnmapTextureSubresource(object_texture, mip_level, array_slice);
}

void DeviceContextImpl::GenerateMips(scoped_refptr<GPUTextureView> view,
                                     ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_view = static_cast<TextureViewImpl*>(view.get());
  auto* object_view = raw_view ? raw_view->AsRawPtr() : nullptr;

  object_->GenerateMips(object_view);
}

void DeviceContextImpl::FinishFrame(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->FinishFrame();
}

void DeviceContextImpl::TransitionResourceStates(
    const base::Vector<scoped_refptr<GPUStateTransitionDesc>>& barriers,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::StateTransitionDesc> objects;
  for (auto& it : barriers) {
    Diligent::StateTransitionDesc desc;
    desc.pResourceBefore =
        reinterpret_cast<Diligent::IDeviceObject*>(it->resource_before_ptr);
    desc.pResource =
        reinterpret_cast<Diligent::IDeviceObject*>(it->resource_ptr);
    desc.FirstMipLevel = it->first_mip_level;
    desc.MipLevelsCount = it->mip_levels_count;
    desc.FirstArraySlice = it->first_array_slice;
    desc.ArraySliceCount = it->array_slice_count;
    desc.OldState = static_cast<Diligent::RESOURCE_STATE>(it->old_state);
    desc.NewState = static_cast<Diligent::RESOURCE_STATE>(it->new_state);
    desc.TransitionType =
        static_cast<Diligent::STATE_TRANSITION_TYPE>(it->transition_type);
    desc.Flags =
        static_cast<Diligent::STATE_TRANSITION_FLAGS>(it->transition_flags);

    objects.push_back(std::move(desc));
  }

  object_->TransitionResourceStates(objects.size(), objects.data());
}

void DeviceContextImpl::ResolveTextureSubresource(
    scoped_refptr<GPUTexture> src,
    scoped_refptr<GPUTexture> dst,
    uint32_t src_mip_level,
    uint32_t src_slice,
    GPU::ResourceStateTransitionMode src_mode,
    uint32_t dst_mip_level,
    uint32_t dst_slice,
    GPU::ResourceStateTransitionMode dst_mode,
    GPU::TextureFormat format,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_src_texture = static_cast<TextureImpl*>(src.get());
  auto* object_src_texture =
      raw_src_texture ? raw_src_texture->AsRawPtr() : nullptr;
  auto* raw_dst_texture = static_cast<TextureImpl*>(dst.get());
  auto* object_dst_texture =
      raw_dst_texture ? raw_dst_texture->AsRawPtr() : nullptr;

  Diligent::ResolveTextureSubresourceAttribs attrib;
  attrib.SrcMipLevel = src_mip_level;
  attrib.SrcSlice = src_slice;
  attrib.SrcTextureTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(src_mode);
  attrib.DstMipLevel = dst_mip_level;
  attrib.DstSlice = dst_slice;
  attrib.DstTextureTransitionMode =
      static_cast<Diligent::RESOURCE_STATE_TRANSITION_MODE>(dst_mode);
  attrib.Format = static_cast<Diligent::TEXTURE_FORMAT>(format);
  object_->ResolveTextureSubresource(object_src_texture, object_dst_texture,
                                     attrib);
}

void DeviceContextImpl::BeginDebugGroup(const base::String& name,
                                        scoped_refptr<Color> color,
                                        ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto debug_color = ColorImpl::From(color);
  base::Vec4 norm_color =
      debug_color ? debug_color->AsNormColor() : base::Vec4();

  object_->BeginDebugGroup(name.c_str(), reinterpret_cast<float*>(&norm_color));
}

void DeviceContextImpl::EndDebugGroup(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->EndDebugGroup();
}

void DeviceContextImpl::InsertDebugGroup(const base::String& name,
                                         scoped_refptr<Color> color,
                                         ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto debug_color = ColorImpl::From(color);
  base::Vec4 norm_color =
      debug_color ? debug_color->AsNormColor() : base::Vec4();

  object_->InsertDebugLabel(name.c_str(),
                            reinterpret_cast<float*>(&norm_color));
}

scoped_refptr<GPUCommandQueue> DeviceContextImpl::LockCommandQueue(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->LockCommandQueue();
  if (!result)
    return nullptr;

  return base::MakeRefCounted<CommandQueueImpl>(context(), result);
}

void DeviceContextImpl::UnlockCommandQueue(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->UnlockCommandQueue();
}

void DeviceContextImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

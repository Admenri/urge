// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUDEVICECONTEXT_H_
#define CONTENT_PUBLIC_ENGINE_GPUDEVICECONTEXT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_color.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpucommandlist.h"
#include "content/public/engine_gpucommandqueue.h"
#include "content/public/engine_gpufence.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuquery.h"
#include "content/public/engine_gpuresourcebinding.h"
#include "content/public/engine_gputexture.h"
#include "content/public/engine_rect.h"

namespace content {

/*--urge(name:GPUDeviceContext)--*/
class URGE_OBJECT(GPUDeviceContext) {
 public:
  virtual ~GPUDeviceContext() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:desc)--*/
  virtual scoped_refptr<GPUDeviceContextDesc> GetDesc(
      ExceptionState& exception_state) = 0;

  /*--urge(name:begin)--*/
  virtual void Begin(uint32_t immediate_context_id,
                     ExceptionState& exception_state) = 0;

  /*--urge(name:set_pipeline_state)--*/
  virtual void SetPipelineState(scoped_refptr<GPUPipelineState> pipeline,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:transition_shader_resources)--*/
  virtual void TransitionShaderResources(
      scoped_refptr<GPUResourceBinding> binding,
      ExceptionState& exception_state) = 0;

  /*--urge(name:commit_shader_resources)--*/
  virtual void CommitShaderResources(scoped_refptr<GPUResourceBinding> binding,
                                     GPU::ResourceStateTransitionMode mode,
                                     ExceptionState& exception_state) = 0;

  /*--urge(name:set_stencil_ref)--*/
  virtual void SetStencilRef(uint32_t ref, ExceptionState& exception_state) = 0;

  /*--urge(name:set_blend_factors)--*/
  virtual void SetBlendFactors(const std::vector<float>& factors,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:set_vertex_buffers)--*/
  virtual void SetVertexBuffers(
      uint32_t start_slot,
      const std::vector<scoped_refptr<GPUBuffer>>& buffers,
      const std::vector<uint64_t>& offsets,
      GPU::ResourceStateTransitionMode mode,
      GPU::SetVertexBuffersFlags flags,
      ExceptionState& exception_state) = 0;

  /*--urge(name:invalidate_state)--*/
  virtual void InvalidateState(ExceptionState& exception_state) = 0;

  /*--urge(name:set_index_buffer)--*/
  virtual void SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                              uint64_t byte_offset,
                              GPU::ResourceStateTransitionMode mode,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:set_viewports)--*/
  virtual void SetViewports(
      const std::vector<scoped_refptr<GPUViewport>>& viewports,
      ExceptionState& exception_state) = 0;

  /*--urge(name:set_scissor_rects)--*/
  virtual void SetScissorRects(const std::vector<scoped_refptr<Rect>>& rects,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:set_render_targets)--*/
  virtual void SetRenderTargets(
      const std::vector<scoped_refptr<GPUTextureView>>& render_targets,
      scoped_refptr<GPUTextureView> depth_stencil,
      GPU::ResourceStateTransitionMode mode,
      ExceptionState& exception_state) = 0;

  /*--urge(name:draw)--*/
  virtual void Draw(uint32_t num_vertices,
                    uint32_t num_instances,
                    uint32_t first_vertex,
                    uint32_t first_instance,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:draw_indexed)--*/
  virtual void DrawIndexed(uint32_t num_indices,
                           uint32_t num_instances,
                           uint32_t first_index,
                           uint32_t base_vertex,
                           uint32_t first_instance,
                           GPU::ValueType index_type,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:draw_indirect)--*/
  virtual void DrawIndirect(
      scoped_refptr<GPUBuffer> attribs_buffer,
      uint64_t draw_args_offset,
      uint32_t draw_count,
      uint32_t draw_args_stride,
      GPU::ResourceStateTransitionMode attribs_buffer_mode,
      scoped_refptr<GPUBuffer> counter_buffer,
      uint64_t counter_offset,
      GPU::ResourceStateTransitionMode counter_buffer_mode,
      ExceptionState& exception_state) = 0;

  /*--urge(name:draw_indexed_indirect)--*/
  virtual void DrawIndexedIndirect(
      scoped_refptr<GPUBuffer> attribs_buffer,
      uint64_t draw_args_offset,
      uint32_t draw_count,
      uint32_t draw_args_stride,
      GPU::ResourceStateTransitionMode attribs_buffer_mode,
      scoped_refptr<GPUBuffer> counter_buffer,
      uint64_t counter_offset,
      GPU::ResourceStateTransitionMode counter_buffer_mode,
      GPU::ValueType index_type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:multi_draw)--*/
  virtual void MultiDraw(
      const std::vector<scoped_refptr<GPUMultiDrawItem>>& items,
      uint32_t num_instances,
      uint32_t first_instance,
      ExceptionState& exception_state) = 0;

  /*--urge(name:multi_draw_indexed)--*/
  virtual void MultiDrawIndexed(
      const std::vector<scoped_refptr<GPUMultiDrawIndexedItem>>& items,
      GPU::ValueType index_type,
      uint32_t num_instances,
      uint32_t first_instance,
      ExceptionState& exception_state) = 0;

  /*--urge(name:dispatch_compute)--*/
  virtual void DispatchCompute(uint32_t thread_group_count_x,
                               uint32_t thread_group_count_y,
                               uint32_t thread_group_count_z,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:dispatch_compute_indirect)--*/
  virtual void DispatchComputeIndirect(
      scoped_refptr<GPUBuffer> attribs_buffer,
      GPU::ResourceStateTransitionMode attribs_buffer_mode,
      uint64_t dispatch_args_byte_offset,
      ExceptionState& exception_state) = 0;

  /*--urge(name:clear_depth_stencil)--*/
  virtual void ClearDepthStencil(scoped_refptr<GPUTextureView> view,
                                 GPU::ClearDepthStencilFlags clear_flags,
                                 float depth,
                                 uint32_t stencil,
                                 GPU::ResourceStateTransitionMode mode,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:clear_render_target)--*/
  virtual void ClearRenderTarget(scoped_refptr<GPUTextureView> view,
                                 scoped_refptr<Color> color,
                                 GPU::ResourceStateTransitionMode mode,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:finish_command_list)--*/
  virtual scoped_refptr<GPUCommandList> FinishCommandList(
      ExceptionState& exception_state) = 0;

  /*--urge(name:execute_command_lists)--*/
  virtual void ExecuteCommandLists(
      const std::vector<scoped_refptr<GPUCommandList>>& command_lists,
      ExceptionState& exception_state) = 0;

  /*--urge(name:enqueue_signal)--*/
  virtual void EnqueueSignal(scoped_refptr<GPUFence> fence,
                             uint64_t value,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:device_wait_for_fence)--*/
  virtual void DeviceWaitForFence(scoped_refptr<GPUFence> fence,
                                  uint64_t value,
                                  ExceptionState& exception_state) = 0;

  /*--urge(name:wait_for_idle)--*/
  virtual void WaitForIdle(ExceptionState& exception_state) = 0;

  /*--urge(name:begin_query)--*/
  virtual void BeginQuery(scoped_refptr<GPUQuery> query,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:end_query)--*/
  virtual void EndQuery(scoped_refptr<GPUQuery> query,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:flush)--*/
  virtual void Flush(ExceptionState& exception_state) = 0;

  /*--urge(name:update_buffer)--*/
  virtual void UpdateBuffer(scoped_refptr<GPUBuffer> buffer,
                            uint64_t offset,
                            const void* data,
                            uint64_t size,
                            GPU::ResourceStateTransitionMode mode,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:copy_buffer)--*/
  virtual void CopyBuffer(scoped_refptr<GPUBuffer> src_buffer,
                          uint64_t src_offset,
                          GPU::ResourceStateTransitionMode src_mode,
                          scoped_refptr<GPUBuffer> dst_buffer,
                          uint64_t dst_offset,
                          GPU::ResourceStateTransitionMode dst_mode,
                          uint64_t copy_size,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:map_buffer)--*/
  virtual uint64_t MapBuffer(scoped_refptr<GPUBuffer> buffer,
                             GPU::MapType map_type,
                             GPU::MapFlags map_flags,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:unmap_buffer)--*/
  virtual void UnmapBuffer(scoped_refptr<GPUBuffer> buffer,
                           GPU::MapType map_type,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:update_texture)--*/
  virtual void UpdateTexture(scoped_refptr<GPUTexture> texture,
                             uint32_t mip_level,
                             uint32_t slice,
                             scoped_refptr<GPUBox> box,
                             scoped_refptr<GPUTextureSubResData> data,
                             GPU::ResourceStateTransitionMode src_buffer_mode,
                             GPU::ResourceStateTransitionMode texture_mode,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:copy_texture)--*/
  virtual void CopyTexture(scoped_refptr<GPUTexture> src_texture,
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
                           ExceptionState& exception_state) = 0;

  /*--urge(name:map_texture_subresource)--*/
  virtual scoped_refptr<GPUMappedTextureSubresource> MapTextureSubresource(
      scoped_refptr<GPUTexture> texture,
      uint32_t mip_level,
      uint32_t array_slice,
      GPU::MapType map_type,
      GPU::MapFlags map_flags,
      scoped_refptr<GPUBox> map_region,
      ExceptionState& exception_state) = 0;

  /*--urge(name:unmap_texture_subresource)--*/
  virtual void UnmapTextureSubresource(scoped_refptr<GPUTexture> texture,
                                       uint32_t mip_level,
                                       uint32_t array_slice,
                                       ExceptionState& exception_state) = 0;

  /*--urge(name:generate_mips)--*/
  virtual void GenerateMips(scoped_refptr<GPUTextureView> view,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:finish_frame)--*/
  virtual void FinishFrame(ExceptionState& exception_state) = 0;

  /*--urge(name:transition_resource_states)--*/
  virtual void TransitionResourceStates(
      const std::vector<scoped_refptr<GPUStateTransitionDesc>>& barriers,
      ExceptionState& exception_state) = 0;

  /*--urge(name:resolve_texture_subresource)--*/
  virtual void ResolveTextureSubresource(
      scoped_refptr<GPUTexture> src,
      scoped_refptr<GPUTexture> dst,
      uint32_t src_mip_level,
      uint32_t src_slice,
      GPU::ResourceStateTransitionMode src_mode,
      uint32_t dst_mip_level,
      uint32_t dst_slice,
      GPU::ResourceStateTransitionMode dst_mode,
      GPU::TextureFormat format,
      ExceptionState& exception_state) = 0;

  /*--urge(name:begin_debug_group)--*/
  virtual void BeginDebugGroup(const std::string& name,
                               scoped_refptr<Color> color,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:end_debug_group)--*/
  virtual void EndDebugGroup(ExceptionState& exception_state) = 0;

  /*--urge(name:insert_debug_group)--*/
  virtual void InsertDebugGroup(const std::string& name,
                                scoped_refptr<Color> color,
                                ExceptionState& exception_state) = 0;

  /*--urge(name:lock_command_queue)--*/
  virtual scoped_refptr<GPUCommandQueue> LockCommandQueue(
      ExceptionState& exception_state) = 0;

  /*--urge(name:unlock_command_queue)--*/
  virtual void UnlockCommandQueue(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUDEVICECONTEXT_H_

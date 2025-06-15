// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_DEVICE_CONTEXT_IMPL_H_
#define CONTENT_GPU_DEVICE_CONTEXT_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_color.h"
#include "content/public/engine_gpudevicecontext.h"
#include "content/public/engine_rect.h"

namespace content {

class DeviceContextImpl : public GPUDeviceContext,
                          public EngineObject,
                          public Disposable {
 public:
  DeviceContextImpl(ExecutionContext* context,
                    Diligent::IDeviceContext* object);
  ~DeviceContextImpl() override;

  DeviceContextImpl(const DeviceContextImpl&) = delete;
  DeviceContextImpl& operator=(const DeviceContextImpl&) = delete;

  Diligent::IDeviceContext* AsRawPtr() const { return object_; }

 protected:
  // Disposable interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;

  // GPUDeviceContext interface
  std::optional<DeviceContextDesc> GetDesc(
      ExceptionState& exception_state) override;
  void Begin(uint32_t immediate_context_id,
             ExceptionState& exception_state) override;
  void SetPipelineState(scoped_refptr<GPUPipelineState> pipeline,
                        ExceptionState& exception_state) override;
  void TransitionShaderResources(scoped_refptr<GPUResourceBinding> binding,
                                 ExceptionState& exception_state) override;
  void CommitShaderResources(scoped_refptr<GPUResourceBinding> binding,
                             GPU::ResourceStateTransitionMode mode,
                             ExceptionState& exception_state) override;
  void SetStencilRef(uint32_t ref, ExceptionState& exception_state) override;
  void SetBlendFactors(const base::Vector<float>& factors,
                       ExceptionState& exception_state) override;
  void SetVertexBuffers(uint32_t start_slot,
                        const base::Vector<scoped_refptr<GPUBuffer>>& buffers,
                        const base::Vector<uint64_t>& offsets,
                        GPU::ResourceStateTransitionMode mode,
                        GPU::SetVertexBuffersFlags flags,
                        ExceptionState& exception_state) override;
  void InvalidateState(ExceptionState& exception_state) override;
  void SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                      uint64_t byte_offset,
                      GPU::ResourceStateTransitionMode mode,
                      ExceptionState& exception_state) override;
  void SetViewports(const base::Vector<ClipViewport>& viewports,
                    ExceptionState& exception_state) override;
  void SetScissorRects(const base::Vector<scoped_refptr<Rect>>& rects,
                       ExceptionState& exception_state) override;
  void SetRenderTargets(
      const base::Vector<scoped_refptr<GPUTextureView>>& render_targets,
      scoped_refptr<GPUTextureView> depth_stencil,
      GPU::ResourceStateTransitionMode mode,
      ExceptionState& exception_state) override;
  void Draw(uint32_t num_vertices,
            uint32_t num_instances,
            uint32_t first_vertex,
            uint32_t first_instance,
            ExceptionState& exception_state) override;
  void DrawIndexed(uint32_t num_indices,
                   uint32_t num_instances,
                   uint32_t first_index,
                   uint32_t base_vertex,
                   uint32_t first_instance,
                   GPU::ValueType index_type,
                   ExceptionState& exception_state) override;
  void DrawIndirect(scoped_refptr<GPUBuffer> attribs_buffer,
                    uint64_t draw_args_offset,
                    uint32_t draw_count,
                    uint32_t draw_args_stride,
                    GPU::ResourceStateTransitionMode attribs_buffer_mode,
                    scoped_refptr<GPUBuffer> counter_buffer,
                    uint64_t counter_offset,
                    GPU::ResourceStateTransitionMode counter_buffer_mode,
                    ExceptionState& exception_state) override;
  void DrawIndexedIndirect(scoped_refptr<GPUBuffer> attribs_buffer,
                           uint64_t draw_args_offset,
                           uint32_t draw_count,
                           uint32_t draw_args_stride,
                           GPU::ResourceStateTransitionMode attribs_buffer_mode,
                           scoped_refptr<GPUBuffer> counter_buffer,
                           uint64_t counter_offset,
                           GPU::ResourceStateTransitionMode counter_buffer_mode,
                           GPU::ValueType index_type,
                           ExceptionState& exception_state) override;
  void DispatchCompute(uint32_t thread_group_count_x,
                       uint32_t thread_group_count_y,
                       uint32_t thread_group_count_z,
                       ExceptionState& exception_state) override;
  void DispatchComputeIndirect(
      scoped_refptr<GPUBuffer> attribs_buffer,
      GPU::ResourceStateTransitionMode attribs_buffer_mode,
      uint64_t dispatch_args_byte_offset,
      ExceptionState& exception_state) override;
  void ClearDepthStencil(scoped_refptr<GPUTextureView> view,
                         GPU::ClearDepthStencilFlags clear_flags,
                         float depth,
                         uint32_t stencil,
                         GPU::ResourceStateTransitionMode mode,
                         ExceptionState& exception_state) override;
  void ClearRenderTarget(scoped_refptr<GPUTextureView> view,
                         scoped_refptr<Color> color,
                         GPU::ResourceStateTransitionMode mode,
                         ExceptionState& exception_state) override;
  scoped_refptr<GPUCommandList> FinishCommandList(
      ExceptionState& exception_state) override;
  void ExecuteCommandLists(
      const base::Vector<scoped_refptr<GPUCommandList>>& command_lists,
      ExceptionState& exception_state) override;
  void EnqueueSignal(scoped_refptr<GPUFence> fence,
                     uint64_t value,
                     ExceptionState& exception_state) override;
  void DeviceWaitForFence(scoped_refptr<GPUFence> fence,
                          uint64_t value,
                          ExceptionState& exception_state) override;
  void WaitForIdle(ExceptionState& exception_state) override;
  void BeginQuery(scoped_refptr<GPUQuery> query,
                  ExceptionState& exception_state) override;
  void EndQuery(scoped_refptr<GPUQuery> query,
                ExceptionState& exception_state) override;
  void Flush(ExceptionState& exception_state) override;
  void UpdateBuffer(scoped_refptr<GPUBuffer> buffer,
                    uint64_t offset,
                    const void* data,
                    uint64_t size,
                    GPU::ResourceStateTransitionMode mode,
                    ExceptionState& exception_state) override;
  void CopyBuffer(scoped_refptr<GPUBuffer> src_buffer,
                  uint64_t src_offset,
                  GPU::ResourceStateTransitionMode src_mode,
                  scoped_refptr<GPUBuffer> dst_buffer,
                  uint64_t dst_offset,
                  GPU::ResourceStateTransitionMode dst_mode,
                  uint64_t copy_size,
                  ExceptionState& exception_state) override;
  uint64_t MapBuffer(scoped_refptr<GPUBuffer> buffer,
                     GPU::MapType map_type,
                     GPU::MapFlags map_flags,
                     ExceptionState& exception_state) override;
  void UnmapBuffer(scoped_refptr<GPUBuffer> buffer,
                   GPU::MapType map_type,
                   ExceptionState& exception_state) override;
  void UpdateTexture(scoped_refptr<GPUTexture> texture,
                     uint32_t mip_level,
                     uint32_t slice,
                     const std::optional<ClipBox>& box,
                     const std::optional<TextureSubResData>& data,
                     GPU::ResourceStateTransitionMode src_buffer_mode,
                     GPU::ResourceStateTransitionMode texture_mode,
                     ExceptionState& exception_state) override;
  void CopyTexture(scoped_refptr<GPUTexture> src_texture,
                   uint32_t src_mip_level,
                   uint32_t src_slice,
                   const std::optional<ClipBox>& src_box,
                   GPU::ResourceStateTransitionMode src_mode,
                   scoped_refptr<GPUTexture> dst_texture,
                   uint32_t dst_mip_level,
                   uint32_t dst_slice,
                   uint32_t dst_x,
                   uint32_t dst_y,
                   uint32_t dst_z,
                   GPU::ResourceStateTransitionMode dst_mode,
                   ExceptionState& exception_state) override;
  std::optional<MappedTextureSubresource> MapTextureSubresource(
      scoped_refptr<GPUTexture> texture,
      uint32_t mip_level,
      uint32_t array_slice,
      GPU::MapType map_type,
      GPU::MapFlags map_flags,
      const std::optional<ClipBox>& map_region,
      ExceptionState& exception_state) override;
  void UnmapTextureSubresource(scoped_refptr<GPUTexture> texture,
                               uint32_t mip_level,
                               uint32_t array_slice,
                               ExceptionState& exception_state) override;
  void GenerateMips(scoped_refptr<GPUTextureView> view,
                    ExceptionState& exception_state) override;
  void FinishFrame(ExceptionState& exception_state) override;
  void TransitionResourceStates(
      const base::Vector<StateTransitionDesc>& barriers,
      ExceptionState& exception_state) override;
  void ResolveTextureSubresource(scoped_refptr<GPUTexture> src,
                                 scoped_refptr<GPUTexture> dst,
                                 uint32_t src_mip_level,
                                 uint32_t src_slice,
                                 GPU::ResourceStateTransitionMode src_mode,
                                 uint32_t dst_mip_level,
                                 uint32_t dst_slice,
                                 GPU::ResourceStateTransitionMode dst_mode,
                                 GPU::TextureFormat format,
                                 ExceptionState& exception_state) override;
  void BeginDebugGroup(const base::String& name,
                       scoped_refptr<Color> color,
                       ExceptionState& exception_state) override;
  void EndDebugGroup(ExceptionState& exception_state) override;
  void InsertDebugGroup(const base::String& name,
                        scoped_refptr<Color> color,
                        ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "GPU.DeviceContext"; }

  Diligent::RefCntAutoPtr<Diligent::IDeviceContext> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_DEVICE_CONTEXT_IMPL_H_

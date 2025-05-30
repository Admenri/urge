// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUDEVICECONTEXT_H_
#define CONTENT_PUBLIC_ENGINE_GPUDEVICECONTEXT_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"
#include "content/public/engine_gpubox.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpucommandlist.h"
#include "content/public/engine_gpufence.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuquery.h"
#include "content/public/engine_gpuresourcebinding.h"
#include "content/public/engine_gputexture.h"
#include "content/public/engine_gputexturesubresdata.h"
#include "content/public/engine_gputextureview.h"
#include "content/public/engine_gpuviewport.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:GPUDeviceContext)--*/
class URGE_RUNTIME_API GPUDeviceContext
    : public base::RefCounted<GPUDeviceContext> {
 public:
  virtual ~GPUDeviceContext() = default;

  /*--urge(name:ResourceStateTransitionMode)--*/
  enum ResourceStateTransitionMode {
    RESOURCE_STATE_TRANSITION_MODE_NONE = 0,
    RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
    RESOURCE_STATE_TRANSITION_MODE_VERIFY,
  };

  /*--urge(name:ValueType)--*/
  enum ValueType {
    VT_UNDEFINED = 0,
    VT_INT8,
    VT_INT16,
    VT_INT32,
    VT_UINT8,
    VT_UINT16,
    VT_UINT32,
    VT_FLOAT16,
    VT_FLOAT32,
    VT_FLOAT64,
  };

  /*--urge(name:ClearDepthStencilFlags)--*/
  enum ClearDepthStencilFlags {
    CLEAR_DEPTH_FLAG_NONE = 0x00,
    CLEAR_DEPTH_FLAG = 0x01,
    CLEAR_STENCIL_FLAG = 0x02,
  };

  /*--urge(name:TextureFormat)--*/
  enum TextureFormat {
    TEX_FORMAT_UNKNOWN = 0,
    TEX_FORMAT_RGBA32_TYPELESS,
    TEX_FORMAT_RGBA32_FLOAT,
    TEX_FORMAT_RGBA32_UINT,
    TEX_FORMAT_RGBA32_SINT,
    TEX_FORMAT_RGB32_TYPELESS,
    TEX_FORMAT_RGB32_FLOAT,
    TEX_FORMAT_RGB32_UINT,
    TEX_FORMAT_RGB32_SINT,
    TEX_FORMAT_RGBA16_TYPELESS,
    TEX_FORMAT_RGBA16_FLOAT,
    TEX_FORMAT_RGBA16_UNORM,
    TEX_FORMAT_RGBA16_UINT,
    TEX_FORMAT_RGBA16_SNORM,
    TEX_FORMAT_RGBA16_SINT,
    TEX_FORMAT_RG32_TYPELESS,
    TEX_FORMAT_RG32_FLOAT,
    TEX_FORMAT_RG32_UINT,
    TEX_FORMAT_RG32_SINT,
    TEX_FORMAT_R32G8X24_TYPELESS,
    TEX_FORMAT_D32_FLOAT_S8X24_UINT,
    TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    TEX_FORMAT_X32_TYPELESS_G8X24_UINT,
    TEX_FORMAT_RGB10A2_TYPELESS,
    TEX_FORMAT_RGB10A2_UNORM,
    TEX_FORMAT_RGB10A2_UINT,
    TEX_FORMAT_R11G11B10_FLOAT,
    TEX_FORMAT_RGBA8_TYPELESS,
    TEX_FORMAT_RGBA8_UNORM,
    TEX_FORMAT_RGBA8_UNORM_SRGB,
    TEX_FORMAT_RGBA8_UINT,
    TEX_FORMAT_RGBA8_SNORM,
    TEX_FORMAT_RGBA8_SINT,
    TEX_FORMAT_RG16_TYPELESS,
    TEX_FORMAT_RG16_FLOAT,
    TEX_FORMAT_RG16_UNORM,
    TEX_FORMAT_RG16_UINT,
    TEX_FORMAT_RG16_SNORM,
    TEX_FORMAT_RG16_SINT,
    TEX_FORMAT_R32_TYPELESS,
    TEX_FORMAT_D32_FLOAT,
    TEX_FORMAT_R32_FLOAT,
    TEX_FORMAT_R32_UINT,
    TEX_FORMAT_R32_SINT,
    TEX_FORMAT_R24G8_TYPELESS,
    TEX_FORMAT_D24_UNORM_S8_UINT,
    TEX_FORMAT_R24_UNORM_X8_TYPELESS,
    TEX_FORMAT_X24_TYPELESS_G8_UINT,
    TEX_FORMAT_RG8_TYPELESS,
    TEX_FORMAT_RG8_UNORM,
    TEX_FORMAT_RG8_UINT,
    TEX_FORMAT_RG8_SNORM,
    TEX_FORMAT_RG8_SINT,
    TEX_FORMAT_R16_TYPELESS,
    TEX_FORMAT_R16_FLOAT,
    TEX_FORMAT_D16_UNORM,
    TEX_FORMAT_R16_UNORM,
    TEX_FORMAT_R16_UINT,
    TEX_FORMAT_R16_SNORM,
    TEX_FORMAT_R16_SINT,
    TEX_FORMAT_R8_TYPELESS,
    TEX_FORMAT_R8_UNORM,
    TEX_FORMAT_R8_UINT,
    TEX_FORMAT_R8_SNORM,
    TEX_FORMAT_R8_SINT,
    TEX_FORMAT_A8_UNORM,
    TEX_FORMAT_R1_UNORM,
    TEX_FORMAT_RGB9E5_SHAREDEXP,
    TEX_FORMAT_RG8_B8G8_UNORM,
    TEX_FORMAT_G8R8_G8B8_UNORM,
    TEX_FORMAT_BC1_TYPELESS,
    TEX_FORMAT_BC1_UNORM,
    TEX_FORMAT_BC1_UNORM_SRGB,
    TEX_FORMAT_BC2_TYPELESS,
    TEX_FORMAT_BC2_UNORM,
    TEX_FORMAT_BC2_UNORM_SRGB,
    TEX_FORMAT_BC3_TYPELESS,
    TEX_FORMAT_BC3_UNORM,
    TEX_FORMAT_BC3_UNORM_SRGB,
    TEX_FORMAT_BC4_TYPELESS,
    TEX_FORMAT_BC4_UNORM,
    TEX_FORMAT_BC4_SNORM,
    TEX_FORMAT_BC5_TYPELESS,
    TEX_FORMAT_BC5_UNORM,
    TEX_FORMAT_BC5_SNORM,
    TEX_FORMAT_B5G6R5_UNORM,
    TEX_FORMAT_B5G5R5A1_UNORM,
    TEX_FORMAT_BGRA8_UNORM,
    TEX_FORMAT_BGRX8_UNORM,
    TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    TEX_FORMAT_BGRA8_TYPELESS,
    TEX_FORMAT_BGRA8_UNORM_SRGB,
    TEX_FORMAT_BGRX8_TYPELESS,
    TEX_FORMAT_BGRX8_UNORM_SRGB,
    TEX_FORMAT_BC6H_TYPELESS,
    TEX_FORMAT_BC6H_UF16,
    TEX_FORMAT_BC6H_SF16,
    TEX_FORMAT_BC7_TYPELESS,
    TEX_FORMAT_BC7_UNORM,
    TEX_FORMAT_BC7_UNORM_SRGB,
    TEX_FORMAT_ETC2_RGB8_UNORM,
    TEX_FORMAT_ETC2_RGB8_UNORM_SRGB,
    TEX_FORMAT_ETC2_RGB8A1_UNORM,
    TEX_FORMAT_ETC2_RGB8A1_UNORM_SRGB,
    TEX_FORMAT_ETC2_RGBA8_UNORM,
    TEX_FORMAT_ETC2_RGBA8_UNORM_SRGB,
    TEX_FORMAT_NUM_FORMATS,
  };

  /*--urge(name:submit)--*/
  virtual void Submit(ExceptionState& exception_state) = 0;

  /*--urge(name:synchronize)--*/
  virtual void Synchronize(ExceptionState& exception_state) = 0;

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
                                     ResourceStateTransitionMode mode,
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
      ResourceStateTransitionMode mode,
      ExceptionState& exception_state) = 0;

  /*--urge(name:invalidate_state)--*/
  virtual void InvalidateState(ExceptionState& exception_state) = 0;

  /*--urge(name:set_index_buffer)--*/
  virtual void SetIndexBuffer(scoped_refptr<GPUBuffer> buffer,
                              uint64_t byte_offset,
                              ResourceStateTransitionMode mode,
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
                           ValueType index_type,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:draw_indirect)--*/
  virtual void DrawIndirect(scoped_refptr<GPUBuffer> attribs_buffer,
                            uint64_t draw_args_offset,
                            uint32_t draw_count,
                            uint32_t draw_args_stride,
                            ResourceStateTransitionMode attribs_buffer_mode,
                            scoped_refptr<GPUBuffer> counter_buffer,
                            uint64_t counter_offset,
                            ResourceStateTransitionMode counter_buffer_mode,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:draw_indexed_indirect)--*/
  virtual void DrawIndexedIndirect(
      scoped_refptr<GPUBuffer> attribs_buffer,
      uint64_t draw_args_offset,
      uint32_t draw_count,
      uint32_t draw_args_stride,
      ResourceStateTransitionMode attribs_buffer_mode,
      scoped_refptr<GPUBuffer> counter_buffer,
      uint64_t counter_offset,
      ResourceStateTransitionMode counter_buffer_mode,
      ValueType index_type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:dispatch_compute)--*/
  virtual void DispatchCompute(uint32_t thread_group_count_x,
                               uint32_t thread_group_count_y,
                               uint32_t thread_group_count_z,
                               ExceptionState& exception_state) = 0;

  /*--urge(name:dispatch_compute_indirect)--*/
  virtual void DispatchComputeIndirect(
      scoped_refptr<GPUBuffer> attribs_buffer,
      ResourceStateTransitionMode attribs_buffer_mode,
      uint64_t dispatch_args_byte_offset,
      ExceptionState& exception_state) = 0;

  /*--urge(name:clear_depth_stencil)--*/
  virtual void ClearDepthStencil(scoped_refptr<GPUTextureView> view,
                                 ClearDepthStencilFlags clear_flags,
                                 float depth,
                                 uint32_t stencil,
                                 ExceptionState& exception_state) = 0;

  /*--urge(name:clear_render_target)--*/
  virtual void ClearRenderTarget(scoped_refptr<GPUTextureView> view,
                                 scoped_refptr<Color> color,
                                 ResourceStateTransitionMode mode,
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
                            const std::string& data,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:copy_buffer)--*/
  virtual void CopyBuffer(scoped_refptr<GPUBuffer> src_buffer,
                          uint64_t src_offset,
                          ResourceStateTransitionMode src_mode,
                          scoped_refptr<GPUBuffer> dst_buffer,
                          uint64_t dst_offset,
                          ResourceStateTransitionMode dst_mode,
                          uint64_t copy_size,
                          ExceptionState& exception_state) = 0;

  /*--urge(name:update_texture)--*/
  virtual void UpdateTexture(scoped_refptr<GPUTexture> texture,
                             uint32_t mip_level,
                             uint32_t slice,
                             scoped_refptr<GPUBox> box,
                             scoped_refptr<GPUTextureSubResData> data,
                             ResourceStateTransitionMode src_buffer_mode,
                             ResourceStateTransitionMode texture_mode,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:copy_texture)--*/
  virtual void CopyTexture(scoped_refptr<GPUTexture> src_texture,
                           uint32_t src_mip_level,
                           uint32_t src_slice,
                           scoped_refptr<GPUBox> src_box,
                           ResourceStateTransitionMode src_mode,
                           scoped_refptr<GPUTexture> dst_texture,
                           uint32_t dst_mip_level,
                           uint32_t dst_slice,
                           uint32_t dst_x,
                           uint32_t dst_y,
                           uint32_t dst_z,
                           ResourceStateTransitionMode dst_mode,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:generate_mips)--*/
  virtual void GenerateMips(scoped_refptr<GPUTextureView> view,
                            ExceptionState& exception_state) = 0;

  /*--urge(name:finish_frame)--*/
  virtual void FinishFrame(ExceptionState& exception_state) = 0;

  /*--urge(name:resolve_texture_subresource)--*/
  virtual void ResolveTextureSubresource(scoped_refptr<GPUTexture> src,
                                         scoped_refptr<GPUTexture> dst,
                                         uint32_t src_mip_level,
                                         uint32_t src_slice,
                                         ResourceStateTransitionMode src_mode,
                                         uint32_t dst_mip_level,
                                         uint32_t dst_slice,
                                         ResourceStateTransitionMode dst_mode,
                                         TextureFormat format,
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
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUDEVICECONTEXT_H_

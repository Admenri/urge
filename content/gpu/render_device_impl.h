// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_RENDER_DEVICE_IMPL_H_
#define CONTENT_GPU_RENDER_DEVICE_IMPL_H_

#include "content/public/engine_gpurenderdevice.h"
#include "renderer/device/render_device.h"

namespace content {

class RenderDeviceImpl : public GPURenderDevice {
 public:
  RenderDeviceImpl(Diligent::IRenderDevice* host);
  ~RenderDeviceImpl() override;

  RenderDeviceImpl(const RenderDeviceImpl&) = delete;
  RenderDeviceImpl& operator=(const RenderDeviceImpl&) = delete;

 protected:
  scoped_refptr<GPUBuffer> CreateBuffer(
      const std::optional<BufferDesc>& desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUBuffer> CreateBuffer(
      const std::optional<BufferDesc>& desc,
      const base::String& data,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUShader> CreateShader(
      const std::optional<ShaderCreateInfo>& create_info,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUTexture> CreateTexture(
      const std::optional<TextureDesc>& desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUTexture> CreateTexture(
      const std::optional<TextureDesc>& desc,
      const base::Vector<TextureSubResData>& subresources,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUSampler> CreateSampler(
      const std::optional<SamplerDesc>& desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineState> CreateGraphicsPipelineState(
      const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      const std::optional<GraphicsPipelineDesc>& graphics_pipeline_desc,
      scoped_refptr<GPUShader> vertex_shader,
      scoped_refptr<GPUShader> pixel_shader,
      scoped_refptr<GPUShader> domain_shader,
      scoped_refptr<GPUShader> hull_shader,
      scoped_refptr<GPUShader> geometry_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineState> CreateComputePipelineState(
      const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      scoped_refptr<GPUShader> compute_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUFence> CreateFence(GPU::FenceType type,
                                      ExceptionState& exception_state) override;
  scoped_refptr<GPUQuery> CreateQuery(GPU::QueryType type,
                                      ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineSignature> CreatePipelineSignature(
      const base::Vector<PipelineResourceDesc>& resources,
      const base::Vector<ImmutableSamplerDesc>& samplers,
      uint8_t binding_index,
      bool use_combined_texture_samplers,
      const base::String& combined_sampler_suffix,
      ExceptionState& exception_state) override;

 private:
  RRefPtr<Diligent::IRenderDevice> device_;
};

}  // namespace content

#endif  //! CONTENT_GPU_RENDER_DEVICE_H_

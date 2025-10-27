// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_RENDER_DEVICE_IMPL_H_
#define CONTENT_GPU_RENDER_DEVICE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpurenderdevice.h"

namespace content {

class RenderDeviceImpl : public GPURenderDevice,
                         public EngineObject,
                         public Disposable {
 public:
  RenderDeviceImpl(ExecutionContext* context, Diligent::IRenderDevice* object);
  ~RenderDeviceImpl() override;

  RenderDeviceImpl(const RenderDeviceImpl&) = delete;
  RenderDeviceImpl& operator=(const RenderDeviceImpl&) = delete;

  Diligent::IRenderDevice* AsRawPtr() const { return object_; }

 protected:
  // GPURenderDevice interface
  URGE_DECLARE_DISPOSABLE;
  scoped_refptr<GPURenderDeviceInfo> GetDeviceInfo(
      ExceptionState& exception_state) override;
  scoped_refptr<GPUGraphicsAdapterInfo> GetAdapterInfo(
      ExceptionState& exception_state) override;
  scoped_refptr<GPUBuffer> CreateBuffer(
      scoped_refptr<GPUBufferDesc> desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUBuffer> CreateBuffer(
      scoped_refptr<GPUBufferDesc> desc,
      scoped_refptr<GPUBufferData> data,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUShader> CreateShader(
      scoped_refptr<GPUShaderCreateInfo> create_info,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUTexture> CreateTexture(
      scoped_refptr<GPUTextureDesc> desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUTexture> CreateTexture(
      scoped_refptr<GPUTextureDesc> desc,
      scoped_refptr<GPUTextureData> data,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUSampler> CreateSampler(
      scoped_refptr<GPUSamplerDesc> desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUResourceMapping> CreateResourceMapping(
      const std::vector<scoped_refptr<GPUResourceMappingEntry>>& entries,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineState> CreateGraphicsPipelineState(
      const std::vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      scoped_refptr<GPUGraphicsPipelineDesc> graphics_pipeline_desc,
      scoped_refptr<GPUShader> vertex_shader,
      scoped_refptr<GPUShader> pixel_shader,
      scoped_refptr<GPUShader> domain_shader,
      scoped_refptr<GPUShader> hull_shader,
      scoped_refptr<GPUShader> geometry_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineState> CreateComputePipelineState(
      const std::vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      scoped_refptr<GPUShader> compute_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUFence> CreateFence(GPU::FenceType type,
                                      ExceptionState& exception_state) override;
  scoped_refptr<GPUQuery> CreateQuery(GPU::QueryType type,
                                      ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineSignature> CreatePipelineSignature(
      scoped_refptr<GPUPipelineSignatureDesc> desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUDeviceContext> CreateDeferredContext(
      ExceptionState& exception_state) override;
  void IdleGPU(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.RenderDevice"; }

  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_RENDER_DEVICE_IMPL_H_

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
#define CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpudevicecontext.h"
#include "content/public/engine_gpufence.h"
#include "content/public/engine_gpupipelinesignature.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuquery.h"
#include "content/public/engine_gpuresourcemapping.h"
#include "content/public/engine_gpusampler.h"
#include "content/public/engine_gpushader.h"
#include "content/public/engine_gputexture.h"

namespace content {

/*--urge(name:GPURenderDevice)--*/
class URGE_OBJECT(GPURenderDevice) {
 public:
  virtual ~GPURenderDevice() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:device_info)--*/
  virtual scoped_refptr<GPURenderDeviceInfo> GetDeviceInfo(
      ExceptionState& exception_state) = 0;

  /*--urge(name:adapter_info)--*/
  virtual scoped_refptr<GPUGraphicsAdapterInfo> GetAdapterInfo(
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      scoped_refptr<GPUBufferDesc> desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      scoped_refptr<GPUBufferDesc> desc,
      scoped_refptr<GPUBufferData> data,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_shader)--*/
  virtual scoped_refptr<GPUShader> CreateShader(
      scoped_refptr<GPUShaderCreateInfo> create_info,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_texture)--*/
  virtual scoped_refptr<GPUTexture> CreateTexture(
      scoped_refptr<GPUTextureDesc> desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_texture)--*/
  virtual scoped_refptr<GPUTexture> CreateTexture(
      scoped_refptr<GPUTextureDesc> desc,
      scoped_refptr<GPUTextureData> data,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_sampler)--*/
  virtual scoped_refptr<GPUSampler> CreateSampler(
      scoped_refptr<GPUSamplerDesc> desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_resource_mapping)--*/
  virtual scoped_refptr<GPUResourceMapping> CreateResourceMapping(
      const std::vector<scoped_refptr<GPUResourceMappingEntry>>& entries,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_graphics_pipeline_state)--*/
  virtual scoped_refptr<GPUPipelineState> CreateGraphicsPipelineState(
      const std::vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      scoped_refptr<GPUGraphicsPipelineDesc> graphics_pipeline_desc,
      scoped_refptr<GPUShader> vertex_shader,
      scoped_refptr<GPUShader> pixel_shader,
      scoped_refptr<GPUShader> domain_shader,
      scoped_refptr<GPUShader> hull_shader,
      scoped_refptr<GPUShader> geometry_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_compute_pipeline_state)--*/
  virtual scoped_refptr<GPUPipelineState> CreateComputePipelineState(
      const std::vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      scoped_refptr<GPUShader> compute_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_fence)--*/
  virtual scoped_refptr<GPUFence> CreateFence(
      GPU::FenceType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_query)--*/
  virtual scoped_refptr<GPUQuery> CreateQuery(
      GPU::QueryType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_pipeline_signature)--*/
  virtual scoped_refptr<GPUPipelineSignature> CreatePipelineSignature(
      scoped_refptr<GPUPipelineSignatureDesc> desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_deferred_context)--*/
  virtual scoped_refptr<GPUDeviceContext> CreateDeferredContext(
      ExceptionState& exception_state) = 0;

  /*--urge(name:idle_gpu)--*/
  virtual void IdleGPU(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/render_device_impl.h"

#include "content/context/execution_context.h"

namespace content {

RenderDeviceImpl::RenderDeviceImpl(
    ExecutionContext* context,
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

RenderDeviceImpl::~RenderDeviceImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void RenderDeviceImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool RenderDeviceImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

scoped_refptr<GPUBuffer> RenderDeviceImpl::CreateBuffer(
    const std::optional<BufferDesc>& desc,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUBuffer>();
}

scoped_refptr<GPUBuffer> RenderDeviceImpl::CreateBuffer(
    const std::optional<BufferDesc>& desc,
    const base::String& data,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUBuffer>();
}

scoped_refptr<GPUShader> RenderDeviceImpl::CreateShader(
    const std::optional<ShaderCreateInfo>& create_info,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUShader>();
}

scoped_refptr<GPUTexture> RenderDeviceImpl::CreateTexture(
    const std::optional<TextureDesc>& desc,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUTexture>();
}

scoped_refptr<GPUTexture> RenderDeviceImpl::CreateTexture(
    const std::optional<TextureDesc>& desc,
    const base::Vector<TextureSubResData>& subresources,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUTexture>();
}

scoped_refptr<GPUSampler> RenderDeviceImpl::CreateSampler(
    const std::optional<SamplerDesc>& desc,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUSampler>();
}

scoped_refptr<GPUResourceMapping> RenderDeviceImpl::CreateResourceMapping(
    const base::Vector<ResourceMappingEntry>& entries,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUResourceMapping>();
}

scoped_refptr<GPUPipelineState> RenderDeviceImpl::CreateGraphicsPipelineState(
    const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
    const std::optional<GraphicsPipelineDesc>& graphics_pipeline_desc,
    scoped_refptr<GPUShader> vertex_shader,
    scoped_refptr<GPUShader> pixel_shader,
    scoped_refptr<GPUShader> domain_shader,
    scoped_refptr<GPUShader> hull_shader,
    scoped_refptr<GPUShader> geometry_shader,
    uint64_t immediate_context_mask,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUPipelineState>();
}

scoped_refptr<GPUPipelineState> RenderDeviceImpl::CreateComputePipelineState(
    const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
    scoped_refptr<GPUShader> compute_shader,
    uint64_t immediate_context_mask,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUPipelineState>();
}

scoped_refptr<GPUFence> RenderDeviceImpl::CreateFence(
    GPU::FenceType type,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUFence>();
}

scoped_refptr<GPUQuery> RenderDeviceImpl::CreateQuery(
    GPU::QueryType type,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUQuery>();
}

scoped_refptr<GPUPipelineSignature> RenderDeviceImpl::CreatePipelineSignature(
    const std::optional<PipelineSignatureDesc>& desc,
    ExceptionState& exception_state) {
  return scoped_refptr<GPUPipelineSignature>();
}

void RenderDeviceImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

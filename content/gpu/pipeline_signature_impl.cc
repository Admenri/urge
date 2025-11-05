// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/pipeline_signature_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/resource_binding_impl.h"
#include "content/gpu/resource_mapping_impl.h"
#include "content/gpu/resource_variable_impl.h"

namespace content {

///////////////////////////////////////////////////////////////////////////////
// PipelineSignatureImpl Implement

PipelineSignatureImpl::PipelineSignatureImpl(
    ExecutionContext* context,
    Diligent::IPipelineResourceSignature* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(PipelineSignatureImpl);

scoped_refptr<GPUResourceBinding> PipelineSignatureImpl::CreateResourceBinding(
    bool init_static_resources,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> result;
  object_->CreateShaderResourceBinding(&result, init_static_resources);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<ResourceBindingImpl>(context(), result);
}

void PipelineSignatureImpl::BindStaticResources(
    GPU::ShaderType shader_type,
    scoped_refptr<GPUResourceMapping> mapping,
    GPU::BindShaderResourcesFlags flags,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_mapping = static_cast<ResourceMappingImpl*>(mapping.get());
  auto* object_mapping = raw_mapping ? raw_mapping->AsRawPtr() : nullptr;

  object_->BindStaticResources(
      static_cast<Diligent::SHADER_TYPE>(shader_type), object_mapping,
      static_cast<Diligent::BIND_SHADER_RESOURCES_FLAGS>(flags));
}

scoped_refptr<GPUResourceVariable>
PipelineSignatureImpl::GetStaticVariableByName(
    GPU::ShaderType type,
    const std::string& name,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto result = object_->GetStaticVariableByName(
      static_cast<Diligent::SHADER_TYPE>(type), name.c_str());
  if (!result)
    return nullptr;

  return base::MakeRefCounted<ResourceVariableImpl>(context(), result);
}

scoped_refptr<GPUResourceVariable>
PipelineSignatureImpl::GetStaticVariableByIndex(
    GPU::ShaderType type,
    uint32_t index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto result = object_->GetStaticVariableByIndex(
      static_cast<Diligent::SHADER_TYPE>(type), index);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<ResourceVariableImpl>(context(), result);
}

uint32_t PipelineSignatureImpl::GetStaticVariableCount(
    GPU::ShaderType type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetStaticVariableCount(
      static_cast<Diligent::SHADER_TYPE>(type));
}

void PipelineSignatureImpl::InitializeStaticSRBResources(
    scoped_refptr<GPUResourceBinding> srb,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_srb = static_cast<ResourceBindingImpl*>(srb.get());
  auto* object_srb = raw_srb ? raw_srb->AsRawPtr() : nullptr;

  return object_->InitializeStaticSRBResources(object_srb);
}

void PipelineSignatureImpl::CopyStaticResources(
    scoped_refptr<GPUPipelineSignature> dst,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_other = static_cast<PipelineSignatureImpl*>(dst.get());
  auto* object_other = raw_other ? raw_other->AsRawPtr() : nullptr;

  return object_->CopyStaticResources(object_other);
}

bool PipelineSignatureImpl::IsCompatibleWith(
    scoped_refptr<GPUPipelineSignature> other,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  auto* raw_other = static_cast<PipelineSignatureImpl*>(other.get());
  auto* object_other = raw_other ? raw_other->AsRawPtr() : nullptr;

  return object_->IsCompatibleWith(object_other);
}

void PipelineSignatureImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

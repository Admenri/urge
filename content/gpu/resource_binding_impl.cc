// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/resource_binding_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/pipeline_signature_impl.h"
#include "content/gpu/resource_mapping_impl.h"
#include "content/gpu/resource_variable_impl.h"

namespace content {

///////////////////////////////////////////////////////////////////////////////
// ResourceBindingImpl Implement

ResourceBindingImpl::ResourceBindingImpl(
    ExecutionContext* context,
    Diligent::IShaderResourceBinding* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(ResourceBindingImpl);

scoped_refptr<GPUPipelineSignature> ResourceBindingImpl::GetPipelineSignature(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  if (!object_->GetPipelineResourceSignature())
    return nullptr;

  return base::MakeRefCounted<PipelineSignatureImpl>(
      context(), object_->GetPipelineResourceSignature());
}

void ResourceBindingImpl::BindResources(
    GPU::ShaderType shader_type,
    scoped_refptr<GPUResourceMapping> mapping,
    GPU::BindShaderResourcesFlags flags,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_mapping = static_cast<ResourceMappingImpl*>(mapping.get());
  auto* object_mapping = raw_mapping ? raw_mapping->AsRawPtr() : nullptr;

  object_->BindResources(
      static_cast<Diligent::SHADER_TYPE>(shader_type), object_mapping,
      static_cast<Diligent::BIND_SHADER_RESOURCES_FLAGS>(flags));
}

GPU::ShaderResourceVariableType ResourceBindingImpl::CheckResources(
    GPU::ShaderType shader_type,
    scoped_refptr<GPUResourceMapping> mapping,
    GPU::BindShaderResourcesFlags flags,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::ShaderResourceVariableType());

  auto* raw_mapping = static_cast<ResourceMappingImpl*>(mapping.get());
  auto* object_mapping = raw_mapping ? raw_mapping->AsRawPtr() : nullptr;

  return GPU::ShaderResourceVariableType(object_->CheckResources(
      static_cast<Diligent::SHADER_TYPE>(shader_type), object_mapping,
      static_cast<Diligent::BIND_SHADER_RESOURCES_FLAGS>(flags)));
}

scoped_refptr<GPUResourceVariable> ResourceBindingImpl::GetVariableByName(
    GPU::ShaderType stage,
    const std::string& name,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->GetVariableByName(
      static_cast<Diligent::SHADER_TYPE>(stage), name.c_str());

  return base::MakeRefCounted<ResourceVariableImpl>(context(), result);
}

uint32_t ResourceBindingImpl::GetVariableCount(
    GPU::ShaderType stage,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetVariableCount(static_cast<Diligent::SHADER_TYPE>(stage));
}

scoped_refptr<GPUResourceVariable> ResourceBindingImpl::GetVariableByIndex(
    GPU::ShaderType stage,
    uint32_t index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->GetVariableByIndex(
      static_cast<Diligent::SHADER_TYPE>(stage), index);

  return base::MakeRefCounted<ResourceVariableImpl>(context(), result);
}

bool ResourceBindingImpl::StaticResourcesInitialized(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return object_->StaticResourcesInitialized();
}

void ResourceBindingImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

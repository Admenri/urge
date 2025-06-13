// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/resource_variable_impl.h"

#include "content/context/execution_context.h"

namespace content {

ResourceVariableImpl::ResourceVariableImpl(
    ExecutionContext* context,
    Diligent::IShaderResourceVariable* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

ResourceVariableImpl::~ResourceVariableImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void ResourceVariableImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool ResourceVariableImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void ResourceVariableImpl::Set(uint64_t device_object,
                               GPU::SetShaderResourceFlags flags,
                               ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->Set(reinterpret_cast<Diligent::IDeviceObject*>(device_object),
               static_cast<Diligent::SET_SHADER_RESOURCE_FLAGS>(flags));
}

void ResourceVariableImpl::SetArray(
    const base::Vector<uint64_t>& device_objects,
    uint32_t first_element,
    GPU::SetShaderResourceFlags flags,
    ExceptionState& exception_state) {
  DISPOSE_CHECK;

  base::Vector<Diligent::IDeviceObject*> objects;
  for (auto element : device_objects)
    objects.push_back(reinterpret_cast<Diligent::IDeviceObject*>(element));

  object_->SetArray(objects.data(), first_element, objects.size(),
                    static_cast<Diligent::SET_SHADER_RESOURCE_FLAGS>(flags));
}

void ResourceVariableImpl::SetBufferRange(uint64_t device_object,
                                          uint64_t offset,
                                          uint64_t size,
                                          uint32_t array_index,
                                          GPU::SetShaderResourceFlags flags,
                                          ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->SetBufferRange(
      reinterpret_cast<Diligent::IDeviceObject*>(device_object), offset, size,
      array_index, static_cast<Diligent::SET_SHADER_RESOURCE_FLAGS>(flags));
}

GPU::ShaderResourceVariableType ResourceVariableImpl::GetType(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::ShaderResourceVariableType());

  return static_cast<GPU::ShaderResourceVariableType>(object_->GetType());
}

std::optional<GPUResourceVariable::ShaderResourceDesc>
ResourceVariableImpl::GetResourceDesc(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(std::nullopt);

  Diligent::ShaderResourceDesc desc;
  object_->GetResourceDesc(desc);

  GPUResourceVariable::ShaderResourceDesc result;
  result.name = desc.Name;
  result.type = static_cast<GPU::ShaderResourceType>(desc.Type);
  result.array_size = desc.ArraySize;

  return result;
}

uint32_t ResourceVariableImpl::GetIndex(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetIndex();
}

uint64_t ResourceVariableImpl::Get(uint32_t array_index,
                                   ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return reinterpret_cast<uint64_t>(object_->Get(array_index));
}

void ResourceVariableImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/resource_mapping_impl.h"

#include "content/context/execution_context.h"

namespace content {

ResourceMappingImpl::ResourceMappingImpl(
    ExecutionContext* context,
    Diligent::RefCntAutoPtr<Diligent::IResourceMapping> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

ResourceMappingImpl::~ResourceMappingImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void ResourceMappingImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool ResourceMappingImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void ResourceMappingImpl::AddResource(const base::String& name,
                                      uint64_t device_object,
                                      bool is_unique,
                                      ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  object_->AddResource(
      name.c_str(), reinterpret_cast<Diligent::IDeviceObject*>(device_object),
      is_unique);
}

void ResourceMappingImpl::AddResourceArray(
    const base::String& name,
    uint32_t start_index,
    const base::Vector<uint64_t>& device_objects,
    bool is_unique,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  base::Vector<Diligent::IDeviceObject*> objects;
  for (auto element : device_objects)
    objects.push_back(reinterpret_cast<Diligent::IDeviceObject*>(element));

  object_->AddResourceArray(name.c_str(), start_index, objects.data(),
                            objects.size(), is_unique);
}

void ResourceMappingImpl::RemoveResourceByName(
    const base::String& name,
    uint32_t array_index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  object_->RemoveResourceByName(name.c_str(), array_index);
}

uint64_t ResourceMappingImpl::GetResource(const base::String& name,
                                          uint32_t array_index,
                                          ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  return reinterpret_cast<uint64_t>(
      object_->GetResource(name.c_str(), array_index));
}

uint64_t ResourceMappingImpl::GetSize(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  return object_->GetSize();
}

void ResourceMappingImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

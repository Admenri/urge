// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/buffer_impl.h"

#include "content/context/execution_context.h"

namespace content {

BufferViewImpl::BufferViewImpl(
    ExecutionContext* context,
    Diligent::RefCntAutoPtr<Diligent::IBufferView> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

BufferViewImpl::~BufferViewImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void BufferViewImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool BufferViewImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint64_t BufferViewImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUBuffer> BufferViewImpl::GetBuffer(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return base::MakeRefCounted<BufferImpl>(context(), object_->GetBuffer());
}

void BufferViewImpl::OnObjectDisposed() {
  object_.Release();
}

BufferImpl::BufferImpl(ExecutionContext* context,
                       Diligent::RefCntAutoPtr<Diligent::IBuffer> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

BufferImpl::~BufferImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void BufferImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool BufferImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint64_t BufferImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUBufferView> BufferImpl::CreateView(
    std::optional<BufferViewDesc> desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::BufferViewDesc createinfo_desc;
  if (desc) {
    createinfo_desc.ViewType =
        static_cast<Diligent::BUFFER_VIEW_TYPE>(desc->view_type);
    createinfo_desc.Format.ValueType =
        static_cast<Diligent::VALUE_TYPE>(desc->value_type);
    createinfo_desc.Format.NumComponents = desc->num_components;
    createinfo_desc.Format.IsNormalized = desc->is_normalized;
    createinfo_desc.ByteOffset = desc->byte_offset;
    createinfo_desc.ByteWidth = desc->byte_width;
  }

  Diligent::RefCntAutoPtr<Diligent::IBufferView> result;
  object_->CreateView(createinfo_desc, &result);

  return base::MakeRefCounted<BufferViewImpl>(context(), result);
}

scoped_refptr<GPUBufferView> BufferImpl::GetDefaultView(
    GPU::BufferViewType view_type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->GetDefaultView(
      static_cast<Diligent::BUFFER_VIEW_TYPE>(view_type));

  return base::MakeRefCounted<BufferViewImpl>(context(), result);
}

void BufferImpl::FlushMappedRange(uint64_t start_offset,
                                  uint64_t size,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  object_->FlushMappedRange(start_offset, size);
}

void BufferImpl::InvalidateMappedRange(uint64_t start_offset,
                                       uint64_t size,
                                       ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  object_->InvalidateMappedRange(start_offset, size);
}

GPU::ResourceState BufferImpl::Get_State(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::ResourceState());

  return static_cast<GPU::ResourceState>(object_->GetState());
}

void BufferImpl::Put_State(const GPU::ResourceState& value,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  object_->SetState(static_cast<Diligent::RESOURCE_STATE>(value));
}

void BufferImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

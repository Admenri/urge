// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/buffer_impl.h"

#include "content/context/execution_context.h"

namespace content {

///////////////////////////////////////////////////////////////////////////////
// BufferViewImpl Implement

BufferViewImpl::BufferViewImpl(ExecutionContext* context,
                               Diligent::IBufferView* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(BufferViewImpl);

uint64_t BufferViewImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUBufferViewDesc> BufferViewImpl::GetDesc(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto& object_desc = object_->GetDesc();
  auto result = base::MakeRefCounted<GPUBufferViewDesc>();

  result->view_type = static_cast<GPU::BufferViewType>(object_desc.ViewType);
  result->value_type =
      static_cast<GPU::ValueType>(object_desc.Format.ValueType);
  result->num_components = object_desc.Format.NumComponents;
  result->is_normalized = object_desc.Format.IsNormalized;
  result->byte_offset = object_desc.ByteOffset;
  result->byte_width = object_desc.ByteWidth;

  return result;
}

scoped_refptr<GPUBuffer> BufferViewImpl::GetBuffer(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  if (!object_->GetBuffer())
    return nullptr;

  return base::MakeRefCounted<BufferImpl>(context(), object_->GetBuffer());
}

void BufferViewImpl::OnObjectDisposed() {
  object_.Release();
}

///////////////////////////////////////////////////////////////////////////////
// BufferImpl Implement

BufferImpl::BufferImpl(ExecutionContext* context, Diligent::IBuffer* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(BufferImpl);

uint64_t BufferImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUBufferDesc> BufferImpl::GetDesc(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto& object_desc = object_->GetDesc();
  auto result = base::MakeRefCounted<GPUBufferDesc>();

  result->size = object_desc.Size;
  result->bind_flags = static_cast<GPU::BindFlags>(object_desc.BindFlags);
  result->usage = static_cast<GPU::Usage>(object_desc.Usage);
  result->cpu_access_flags =
      static_cast<GPU::CPUAccessFlags>(object_desc.CPUAccessFlags);
  result->mode = static_cast<GPU::BufferMode>(object_desc.Mode);
  result->element_byte_stride = object_desc.ElementByteStride;
  result->immediate_context_mask = object_desc.ImmediateContextMask;

  return result;
}

scoped_refptr<GPUBufferView> BufferImpl::CreateView(
    scoped_refptr<GPUBufferViewDesc> desc,
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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<BufferViewImpl>(context(), result);
}

scoped_refptr<GPUBufferView> BufferImpl::GetDefaultView(
    GPU::BufferViewType view_type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->GetDefaultView(
      static_cast<Diligent::BUFFER_VIEW_TYPE>(view_type));
  if (!result)
    return nullptr;

  return base::MakeRefCounted<BufferViewImpl>(context(), result);
}

void BufferImpl::FlushMappedRange(uint64_t start_offset,
                                  uint64_t size,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->FlushMappedRange(start_offset, size);
}

void BufferImpl::InvalidateMappedRange(uint64_t start_offset,
                                       uint64_t size,
                                       ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->InvalidateMappedRange(start_offset, size);
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    State,
    GPU::ResourceState,
    BufferImpl,
    {
      DISPOSE_CHECK_RETURN(GPU::ResourceState());
      return static_cast<GPU::ResourceState>(object_->GetState());
    },
    {
      DISPOSE_CHECK;
      object_->SetState(static_cast<Diligent::RESOURCE_STATE>(value));
    });

void BufferImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

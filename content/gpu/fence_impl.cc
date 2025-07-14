// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/fence_impl.h"

#include "content/context/execution_context.h"

namespace content {

FenceImpl::FenceImpl(ExecutionContext* context, Diligent::IFence* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(FenceImpl);

GPU::FenceType FenceImpl::GetType(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::FenceType());

  return static_cast<GPU::FenceType>(object_->GetDesc().Type);
}

uint64_t FenceImpl::GetCompletedValue(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetCompletedValue();
}

void FenceImpl::Signal(uint64_t value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->Signal(value);
}

void FenceImpl::Wait(uint64_t value, ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->Wait(value);
}

void FenceImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

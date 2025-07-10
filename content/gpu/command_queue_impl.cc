// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/command_queue_impl.h"

#include "content/context/execution_context.h"

namespace content {

CommandQueueImpl::CommandQueueImpl(ExecutionContext* context,
                                   Diligent::ICommandQueue* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

CommandQueueImpl::~CommandQueueImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void CommandQueueImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool CommandQueueImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint64_t CommandQueueImpl::GetNextFenceValue(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetNextFenceValue();
}

uint64_t CommandQueueImpl::GetCompletedFenceValue(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetCompletedFenceValue();
}

uint64_t CommandQueueImpl::WaitForIdle(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->WaitForIdle();
}

void CommandQueueImpl::OnObjectDisposed() {
  object_ = nullptr;
}

}  // namespace content

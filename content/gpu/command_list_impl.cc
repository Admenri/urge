// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/command_list_impl.h"

#include "content/context/execution_context.h"

namespace content {

CommandListImpl::CommandListImpl(
    ExecutionContext* context,
    Diligent::RefCntAutoPtr<Diligent::ICommandList> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

CommandListImpl::~CommandListImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void CommandListImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool CommandListImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void CommandListImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

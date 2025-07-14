// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/query_impl.h"

#include "content/context/execution_context.h"

namespace content {

QueryImpl::QueryImpl(ExecutionContext* context, Diligent::IQuery* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(QueryImpl);

bool QueryImpl::GetData(void* data,
                        uint32_t size,
                        bool auto_invalidate,
                        ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(false);

  return object_->GetData(data, size, auto_invalidate);
}

void QueryImpl::Invalidate(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->Invalidate();
}

void QueryImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

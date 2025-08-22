// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/net/fetch_impl.h"

#include "content/context/execution_context.h"

#include <numeric>

namespace content {

scoped_refptr<Fetch> Fetch::New(ExecutionContext* execution_context,
                                scoped_refptr<HTTPRequest> request,
                                ExceptionState& exception_state) {
  return nullptr;
}

FetchImpl::FetchImpl(ExecutionContext* execution_context)
    : EngineObject(execution_context),
      context_(execution_context->network_context->GetContext()),
      resolver_(*context_),
      socket_(*context_),
      ready_state_(ReadyState::STATE_UNSENT) {}

FetchImpl::~FetchImpl() {}

void FetchImpl::Send(ExceptionState& exception_state) {}

void FetchImpl::Abort(ExceptionState& exception_state) {}

Fetch::ReadyState FetchImpl::GetReadyState(ExceptionState& exception_state) {
  return ready_state_.load();
}

void FetchImpl::SetResponseHandler(ResponseHandler handler,
                                   ExceptionState& exception_state) {
  on_response_ = std::move(handler);
}

void FetchImpl::SetProgressHandler(ProgressHandler handler,
                                   ExceptionState& exception_state) {
  on_progress_ = std::move(handler);
}

}  // namespace content

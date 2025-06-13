// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/sampler_impl.h"

#include "content/context/execution_context.h"

namespace content {

SamplerImpl::SamplerImpl(ExecutionContext* context,
                         Diligent::RefCntAutoPtr<Diligent::ISampler> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

SamplerImpl::~SamplerImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void SamplerImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool SamplerImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void SamplerImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

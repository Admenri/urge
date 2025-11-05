// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/sampler_impl.h"

#include "content/context/execution_context.h"

namespace content {

///////////////////////////////////////////////////////////////////////////////
// SamplerImpl Implement

SamplerImpl::SamplerImpl(ExecutionContext* context, Diligent::ISampler* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(SamplerImpl);

void SamplerImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

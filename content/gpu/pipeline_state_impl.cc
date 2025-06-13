// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/pipeline_state_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/pipeline_signature_impl.h"

namespace content {

PipelineStateImpl::PipelineStateImpl(
    ExecutionContext* context,
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

PipelineStateImpl::~PipelineStateImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void PipelineStateImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool PipelineStateImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint32_t PipelineStateImpl::GetResourceSignatureCount(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetResourceSignatureCount();
}

scoped_refptr<GPUPipelineSignature> PipelineStateImpl::GetResourceSignature(
    uint32_t index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto result = object_->GetResourceSignature(index);

  return base::MakeRefCounted<PipelineSignatureImpl>(context(), result);
}

GPU::PipelineStateStatus PipelineStateImpl::GetStatus(
    bool wait_for_completion,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::PipelineStateStatus());

  return static_cast<GPU::PipelineStateStatus>(
      object_->GetStatus(wait_for_completion));
}

void PipelineStateImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

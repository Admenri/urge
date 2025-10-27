// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_PIPELINE_STATE_IMPL_H_
#define CONTENT_GPU_PIPELINE_STATE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpupipelinestate.h"

namespace content {

class PipelineStateImpl : public GPUPipelineState,
                          public EngineObject,
                          public Disposable {
 public:
  PipelineStateImpl(ExecutionContext* context,
                    Diligent::IPipelineState* object);
  ~PipelineStateImpl() override;

  PipelineStateImpl(const PipelineStateImpl&) = delete;
  PipelineStateImpl& operator=(const PipelineStateImpl&) = delete;

  Diligent::IPipelineState* AsRawPtr() const { return object_; }

 protected:
  // GPUPipelineState interface
  URGE_DECLARE_DISPOSABLE;
  uint32_t GetResourceSignatureCount(ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineSignature> GetResourceSignature(
      uint32_t index,
      ExceptionState& exception_state) override;
  GPU::PipelineStateStatus GetStatus(bool wait_for_completion,
                                     ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.PipelineState"; }

  Diligent::RefCntAutoPtr<Diligent::IPipelineState> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_PIPELINE_STATE_IMPL_H_

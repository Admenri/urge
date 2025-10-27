// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUPIPELINESTATE_H_
#define CONTENT_PUBLIC_ENGINE_GPUPIPELINESTATE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpupipelinesignature.h"

namespace content {

/*--urge(name:GPUPipelineState)--*/
class URGE_OBJECT(GPUPipelineState) {
 public:
  virtual ~GPUPipelineState() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:resource_signature_count)--*/
  virtual uint32_t GetResourceSignatureCount(
      ExceptionState& exception_state) = 0;

  /*--urge(name:resource_signature)--*/
  virtual scoped_refptr<GPUPipelineSignature> GetResourceSignature(
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:status)--*/
  virtual GPU::PipelineStateStatus GetStatus(
      bool wait_for_completion,
      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUPIPELINESTATE_H_

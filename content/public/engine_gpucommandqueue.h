// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUCOMMANDQUEUE_H_
#define CONTENT_PUBLIC_ENGINE_GPUCOMMANDQUEUE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUCommandQueue)--*/
class URGE_OBJECT(GPUCommandQueue) {
 public:
  virtual ~GPUCommandQueue() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:next_fence_value)--*/
  virtual uint64_t GetNextFenceValue(ExceptionState& exception_state) = 0;

  /*--urge(name:completed_fence_value)--*/
  virtual uint64_t GetCompletedFenceValue(ExceptionState& exception_state) = 0;

  /*--urge(name:wait_for_idle)--*/
  virtual uint64_t WaitForIdle(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUCOMMANDQUEUE_H_

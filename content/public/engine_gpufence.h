// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUFENCE_H_
#define CONTENT_PUBLIC_ENGINE_GPUFENCE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUFence)--*/
class URGE_OBJECT(GPUFence) {
 public:
  virtual ~GPUFence() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:type)--*/
  virtual GPU::FenceType GetType(ExceptionState& exception_state) = 0;

  /*--urge(name:completed_value)--*/
  virtual uint64_t GetCompletedValue(ExceptionState& exception_state) = 0;

  /*--urge(name:signal)--*/
  virtual void Signal(uint64_t value, ExceptionState& exception_state) = 0;

  /*--urge(name:wait)--*/
  virtual void Wait(uint64_t value, ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUFENCE_H_

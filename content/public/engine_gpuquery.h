// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUQUERY_H_
#define CONTENT_PUBLIC_ENGINE_GPUQUERY_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUQuery)--*/
class URGE_OBJECT(GPUQuery) {
 public:
  virtual ~GPUQuery() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:data)--*/
  virtual bool GetData(void* data,
                       uint32_t size,
                       bool auto_invalidate,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:invalidate)--*/
  virtual void Invalidate(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUQUERY_H_

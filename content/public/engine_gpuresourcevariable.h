// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURESOURCEVARIABLE_H_
#define CONTENT_PUBLIC_ENGINE_GPURESOURCEVARIABLE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:GPUResourceVariable)--*/
class URGE_RUNTIME_API GPUResourceVariable
    : public base::RefCounted<GPUResourceVariable> {
 public:
  virtual ~GPUResourceVariable() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURESOURCEVARIABLE_H_

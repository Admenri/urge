// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUCOMMANDLIST_H_
#define CONTENT_PUBLIC_ENGINE_GPUCOMMANDLIST_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUCommandList)--*/
class URGE_OBJECT(GPUCommandList) {
 public:
  virtual ~GPUCommandList() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUCOMMANDLIST_H_

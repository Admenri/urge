// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUBUFFER_H_
#define CONTENT_PUBLIC_ENGINE_GPUBUFFER_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:GPUBuffer)--*/
class URGE_RUNTIME_API GPUBuffer : public base::RefCounted<GPUBuffer> {
 public:
  virtual ~GPUBuffer() = default;
};

/*--urge(name:GPUBufferView)--*/
class URGE_RUNTIME_API GPUBufferView : public base::RefCounted<GPUBufferView> {
 public:
  virtual ~GPUBufferView() = default;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUBUFFER_H_

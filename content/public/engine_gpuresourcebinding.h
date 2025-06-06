// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURESOURCEBINDING_H_
#define CONTENT_PUBLIC_ENGINE_GPURESOURCEBINDING_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:GPUResourceBinding)--*/
class URGE_RUNTIME_API GPUResourceBinding
    : public base::RefCounted<GPUResourceBinding> {
 public:
  virtual ~GPUResourceBinding() = default;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURESOURCEBINDING_H_

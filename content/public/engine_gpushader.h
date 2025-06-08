// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUSHADER_H_
#define CONTENT_PUBLIC_ENGINE_GPUSHADER_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:GPUShader)--*/
class URGE_RUNTIME_API GPUShader : public base::RefCounted<GPUShader> {
 public:
  virtual ~GPUShader() = default;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUSHADER_H_

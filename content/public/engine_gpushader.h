// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUSHADER_H_
#define CONTENT_PUBLIC_ENGINE_GPUSHADER_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUShader)--*/
class URGE_OBJECT(GPUShader) {
 public:
  virtual ~GPUShader() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:resource_count)--*/
  virtual uint32_t GetResourceCount(ExceptionState& exception_state) = 0;

  /*--urge(name:resource_desc)--*/
  virtual scoped_refptr<GPUShaderResourceDesc> GetResourceDesc(
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:constant_buffer_desc)--*/
  virtual scoped_refptr<GPUShaderCodeBufferDesc> GetConstantBufferDesc(
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:bytecode)--*/
  virtual std::string GetBytecode(ExceptionState& exception_state) = 0;

  /*--urge(name:status)--*/
  virtual GPU::ShaderStatus GetStatus(bool wait_for_completion,
                                      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUSHADER_H_

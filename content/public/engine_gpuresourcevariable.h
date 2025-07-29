// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURESOURCEVARIABLE_H_
#define CONTENT_PUBLIC_ENGINE_GPURESOURCEVARIABLE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"

namespace content {

/*--urge(name:GPUResourceVariable)--*/
class URGE_OBJECT(GPUResourceVariable) {
 public:
  virtual ~GPUResourceVariable() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:set)--*/
  virtual void Set(uint64_t device_object,
                   GPU::SetShaderResourceFlags flags,
                   ExceptionState& exception_state) = 0;

  /*--urge(name:set_array)--*/
  virtual void SetArray(const std::vector<uint64_t>& device_objects,
                        uint32_t first_element,
                        GPU::SetShaderResourceFlags flags,
                        ExceptionState& exception_state) = 0;

  /*--urge(name:set_buffer_range)--*/
  virtual void SetBufferRange(uint64_t device_object,
                              uint64_t offset,
                              uint64_t size,
                              uint32_t array_index,
                              GPU::SetShaderResourceFlags flags,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:type)--*/
  virtual GPU::ShaderResourceVariableType GetType(
      ExceptionState& exception_state) = 0;

  /*--urge(name:resource_desc)--*/
  virtual scoped_refptr<GPUShaderResourceDesc> GetResourceDesc(
      ExceptionState& exception_state) = 0;

  /*--urge(name:index)--*/
  virtual uint32_t GetIndex(ExceptionState& exception_state) = 0;

  /*--urge(name:get)--*/
  virtual uint64_t Get(uint32_t array_index,
                       ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURESOURCEVARIABLE_H_

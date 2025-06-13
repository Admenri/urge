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
class URGE_RUNTIME_API GPUShader : public base::RefCounted<GPUShader> {
 public:
  virtual ~GPUShader() = default;

  /*--urge(name:ShaderResourceDesc)--*/
  struct ShaderResourceDesc {
    base::String name;
    GPU::ShaderResourceType type;
    uint32_t array_size;
  };

  /*--urge(name:ShaderCodeVariableDesc)--*/
  struct ShaderCodeVariableDesc {
    base::String name;
    base::String type_name;
    GPU::ShaderCodeVariableClass klass =
        GPU::SHADER_CODE_VARIABLE_CLASS_UNKNOWN;
    GPU::ShaderCodeBasicType basic_type = GPU::SHADER_CODE_BASIC_TYPE_UNKNOWN;
    uint8_t num_rows = 0;
    uint8_t num_columns = 0;
    uint32_t offset = 0;
    uint32_t array_size = 0;
    base::Vector<ShaderCodeVariableDesc> members;
  };

  /*--urge(name:ShaderCodeBufferDesc)--*/
  struct ShaderCodeBufferDesc {
    uint32_t byte_size;
    base::Vector<ShaderCodeVariableDesc> variables;
  };

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:resource_count)--*/
  virtual uint32_t GetResourceCount(ExceptionState& exception_state) = 0;

  /*--urge(name:resource_desc)--*/
  virtual std::optional<ShaderResourceDesc> GetResourceDesc(
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:constant_buffer_desc)--*/
  virtual std::optional<ShaderCodeBufferDesc> GetConstantBufferDesc(
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:bytecode)--*/
  virtual base::String GetBytecode(ExceptionState& exception_state) = 0;

  /*--urge(name:status)--*/
  virtual GPU::ShaderStatus GetStatus(bool wait_for_completion,
                                      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUSHADER_H_

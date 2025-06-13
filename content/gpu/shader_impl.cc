// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/shader_impl.h"

#include "content/context/execution_context.h"

namespace content {

namespace {

GPUShader::ShaderCodeVariableDesc GetVariableDesc(
    const Diligent::ShaderCodeVariableDesc* desc) {
  GPUShader::ShaderCodeVariableDesc variable_desc;
  variable_desc.name = desc->Name;
  variable_desc.type_name = desc->TypeName;
  variable_desc.klass = static_cast<GPU::ShaderCodeVariableClass>(desc->Class);
  variable_desc.basic_type =
      static_cast<GPU::ShaderCodeBasicType>(desc->BasicType);
  variable_desc.num_rows = desc->NumRows;
  variable_desc.num_columns = desc->NumColumns;
  variable_desc.offset = desc->Offset;
  variable_desc.array_size = desc->ArraySize;
  for (uint32_t i = 0; i < desc->NumMembers; ++i)
    variable_desc.members.push_back(GetVariableDesc(&desc->pMembers[i]));

  return variable_desc;
}

}  // namespace

ShaderImpl::ShaderImpl(ExecutionContext* context, Diligent::IShader* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

ShaderImpl::~ShaderImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void ShaderImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool ShaderImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint32_t ShaderImpl::GetResourceCount(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetResourceCount();
}

std::optional<GPUShader::ShaderResourceDesc> ShaderImpl::GetResourceDesc(
    uint32_t index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(std::nullopt);

  Diligent::ShaderResourceDesc desc;
  object_->GetResourceDesc(index, desc);

  GPUShader::ShaderResourceDesc result;
  result.name = desc.Name;
  result.type = static_cast<GPU::ShaderResourceType>(desc.Type);
  result.array_size = desc.ArraySize;

  return result;
}

std::optional<GPUShader::ShaderCodeBufferDesc>
ShaderImpl::GetConstantBufferDesc(uint32_t index,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(std::nullopt);

  const Diligent::ShaderCodeBufferDesc* desc =
      object_->GetConstantBufferDesc(index);

  GPUShader::ShaderCodeBufferDesc result;
  result.byte_size = desc->Size;
  for (uint32_t i = 0; i < desc->NumVariables; ++i)
    result.variables.push_back(GetVariableDesc(&desc->pVariables[i]));

  return result;
}

base::String ShaderImpl::GetBytecode(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(base::String());

  const void* byte_buffer;
  uint64_t byte_size;
  object_->GetBytecode(&byte_buffer, byte_size);

  base::String buffer(byte_size, 0);
  std::memcpy(buffer.data(), byte_buffer, byte_size);

  return buffer;
}

GPU::ShaderStatus ShaderImpl::GetStatus(bool wait_for_completion,
                                        ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::ShaderStatus());

  return static_cast<GPU::ShaderStatus>(
      object_->GetStatus(wait_for_completion));
}

void ShaderImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

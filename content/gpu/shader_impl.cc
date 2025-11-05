// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/shader_impl.h"

#include "content/context/execution_context.h"

namespace content {

namespace {

scoped_refptr<GPUShaderCodeVariableDesc> GetVariableDesc(
    const Diligent::ShaderCodeVariableDesc* desc) {
  auto variable_desc = base::MakeRefCounted<GPUShaderCodeVariableDesc>();
  variable_desc->name = desc->Name;
  variable_desc->type_name = desc->TypeName;
  variable_desc->klass = static_cast<GPU::ShaderCodeVariableClass>(desc->Class);
  variable_desc->basic_type =
      static_cast<GPU::ShaderCodeBasicType>(desc->BasicType);
  variable_desc->num_rows = desc->NumRows;
  variable_desc->num_columns = desc->NumColumns;
  variable_desc->offset = desc->Offset;
  variable_desc->array_size = desc->ArraySize;
  for (uint32_t i = 0; i < desc->NumMembers; ++i)
    variable_desc->members.push_back(GetVariableDesc(&desc->pMembers[i]));

  return variable_desc;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// ShaderImpl Implement

ShaderImpl::ShaderImpl(ExecutionContext* context, Diligent::IShader* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(ShaderImpl);

uint32_t ShaderImpl::GetResourceCount(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  return object_->GetResourceCount();
}

scoped_refptr<GPUShaderResourceDesc> ShaderImpl::GetResourceDesc(
    uint32_t index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::ShaderResourceDesc desc;
  object_->GetResourceDesc(index, desc);

  auto result = base::MakeRefCounted<GPUShaderResourceDesc>();
  result->name = desc.Name;
  result->type = static_cast<GPU::ShaderResourceType>(desc.Type);
  result->array_size = desc.ArraySize;

  return result;
}

scoped_refptr<GPUShaderCodeBufferDesc> ShaderImpl::GetConstantBufferDesc(
    uint32_t index,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  const Diligent::ShaderCodeBufferDesc* desc =
      object_->GetConstantBufferDesc(index);

  auto result = base::MakeRefCounted<GPUShaderCodeBufferDesc>();
  result->byte_size = desc->Size;
  for (uint32_t i = 0; i < desc->NumVariables; ++i)
    result->variables.push_back(GetVariableDesc(&desc->pVariables[i]));

  return result;
}

std::string ShaderImpl::GetBytecode(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(std::string());

  const void* byte_buffer;
  uint64_t byte_size;
  object_->GetBytecode(&byte_buffer, byte_size);

  std::string buffer(byte_size, 0);
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

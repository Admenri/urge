// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURESOURCEBINDING_H_
#define CONTENT_PUBLIC_ENGINE_GPURESOURCEBINDING_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpupipelinesignature.h"
#include "content/public/engine_gpuresourcevariable.h"

namespace content {

class GPUPipelineSignature;
class GPUResourceMapping;

/*--urge(name:GPUResourceBinding)--*/
class URGE_RUNTIME_API GPUResourceBinding
    : public base::RefCounted<GPUResourceBinding> {
 public:
  virtual ~GPUResourceBinding() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:pipeline_signature)--*/
  virtual scoped_refptr<GPUPipelineSignature> GetPipelineSignature(
      ExceptionState& exception_state) = 0;

  /*--urge(name:bind_resources)--*/
  virtual void BindResources(GPU::ShaderType shader_type,
                             scoped_refptr<GPUResourceMapping> mapping,
                             GPU::BindShaderResourcesFlags flags,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:check_resources)--*/
  virtual GPU::ShaderResourceVariableType CheckResources(
      GPU::ShaderType shader_type,
      scoped_refptr<GPUResourceMapping> mapping,
      GPU::BindShaderResourcesFlags flags,
      ExceptionState& exception_state) = 0;

  /*--urge(name:variable_by_name)--*/
  virtual scoped_refptr<GPUResourceVariable> GetVariableByName(
      GPU::ShaderType stage,
      const base::String& name,
      ExceptionState& exception_state) = 0;

  /*--urge(name:variable_count)--*/
  virtual uint32_t GetVariableCount(GPU::ShaderType stage,
                                    ExceptionState& exception_state) = 0;

  /*--urge(name:variable_by_index)--*/
  virtual scoped_refptr<GPUResourceVariable> GetVariableByIndex(
      GPU::ShaderType stage,
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:static_resources_initialized?)--*/
  virtual bool StaticResourcesInitialized(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURESOURCEBINDING_H_

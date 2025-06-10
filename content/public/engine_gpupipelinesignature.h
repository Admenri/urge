// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUPIPELINESIGNATURE_H_
#define CONTENT_PUBLIC_ENGINE_GPUPIPELINESIGNATURE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpuresourcebinding.h"
#include "content/public/engine_gpuresourcevariable.h"

namespace content {

/*--urge(name:GPUPipelineSignature)--*/
class URGE_RUNTIME_API GPUPipelineSignature
    : public base::RefCounted<GPUPipelineSignature> {
 public:
  virtual ~GPUPipelineSignature() = default;

  /*--urge(name:create_resource_binding,optional:init_static_resources=false)--*/
  virtual scoped_refptr<GPUResourceBinding> CreateResourceBinding(
      bool init_static_resources,
      ExceptionState& exception_state) = 0;

  /*--urge(name:static_variable_by_name)--*/
  virtual scoped_refptr<GPUResourceVariable> GetStaticVariableByName(
      GPU::ShaderType type,
      const base::String& name,
      ExceptionState& exception_state) = 0;

  /*--urge(name:static_variable_by_index)--*/
  virtual scoped_refptr<GPUResourceVariable> GetStaticVariableByIndex(
      GPU::ShaderType type,
      uint32_t index,
      ExceptionState& exception_state) = 0;

  /*--urge(name:static_variable_count)--*/
  virtual uint32_t GetStaticVariableCount(GPU::ShaderType type,
                                          ExceptionState& exception_state) = 0;

  /*--urge(name:initialize_static_srb_resources)--*/
  virtual void InitializeStaticSRBResources(
      scoped_refptr<GPUResourceBinding> srb,
      ExceptionState& exception_state) = 0;

  /*--urge(name:copy_static_resources)--*/
  virtual void CopyStaticResources(scoped_refptr<GPUPipelineSignature> dst,
                                   ExceptionState& exception_state) = 0;

  /*--urge(name:is_compatible_with)--*/
  virtual bool IsCompatibleWith(scoped_refptr<GPUPipelineSignature> other,
                                ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUPIPELINESIGNATURE_H_

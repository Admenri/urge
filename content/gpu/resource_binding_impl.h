// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_RESOURCE_BINDING_IMPL_H_
#define CONTENT_GPU_RESOURCE_BINDING_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpuresourcebinding.h"

namespace content {

class ResourceBindingImpl : public GPUResourceBinding,
                            public EngineObject,
                            public Disposable {
 public:
  ResourceBindingImpl(ExecutionContext* context,
                      Diligent::IShaderResourceBinding* object);
  ~ResourceBindingImpl() override;

  ResourceBindingImpl(const ResourceBindingImpl&) = delete;
  ResourceBindingImpl& operator=(const ResourceBindingImpl&) = delete;

  Diligent::IShaderResourceBinding* AsRawPtr() const { return object_; }

 protected:
  // GPUResourceBinding interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  scoped_refptr<GPUPipelineSignature> GetPipelineSignature(
      ExceptionState& exception_state) override;
  void BindResources(GPU::ShaderType shader_type,
                     scoped_refptr<GPUResourceMapping> mapping,
                     GPU::BindShaderResourcesFlags flags,
                     ExceptionState& exception_state) override;
  GPU::ShaderResourceVariableType CheckResources(
      GPU::ShaderType shader_type,
      scoped_refptr<GPUResourceMapping> mapping,
      GPU::BindShaderResourcesFlags flags,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUResourceVariable> GetVariableByName(
      GPU::ShaderType stage,
      const std::string& name,
      ExceptionState& exception_state) override;
  uint32_t GetVariableCount(GPU::ShaderType stage,
                            ExceptionState& exception_state) override;
  scoped_refptr<GPUResourceVariable> GetVariableByIndex(
      GPU::ShaderType stage,
      uint32_t index,
      ExceptionState& exception_state) override;
  bool StaticResourcesInitialized(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.ResourceBinding"; }

  Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_RESOURCE_BINDING_IMPL_H_

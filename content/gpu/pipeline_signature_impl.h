// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_PIPELINE_SIGNATURE_IMPL_H_
#define CONTENT_GPU_PIPELINE_SIGNATURE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/PipelineResourceSignature.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpupipelinesignature.h"

namespace content {

class PipelineSignatureImpl : public GPUPipelineSignature,
                              public EngineObject,
                              public Disposable {
 public:
  PipelineSignatureImpl(ExecutionContext* context,
                        Diligent::IPipelineResourceSignature* object);
  ~PipelineSignatureImpl() override;

  PipelineSignatureImpl(const PipelineSignatureImpl&) = delete;
  PipelineSignatureImpl& operator=(const PipelineSignatureImpl&) = delete;

  Diligent::IPipelineResourceSignature* AsRawPtr() const { return object_; }

 protected:
  // GPUPipelineSignature interface
  URGE_DECLARE_DISPOSABLE;
  scoped_refptr<GPUResourceBinding> CreateResourceBinding(
      bool init_static_resources,
      ExceptionState& exception_state) override;
  void BindStaticResources(GPU::ShaderType shader_type,
                           scoped_refptr<GPUResourceMapping> mapping,
                           GPU::BindShaderResourcesFlags flags,
                           ExceptionState& exception_state) override;
  scoped_refptr<GPUResourceVariable> GetStaticVariableByName(
      GPU::ShaderType type,
      const std::string& name,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUResourceVariable> GetStaticVariableByIndex(
      GPU::ShaderType type,
      uint32_t index,
      ExceptionState& exception_state) override;
  uint32_t GetStaticVariableCount(GPU::ShaderType type,
                                  ExceptionState& exception_state) override;
  void InitializeStaticSRBResources(scoped_refptr<GPUResourceBinding> srb,
                                    ExceptionState& exception_state) override;
  void CopyStaticResources(scoped_refptr<GPUPipelineSignature> dst,
                           ExceptionState& exception_state) override;
  bool IsCompatibleWith(scoped_refptr<GPUPipelineSignature> other,
                        ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.PipelineSignature"; }

  Diligent::RefCntAutoPtr<Diligent::IPipelineResourceSignature> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_PIPELINE_SIGNATURE_IMPL_H_

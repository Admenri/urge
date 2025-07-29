// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_SHADER_IMPL_H_
#define CONTENT_GPU_SHADER_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Shader.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpushader.h"

namespace content {

class ShaderImpl : public GPUShader, public EngineObject, public Disposable {
 public:
  ShaderImpl(ExecutionContext* context, Diligent::IShader* object);
  ~ShaderImpl() override;

  ShaderImpl(const ShaderImpl&) = delete;
  ShaderImpl& operator=(const ShaderImpl&) = delete;

  Diligent::IShader* AsRawPtr() const { return object_; }

 protected:
  // GPUShader interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  uint32_t GetResourceCount(ExceptionState& exception_state) override;
  scoped_refptr<GPUShaderResourceDesc> GetResourceDesc(
      uint32_t index,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUShaderCodeBufferDesc> GetConstantBufferDesc(
      uint32_t index,
      ExceptionState& exception_state) override;
  std::string GetBytecode(ExceptionState& exception_state) override;
  GPU::ShaderStatus GetStatus(bool wait_for_completion,
                              ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.Shader"; }

  Diligent::RefCntAutoPtr<Diligent::IShader> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_SHADER_IMPL_H_

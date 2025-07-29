// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_RESOURCE_VARIABLE_IMPL_H_
#define CONTENT_GPU_RESOURCE_VARIABLE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/ShaderResourceVariable.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpuresourcevariable.h"

namespace content {

class ResourceVariableImpl : public GPUResourceVariable,
                             public EngineObject,
                             public Disposable {
 public:
  ResourceVariableImpl(ExecutionContext* context,
                       Diligent::IShaderResourceVariable* object);
  ~ResourceVariableImpl() override;

  ResourceVariableImpl(const ResourceVariableImpl&) = delete;
  ResourceVariableImpl& operator=(const ResourceVariableImpl&) = delete;

  Diligent::IShaderResourceVariable* AsRawPtr() const { return object_; }

 protected:
  // GPUResourceVariable interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void Set(uint64_t device_object,
           GPU::SetShaderResourceFlags flags,
           ExceptionState& exception_state) override;
  void SetArray(const std::vector<uint64_t>& device_objects,
                uint32_t first_element,
                GPU::SetShaderResourceFlags flags,
                ExceptionState& exception_state) override;
  void SetBufferRange(uint64_t device_object,
                      uint64_t offset,
                      uint64_t size,
                      uint32_t array_index,
                      GPU::SetShaderResourceFlags flags,
                      ExceptionState& exception_state) override;
  GPU::ShaderResourceVariableType GetType(
      ExceptionState& exception_state) override;
  scoped_refptr<GPUShaderResourceDesc> GetResourceDesc(
      ExceptionState& exception_state) override;
  uint32_t GetIndex(ExceptionState& exception_state) override;
  uint64_t Get(uint32_t array_index, ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.ResourceVariable"; }

  Diligent::RefCntAutoPtr<Diligent::IShaderResourceVariable> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_RESOURCE_VARIABLE_IMPL_H_

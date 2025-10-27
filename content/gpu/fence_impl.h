// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_FENCE_IMPL_H_
#define CONTENT_GPU_FENCE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Fence.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpufence.h"

namespace content {

class FenceImpl : public GPUFence, public EngineObject, public Disposable {
 public:
  FenceImpl(ExecutionContext* context, Diligent::IFence* object);
  ~FenceImpl() override;

  FenceImpl(const FenceImpl&) = delete;
  FenceImpl& operator=(const FenceImpl&) = delete;

  Diligent::IFence* AsRawPtr() const { return object_; }

 protected:
  // GPUFence interface
  URGE_DECLARE_DISPOSABLE;
  GPU::FenceType GetType(ExceptionState& exception_state) override;
  uint64_t GetCompletedValue(ExceptionState& exception_state) override;
  void Signal(uint64_t value, ExceptionState& exception_state) override;
  void Wait(uint64_t value, ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.Fence"; }

  Diligent::RefCntAutoPtr<Diligent::IFence> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_FENCE_IMPL_H_

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_COMMAND_QUEUE_IMPL_H_
#define CONTENT_GPU_COMMAND_QUEUE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/CommandQueue.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpucommandqueue.h"

namespace content {

class CommandQueueImpl : public GPUCommandQueue,
                         public EngineObject,
                         public Disposable {
 public:
  CommandQueueImpl(ExecutionContext* context, Diligent::ICommandQueue* object);
  ~CommandQueueImpl() override;

  CommandQueueImpl(const CommandQueueImpl&) = delete;
  CommandQueueImpl& operator=(const CommandQueueImpl&) = delete;

  Diligent::ICommandQueue* AsRawPtr() const { return object_; }

 protected:
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  uint64_t GetNextFenceValue(ExceptionState& exception_state) override;
  uint64_t GetCompletedFenceValue(ExceptionState& exception_state) override;
  uint64_t WaitForIdle(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "GPU.CommandQueue"; }

  Diligent::ICommandQueue* object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_COMMAND_QUEUE_IMPL_H_

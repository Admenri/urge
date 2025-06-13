// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_COMMAND_LIST_IMPL_H_
#define CONTENT_GPU_COMMAND_LIST_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/CommandList.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gpucommandlist.h"

namespace content {

class CommandListImpl : public GPUCommandList,
                        public EngineObject,
                        public Disposable {
 public:
  CommandListImpl(ExecutionContext* context, Diligent::ICommandList* object);
  ~CommandListImpl() override;

  CommandListImpl(const CommandListImpl&) = delete;
  CommandListImpl& operator=(const CommandListImpl&) = delete;

  Diligent::ICommandList* AsRawPtr() const { return object_; }

 protected:
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "GPU.CommandList"; }

  Diligent::RefCntAutoPtr<Diligent::ICommandList> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_COMMAND_LIST_IMPL_H_

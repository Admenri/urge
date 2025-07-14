// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/command_list_impl.h"

#include "content/context/execution_context.h"

namespace content {

CommandListImpl::CommandListImpl(ExecutionContext* context,
                                 Diligent::ICommandList* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(CommandListImpl);

void CommandListImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content

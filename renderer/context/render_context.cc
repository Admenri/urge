// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/context/render_context.h"

namespace renderer {

RenderContext::RenderContext(
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context)
    : context_(context), scissor_(ScissorController::Create(context)) {}

RenderContext::~RenderContext() = default;

}  // namespace renderer

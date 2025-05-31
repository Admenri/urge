// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/context/scissor_controller.h"

namespace renderer {

base::OwnedPtr<ScissorController> ScissorController::Create(
    Diligent::IDeviceContext* context) {
  return base::MakeOwnedPtr<ScissorController>(context);
}

ScissorController::ScissorController(Diligent::IDeviceContext* context)
    : context_(context) {
  stack_.push(base::Rect());
}

ScissorController::~ScissorController() = default;

void ScissorController::Push(const base::Rect& scissor) {
  stack_.push(scissor);
  Apply(scissor);
}

void ScissorController::Pop() {
  stack_.pop();
  Apply(stack_.top());
}

base::Rect ScissorController::Current() {
  return current_;
}

void ScissorController::Apply(const base::Rect& scissor) {
  current_ = scissor;

  if (scissor.width && scissor.height) {
    Diligent::Rect render_scissor;
    render_scissor.left = scissor.x;
    render_scissor.top = scissor.y;
    render_scissor.right = scissor.x + scissor.width;
    render_scissor.bottom = scissor.y + scissor.height;
    context_->SetScissorRects(1, &render_scissor, UINT32_MAX, UINT32_MAX);
  }
}

}  // namespace renderer

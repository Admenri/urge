// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_CONTEXT_SCISSOR_CONTROLLER_H_
#define RENDERER_CONTEXT_SCISSOR_CONTROLLER_H_

#include <memory>
#include <stack>

#include "Graphics/GraphicsEngine/interface/DeviceContext.h"

#include "base/math/rectangle.h"
#include "base/memory/allocator.h"

namespace renderer {

class ScissorController {
 public:
  ~ScissorController();

  ScissorController(const ScissorController&) = delete;
  ScissorController& operator=(const ScissorController&) = delete;

  static base::OwnedPtr<ScissorController> Create(
      Diligent::IDeviceContext* context);

  void Push(const base::Rect& scissor);
  void Pop();

  base::Rect Current();
  void Apply(const base::Rect& scissor);

 private:
  friend struct base::Allocator;
  ScissorController(Diligent::IDeviceContext* context);

  Diligent::IDeviceContext* context_;
  base::Rect current_;
  base::Stack<base::Rect> stack_;
};

}  // namespace renderer

#endif  //! RENDERER_CONTEXT_SCISSOR_CONTROLLER_H_

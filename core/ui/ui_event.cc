// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_event.h"

#include "core/ui/ui_state.h"
namespace urge {
UIEvent::UIEvent(std::string type, Node* node, int64_t pointer, glm::vec2 point)
    : target(node),
      type_(std::move(type)),
      pointer_id_(pointer),
      position_(point) {
  if (node && node->ui_state()) target_id_ = node->ui_state()->Id();
}
RefPtr<Vector2> UIEvent::Position() const {
  return MakeRefCounted<Vector2>(position_.x, position_.y);
}
RefPtr<Vector2> UIEvent::LocalPosition() const {
  return MakeRefCounted<Vector2>(local_position.x, local_position.y);
}
}  // namespace urge

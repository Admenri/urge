// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once
#include "core/scene/node.h"
namespace urge {
URGE_BINDING()
class UIEvent : public Object {
 public:
  UIEvent(std::string type, Node* target, int64_t pointer_id = -1,
          glm::vec2 position = glm::vec2(0));
  URGE_BINDING()
  std::string Type() const { return type_; }
  URGE_BINDING()
  int64_t TargetId() const { return target_id_; }
  URGE_BINDING()
  int64_t PointerId() const { return pointer_id_; }
  URGE_BINDING()
  RefPtr<Vector2> Position() const;
  URGE_BINDING()
  RefPtr<Vector2> LocalPosition() const;
  URGE_BINDING()
  void StopPropagation() { stopped_ = true; }
  URGE_BINDING()
  bool Stopped() const { return stopped_; }
  RefPtr<Node> target;
  glm::vec2 local_position{0};

 private:
  std::string type_;
  int64_t target_id_ = 0, pointer_id_ = -1;
  glm::vec2 position_;
  bool stopped_ = false;
};
}  // namespace urge

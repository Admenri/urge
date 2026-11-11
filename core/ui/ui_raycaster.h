// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once
#include "core/scene/node.h"
#include "core/ui/ui_math.h"
namespace urge {
URGE_BINDING()
class UIRaycastHit : public Object {
 public:
  URGE_BINDING()
  int64_t TargetId() const;
  URGE_BINDING()
  RefPtr<Node> Target() const { return target; }
  URGE_BINDING()
  RefPtr<Vector3> Position() const;
  URGE_BINDING()
  RefPtr<Vector3> LocalPosition() const;
  URGE_BINDING()
  RefPtr<Vector2> UV() const;
  URGE_BINDING()
  float Distance() const { return distance; }
  RefPtr<Node> target;
  glm::vec3 position{0}, local_position{0};
  glm::vec2 uv{0};
  float distance = 0;
};
URGE_BINDING()
class UIRaycaster : public Object {
 public:
  URGE_BINDING()
  UIRaycaster(MaybePtr<Node> root = nullptr);
  UIRaycaster(Node* root, bool internal);
  URGE_BINDING()
  RefPtr<UIRaycastHit> RaycastScreen(float x, float y);
  URGE_BINDING()
  RefPtr<UIRaycastHit> RaycastScreen(float x, float y, int32_t width,
                                     int32_t height);
  URGE_BINDING()
  RefPtr<UIRaycastHit> Raycast(RefPtr<Vector3> origin,
                               RefPtr<Vector3> direction,
                               float max_distance = 100000.0f);
  URGE_BINDING()
  ATTR(bool, SortByDistance) {
    if (value) sort_by_distance_ = *value;
    return sort_by_distance_;
  }
  void Detach() {
    root_ = nullptr;
    owned_root_.reset();
  }
  RefPtr<UIRaycastHit> Cast(const ui_math::Ray* ray, const glm::vec2* point,
                            const glm::vec2& size);

 private:
  Node* root_ = nullptr;
  RefPtr<Node> owned_root_;
  bool sort_by_distance_ = false;
};
}  // namespace urge

// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
namespace urge {
URGE_BINDING()
class UIRectTransform : public Object {
 public:
  URGE_BINDING()
  UIRectTransform() = default;
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, Size) {
    if (value.has_value()) size_ = *value;
    return size_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, AnchorMin) {
    if (value.has_value()) anchor_min_ = *value;
    return anchor_min_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, AnchorMax) {
    if (value.has_value()) anchor_max_ = *value;
    return anchor_max_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, Pivot) {
    if (value.has_value()) pivot_ = *value;
    return pivot_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, MinSize) {
    if (value.has_value()) min_size_ = *value;
    return min_size_;
  }
  URGE_BINDING()
  ATTR(RefPtr<Vector2>, MaxSize) {
    if (value.has_value()) max_size_ = *value;
    return max_size_;
  }
  URGE_BINDING()
  ATTR(bool, ExpandWidth) {
    if (value.has_value()) expand_width_ = *value;
    return expand_width_;
  }
  URGE_BINDING()
  ATTR(bool, ExpandHeight) {
    if (value.has_value()) expand_height_ = *value;
    return expand_height_;
  }
  URGE_BINDING()
  ATTR(bool, IgnoreLayout) {
    if (value.has_value()) ignore_layout_ = *value;
    return ignore_layout_;
  }
  URGE_BINDING()
  RefPtr<Vector2> ResolvedSize() const;
  void Resolve(const glm::vec4& parent_rect);
  glm::vec4 rect() const;
  glm::mat4 matrix() const;
  void ClearLayout() { driven = false; }
  bool driven = false;
  glm::vec2 driven_position{0}, driven_size{0};
  glm::vec2 resolved_size{160, 40}, resolved_position{0};
  RefPtr<Vector2> size_ = MakeRefCounted<Vector2>(160.0f, 40.0f);
  RefPtr<Vector2> anchor_min_ = MakeRefCounted<Vector2>(0.0f, 0.0f);
  RefPtr<Vector2> anchor_max_ = MakeRefCounted<Vector2>(0.0f, 0.0f);
  RefPtr<Vector2> pivot_ = MakeRefCounted<Vector2>(0.0f, 0.0f);
  RefPtr<Vector2> min_size_ = MakeRefCounted<Vector2>(0.0f, 0.0f);
  RefPtr<Vector2> max_size_ = MakeRefCounted<Vector2>(8192.0f, 8192.0f);
  bool expand_width_ = false;
  bool expand_height_ = false;
  bool ignore_layout_ = false;
};
}  // namespace urge

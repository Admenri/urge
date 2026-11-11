// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_rect_transform.h"

#include <algorithm>
#include <cmath>

#include "glm/gtc/matrix_transform.hpp"
namespace urge {
void UIRectTransform::Resolve(const glm::vec4& parent) {
  const glm::vec2 extent(parent.z, parent.w);
  const auto minimum = anchor_min_->data, maximum = anchor_max_->data;
  resolved_size =
      driven ? driven_size : extent * (maximum - minimum) + size_->data;
  for (int i = 0; i < 2; ++i) {
    const float lower = std::max(0.0f, min_size_->data[i]);
    const float upper = std::max(lower, max_size_->data[i]);
    resolved_size[i] = std::isfinite(resolved_size[i])
                           ? std::clamp(resolved_size[i], lower, upper)
                           : lower;
  }
  resolved_position =
      driven ? driven_position
             : glm::vec2(parent.x, parent.y) +
                   extent * (minimum + (maximum - minimum) * pivot_->data);
}
glm::vec4 UIRectTransform::rect() const {
  return glm::vec4(-resolved_size * pivot_->data, resolved_size);
}
glm::mat4 UIRectTransform::matrix() const {
  return glm::translate(glm::mat4(1), glm::vec3(resolved_position, 0));
}
RefPtr<Vector2> UIRectTransform::ResolvedSize() const {
  return MakeRefCounted<Vector2>(resolved_size.x, resolved_size.y);
}
}  // namespace urge

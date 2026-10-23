// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <cstdint>
#include <vector>

#include "glm/glm.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"

namespace urge {

URGE_BINDING()
class Node3D : public Object {
 public:
  URGE_BINDING()
  Node3D();
  ~Node3D() override;

  URGE_BINDING()
  ATTR(RefPtr<Vector3>, Position) {
    if (value.has_value()) {
      if (*value)
        position_ = *value;
      return std::nullopt;
    }
    return position_;
  }

  URGE_BINDING()
  ATTR(RefPtr<Vector4>, Rotation) {
    if (value.has_value()) {
      if (*value)
        rotation_ = *value;
      return std::nullopt;
    }
    return rotation_;
  }

  URGE_BINDING()
  ATTR(RefPtr<Vector3>, Scale) {
    if (value.has_value()) {
      if (*value)
        scale_ = *value;
      return std::nullopt;
    }
    return scale_;
  }

  URGE_BINDING()
  ATTR(bool, Visible) {
    if (value.has_value())
      visible_ = *value;
    return visible_;
  }

  URGE_BINDING(Name : "layers")
  ATTR(uint32_t, Layers) {
    if (value.has_value())
      layers_ = *value;
    return layers_;
  }

  URGE_BINDING(Name : "parent")
  RefPtr<Node3D> GetParent() const { return parent_; }
  URGE_BINDING(Name : "parent=")
  void SetParent(RefPtr<Node3D> value);

  URGE_BINDING()
  void AddChild(RefPtr<Node3D> child);
  URGE_BINDING()
  void RemoveChild(RefPtr<Node3D> child);
  URGE_BINDING()
  int32_t ChildCount() const { return static_cast<int32_t>(children_.size()); }
  URGE_BINDING()
  RefPtr<Node3D> ChildAt(int32_t index) const;

  glm::mat4 local_matrix() const;
  glm::mat4 world_matrix() const;

  URGE_BINDING(Name : "world_matrix")
  std::vector<float> GetWorldMatrix();

  glm::vec3 local_position() const { return position_->data; }
  glm::vec4 local_rotation() const { return rotation_->data; }
  glm::vec3 local_scale() const { return scale_->data; }

  void SetLocalPosition(const glm::vec3& value) { position_->data = value; }
  void SetLocalRotation(const glm::vec4& value) { rotation_->data = value; }
  void SetLocalScale(const glm::vec3& value) { scale_->data = value; }

  const std::vector<RefPtr<Node3D>>& children() const { return children_; }
  bool visible() const { return visible_; }
  uint32_t layers() const { return layers_; }

 private:
  void RemoveChildRaw(Node3D* child);

  RefPtr<Vector3> position_;
  RefPtr<Vector4> rotation_;
  RefPtr<Vector3> scale_;
  bool visible_ = true;
  uint32_t layers_ = 1;

  Node3D* parent_ = nullptr;
  std::vector<RefPtr<Node3D>> children_;
};

}

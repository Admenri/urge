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

#include "core/scene3d/node3d.h"

#include <algorithm>
#include <utility>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

namespace urge {

Node3D::Node3D()
    : position_(MakeRefCounted<Vector3>(0.0f)),
      rotation_(MakeRefCounted<Vector4>(0.0f, 0.0f, 0.0f, 1.0f)),
      scale_(MakeRefCounted<Vector3>(1.0f)) {}

Node3D::~Node3D() {
  for (const RefPtr<Node3D>& child : children_) {
    if (child->parent_ == this)
      child->parent_ = nullptr;
  }
  children_.clear();

  if (parent_) {
    parent_->RemoveChildRaw(this);
    parent_ = nullptr;
  }
}

void Node3D::SetParent(RefPtr<Node3D> value) {
  if (parent_ == value.get())
    return;

  if (parent_)
    parent_->RemoveChildRaw(this);

  parent_ = value.get();
  if (parent_)
    parent_->children_.push_back(this);
}

void Node3D::AddChild(RefPtr<Node3D> child) {
  if (!child)
    return;
  child->SetParent(this);
}

void Node3D::RemoveChild(RefPtr<Node3D> child) {
  if (!child)
    return;
  if (child->parent_ == this)
    child->parent_ = nullptr;
  RemoveChildRaw(child.get());
}

RefPtr<Node3D> Node3D::ChildAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(children_.size()))
    return nullptr;
  return children_[index];
}

void Node3D::RemoveChildRaw(Node3D* child) {
  children_.erase(std::remove_if(children_.begin(), children_.end(),
                                 [child](const RefPtr<Node3D>& item) {
                                   return item.get() == child;
                                 }),
                  children_.end());
}

glm::mat4 Node3D::local_matrix() const {
  glm::mat4 result = glm::translate(glm::mat4(1.0f), position_->data);
  result *= glm::mat4_cast(glm::quat(rotation_->data.w, rotation_->data.x,
                                     rotation_->data.y, rotation_->data.z));
  return glm::scale(result, scale_->data);
}

glm::mat4 Node3D::world_matrix() const {
  const glm::mat4 local = local_matrix();
  return parent_ ? parent_->world_matrix() * local : local;
}

std::vector<float> Node3D::GetWorldMatrix() {
  const glm::mat4 matrix = world_matrix();

  std::vector<float> result(16);
  for (size_t i = 0; i < 16; ++i)
    result[i] = matrix[i / 4][i % 4];
  return result;
}

}

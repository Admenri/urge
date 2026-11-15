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

#include "core/common/persistent.h"
#include "core/scene3d/node3d.h"

#include <algorithm>
#include <utility>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

namespace urge {

void Node3D::Trace(GCVisitor& visitor) const {
  Object::Trace(visitor);
  visitor.Trace(position_);
  visitor.Trace(rotation_);
  visitor.Trace(scale_);
  visitor.Trace(children_);
}

Node3D::Node3D()
    : position_(MakeGarbageCollected<Vector3>(0.0f)),
      rotation_(MakeGarbageCollected<Vector4>(0.0f, 0.0f, 0.0f, 1.0f)),
      scale_(MakeGarbageCollected<Vector3>(1.0f)) {}

Node3D::~Node3D() = default;

void Node3D::PrepareForCollection() {
  Object::PrepareForCollection();
  // Weak slots targeting garbage have already been cleared. Always process
  // our children while the entire garbage batch is still allocated.
  for (const auto& child : children_) {
    if (child->parent_.get() == this)
      child->parent_.reset();
  }
  children_.clear();

  if (parent_)
    parent_->RemoveChildRaw(this);
  parent_.reset();
}

void Node3D::SetParent(Node3D* value) {
  if (parent_.get() == value)
    return;
  for (Node3D* ancestor = value; ancestor; ancestor = ancestor->parent_.get())
    if (ancestor == this)
      return;

  // Keep the child rooted while changing the old and new parent edges.
  Persistent<Node3D> keep_alive(this);

  if (parent_)
    parent_->RemoveChildRaw(this);

  parent_ = value;
  if (parent_)
    parent_->children_.push_back(this);
}

void Node3D::AddChild(Node3D* child) {
  if (!child)
    return;
  child->SetParent(this);
}

void Node3D::RemoveChild(Node3D* child) {
  if (!child)
    return;
  if (child->parent_.get() == this)
    child->parent_ = nullptr;
  RemoveChildRaw(child);
}

Node3D* Node3D::ChildAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(children_.size()))
    return nullptr;
  return children_[index].get();
}

std::vector<Node3D*> Node3D::NativeChildrenSnapshot() const {
  std::vector<Node3D*> result;
  result.reserve(children_.size());
  for (const auto& child : children_)
    result.emplace_back(child.get());
  return result;
}

void Node3D::RemoveChildRaw(Node3D* child) {
  children_.erase(
      std::remove_if(children_.begin(), children_.end(),
                     [child](const auto& item) { return item.get() == child; }),
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

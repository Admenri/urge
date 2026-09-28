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

#include "core/node.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include "core/graphics.h"

namespace urge {

Node::Node()
    : self_([&](DrawParam param) { ExecutePrepare(param); },
            [&](DrawParam param) { ExecuteRendering(param); },
            ZValue()),
      world_root_(true) {}

Node::Node(RefPtr<Node> parent, const ZValue& z)
    : self_([&](DrawParam param) { ExecutePrepare(param); },
            [&](DrawParam param) { ExecuteRendering(param); },
            z),
      world_root_(false) {
  Attr_Parent(parent);
}

Node::~Node() {
  Disposable::Dispose();
}

void Node::Render(RefPtr<Bitmap> target, RefPtr<Color> clear) {}

ATTR_DEF(Node, int32_t, Z) {
  if (value.has_value()) {
    ZValue old = self_.order();
    old.value = *value;
    self_.SortWith(old);
    return std::nullopt;
  } else {
    return self_.order().value;
  }
}

ATTR_DEF(Node, bool, Visible) {
  if (value.has_value()) {
    self_.visible() = *value;
    return std::nullopt;
  } else {
    return self_.visible();
  }
}

ATTR_DEF(Node, RefPtr<Node>, Parent) {
  if (value.has_value()) {
    if (world_root_)
      throw Exception(Exception::kRGSSError, "cannot set world root parent.");

    parent_ = *value;
    DrawableSet* default_parent = &Graphics::Get().root()->children_;
    DrawableSet* parent = parent_ ? &parent_->children_ : default_parent;
    self_.SetParent(parent);
    return std::nullopt;
  } else {
    return parent_;
  }
}

ATTR_DEF(Node, RefPtr<Vector3>, Position) {
  if (value) {
    if (*value) {
      transform_.position = *value;
      RebuildModelTransform();
    }
    return std::nullopt;
  } else {
    return transform_.position;
  }
}

ATTR_DEF(Node, RefPtr<Vector4>, Quaternion) {
  if (value) {
    if (*value) {
      transform_.quaternion = *value;
      RebuildModelTransform();
    }
    return std::nullopt;
  } else {
    return transform_.quaternion;
  }
}

ATTR_DEF(Node, RefPtr<Vector3>, Scale) {
  if (value) {
    if (*value) {
      transform_.scale = *value;
      RebuildModelTransform();
    }
    return std::nullopt;
  } else {
    return transform_.scale;
  }
}

void Node::Render(DrawParam param, std::optional<Vec4> clear) {
  ExecutePrepare(param);
  ExecuteRendering(param);
}

void Node::DisposeObject() {
  self_.RemoveFromList();
  parent_.reset();
}

void Node::ExecutePrepare(DrawParam param) {
  const auto current_model = param->model.top();
  param->model.push(current_model * transform_.local);
  {
    transform_.world = param->model.top();
    Prepare(param);
    children_.DispatchPrepare(param);
  }
  param->model.pop();
}

void Node::ExecuteRendering(DrawParam param) {
  DoDraw(param);
  children_.DispatchDraw(param);
  PostDraw(param);
}

void Node::RebuildModelTransform() {
  auto pos = transform_.position->data;
  auto quat = transform_.quaternion->data;
  auto scale = transform_.scale->data;
  auto model = Mat4x4(1.0f);

  model = glm::translate(model, pos);
  model *= glm::mat4_cast(glm::quat(quat.w, quat.x, quat.y, quat.z));
  model = glm::scale(model, scale);
  transform_.local = model;
}

}  // namespace urge

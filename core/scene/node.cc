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

#include "core/scene/node.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include "core/graphics/graphics.h"
#include "core/render/primitive.h"
#include "core/render/render_stats.h"
#include "core/render/uniform.h"
#include "core/scene/sprite_batch.h"
#include "core/ui/ui.h"

namespace urge {

Node::Node()
    : self_([&](DrawParam param) { ExecutePrepare(param); },
            [&](DrawParam param) { ExecuteRendering(param); },
            ZValue()),
      world_root_(true) {
  self_.SetNode(this);
  children_.SetNode(this);
}

Node::Node(RefPtr<Node> parent, const ZValue& z)
    : self_([&](DrawParam param) { ExecutePrepare(param); },
            [&](DrawParam param) { ExecuteRendering(param); },
            z),
      world_root_(false) {
  self_.SetNode(this);
  children_.SetNode(this);
  Attr_Parent(parent);
}

Node::Node(RefPtr<Node> parent) : Node(parent, ZValue()) {}

Node::~Node() {
  Disposable::Dispose();
}

void Node::Render(RefPtr<Bitmap> target, RefPtr<Color> clear) {
  if (UI::TryGet())
    UI::Get().PrepareTree(this, glm::vec2(target->size()));
  auto encoder = GPUDevice::Get().device().CreateCommandEncoder(nullptr);

  DrawContext context = {};
  context.model.push(glm::mat4(1.0f));
  context.command = encoder;
  context.target = target;
  context.scene = target->scene_group();

  UniformManager& uniforms = UniformManager::Get();
  QuadVertexManager& quads = QuadVertexManager::Get();
  context.vertices = &quads.emitter();

  uniforms.BeginFrame();
  quads.BeginFrame();
  SpriteBatch::Get().BeginFrame();

  context.scissors.push(RectI(target->size()));
  ExecutePrepare(&context);

  uniforms.Flush();
  quads.Upload();
  SpriteBatch::Get().Flush();

  std::optional<glm::vec4> clear_color = std::nullopt;
  if (clear)
    clear_color = clear->Normalize();
  context.pass = target->BeginRendering(encoder, clear_color);
  {
    ExecuteRendering(&context);
    if (context.pass)
      FlushSpriteBatch(&context);
  }
  if (context.pass)
    context.pass.End();
  auto command_buffer = encoder.Finish(nullptr);
  CountSubmit(false);
  GPUDevice::Get().queue().Submit(1, &command_buffer);
  // Rendering changes the bitmap just like a drawing operation. Invalidate
  // pixel readback caches and consumers that cache this bitmap as a UI skin.
  target->TouchContent();
}

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
    // A cycle cannot be rendered or transformed. Preserve the previous parent.
    for (Node* ancestor = value->get(); ancestor;
         ancestor = ancestor->scene_parent())
      if (ancestor == this)
        return std::nullopt;
    parent_ = *value;
    DrawableSet* default_parent = Graphics::TryGet() && Graphics::Get().root()
        ? &Graphics::Get().root()->children_ : nullptr;
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

ATTR_DEF(Node, RefPtr<Camera>, Camera) {
  if (value.has_value()) {
    camera_ = *value;
    return std::nullopt;
  } else {
    return camera_;
  }
}

void Node::DisposeObject() {
  self_.SetParent(nullptr);
  parent_.reset();
}

void Node::ExecutePrepare(DrawParam param) {
  const auto current_model = param->model.top();
  param->model.push(current_model * local_matrix());
  {
    transform_.world = param->model.top();
    allow_do_draw_ = Prepare(param);
    param->model.push(transform_.world * child_matrix());
    children_.DispatchPrepare(param);
    param->model.pop();
    if (allow_do_draw_)
      PostPrepare(param);
  }
  param->model.pop();
}

std::vector<RefPtr<Node>> Node::Children() const {
  std::vector<RefPtr<Node>> result;
  for (Node* child : children_raw())
    result.emplace_back(child);
  return result;
}

glm::mat4 Node::local_matrix() const {
  // Vector objects are mutable through Ruby. Read their current values rather
  // than relying on attribute assignment to invalidate a cached matrix.
  const auto q = transform_.quaternion->data;
  return layout_matrix() *
      glm::translate(glm::mat4(1.0f), transform_.position->data) *
      glm::mat4_cast(glm::quat(q.w, q.x, q.y, q.z)) *
      glm::scale(glm::mat4(1.0f), transform_.scale->data);
}

glm::mat4 Node::resolved_world_matrix() const {
  Node* parent = scene_parent();
  return parent ? parent->resolved_world_matrix() * parent->child_matrix() *
                     local_matrix() : local_matrix();
}

std::vector<float> Node::WorldMatrix() const {
  const auto matrix = resolved_world_matrix();
  std::vector<float> result(16);
  for (int i = 0; i < 16; ++i)
    result[i] = matrix[i / 4][i % 4];
  return result;
}

bool Node::visible_in_tree() const {
  if (IsDisposed() || !self_.visible())
    return false;
  Node* parent = scene_parent();
  return !parent || parent->visible_in_tree();
}

RefPtr<Camera> Node::effective_camera() const {
  if (camera_)
    return camera_;
  Node* parent = scene_parent();
  return parent ? parent->effective_camera() : nullptr;
}

void Node::ExecuteRendering(DrawParam param) {
  const wgpu::BindGroup outer_scene = param->scene;
  if (camera_)
    param->scene = camera_->AcquireScene(param->target->size());

  allow_post_draw_ = allow_do_draw_ ? DoDraw(param) : false;
  children_.DispatchDraw(param);
  allow_post_draw_ ? PostDraw(param) : void();

  param->scene = outer_scene;
}

void Node::RebuildModelTransform() {
  auto pos = transform_.position->data;
  auto quat = transform_.quaternion->data;
  auto scale = transform_.scale->data;
  auto model = glm::mat4(1.0f);

  model = glm::translate(model, pos);
  model *= glm::mat4_cast(glm::quat(quat.w, quat.x, quat.y, quat.z));
  model = glm::scale(model, scale);
  transform_.local = model;
}

}

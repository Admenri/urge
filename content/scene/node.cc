// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/node.h"

#include <algorithm>

namespace content {

///
/// Node
///

// static
scoped_refptr<Node> Node::New(URGE_EXCEPTION) {
  return Object::Create<Node>();
}

Node::Node()
    : node_(this, nullptr, SortKey()),
      transform_(Object::Create<Transform>()),
      transform_dirty_(true),
      world_(nullptr) {
  auto transform_handler =
      base::BindRepeating(&Node::OnTransformChange, base::Unretained(this));
  transform_->set_change_handler(transform_handler);
}

Node::~Node() {
  Dispose();
}

void Node::SetupAsWorldRoot(World* world) {
  // Leave old world notify
  if (world_)
    OnLeaveWorld(world_);

  // Enter new world notify
  if (world)
    OnEnterWorld(world);

  // Setup self and children world
  world_ = world;
}

const glm::dmat4x4& Node::GetModelMatrix() {
  if (transform_dirty_) {
    transform_dirty_ = false;

    model_ = transform_->GetModelMatrix();
    if (parent_)
      model_ = parent_->GetModelMatrix() * model_;
  }

  return model_;
}

World* Node::GetWorld() {
  World* root_world = nullptr;
  for (Node* iter_node = this; iter_node != nullptr;
       iter_node = iter_node->parent_.get()) {
    root_world = iter_node->world_;

    if (root_world)
      break;
  }

  return root_world;
}

URGE_ATTRIBUTE_DEFINE(
    Node,
    Parent,
    scoped_refptr<Node>,
    { return parent_; },
    {
      if (parent_ == value)
        return;

      // Old world unbind
      if (World* root = GetWorld(); root)
        OnLeaveWorld(root);

      // Transform changing
      transform_dirty_ = true;
      parent_ = value;
      if (parent_) {
        node_.RebindController(&parent_->children_);
        OnTransformChange();
      }

      // New world bind
      if (World* root = GetWorld(); root)
        OnEnterWorld(root);
    });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Active,
    bool,
    { return node_.GetActive(); },
    {
      node_.SetActive(value);
      OnTransformChange();
    });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Order,
    int64_t,
    { return node_.GetSortKey()->weight[0]; },
    { node_.SetNodeSortWeight(value); });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Layer,
    uint32_t,
    { return layer_; },
    { layer_ = value; });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Name,
    std::string,
    { return name_; },
    { name_ = value; });

scoped_refptr<Transform> Node::GetTransform(URGE_EXCEPTION) {
  return transform_;
}

scoped_refptr<Node> Node::FirstChild(URGE_EXCEPTION) {
  auto* node = children_.head();
  return node ? static_cast<NodeLink<Node>*>(node)->self() : nullptr;
}

scoped_refptr<Node> Node::LastChild(URGE_EXCEPTION) {
  auto* node = children_.tail();
  return node ? static_cast<NodeLink<Node>*>(node)->self() : nullptr;
}

scoped_refptr<Node> Node::PreviousSibling(URGE_EXCEPTION) {
  auto* sibling = node_.GetPreviousSibling();
  return sibling ? sibling->self() : nullptr;
}

scoped_refptr<Node> Node::NextSibling(URGE_EXCEPTION) {
  auto* sibling = node_.GetNextSibling();
  return sibling ? sibling->self() : nullptr;
}

void Node::OnEnterWorld(World* new_world) {
  for (auto* it = children_.head(); it != children_.end(); it = it->next()) {
    auto* child = static_cast<NodeLink<Node>*>(it)->self();
    child->OnEnterWorld(new_world);
  }
}

void Node::OnLeaveWorld(World* old_world) {
  for (auto* it = children_.head(); it != children_.end(); it = it->next()) {
    auto* child = static_cast<NodeLink<Node>*>(it)->self();
    child->OnLeaveWorld(old_world);
  }
}

void Node::OnObjectRelease() {
  // World reference release

  // Scene graph dispose
  node_.DisposeNode();

  // Release parent referene
  parent_.reset();
}

void Node::OnTransformChange() {
  if (!node_.GetActive())
    return;

  // Set self transform dirty
  transform_dirty_ = true;

  // Set children transform dirty
  for (auto* it = children_.head(); it != children_.end(); it = it->next()) {
    auto* child = static_cast<NodeLink<Node>*>(it)->self();
    child->OnTransformChange();
  }
}

}  // namespace content

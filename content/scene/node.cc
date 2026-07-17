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
      root_node_(false),
      world_(nullptr) {
  auto transform_handler =
      base::BindRepeating(&Node::TransformChange, base::Unretained(this));
  transform_->set_change_handler(transform_handler);
}

Node::~Node() {
  // World reference release
  ForEachNode([&](Node* node) {
    node->OnLeaveWorld(node->world_);
    return false;
  });

  // Scene graph dispose
  node_.DisposeNode();

  // Release parent referene
  parent_.reset();
}

void Node::SetupWorld(World* new_world, World* old_world) {
  if (new_world == old_world)
    return;

  ForEachNode([&](Node* node) {
    if (old_world)
      node->OnLeaveWorld(old_world);
    if (new_world)
      node->OnEnterWorld(new_world);
    node->world_ = new_world;
    return false;
  });
}

void Node::ResetParent(scoped_refptr<Node> parent) {
  parent_ = parent;
  if (parent)
    node_.RebindController(&parent->children_);
  TransformChange();
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

URGE_ATTRIBUTE_DEFINE(
    Node,
    Parent,
    scoped_refptr<Node>,
    { return parent_; },
    {
      if (parent_ == value)
        return;

      // Disallow root set parent
      if (root_node_) {
        exception_state.Throw(ExceptionCode::CONTENT_ERROR,
                              "unable to reset the parent of root node.");
        return;
      }

      // World change
      SetupWorld(value ? value->world_ : nullptr, world_);

      // Transform changing and setup parent
      ResetParent(value);
    });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Active,
    bool,
    { return node_.GetActive(); },
    {
      if (node_.GetActive() == value)
        return;

      node_.SetActive(value);
      TransformChange();
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

void Node::ForEachNode(std::function<bool(Node*)> iter) {
  std::stack<Node*> stack;
  stack.push(this);

  while (!stack.empty()) {
    Node* node = stack.top();
    stack.pop();

    for (auto* it = node->children_.head(); it != node->children_.end();
         it = it->next()) {
      stack.push(static_cast<NodeLink<Node>*>(it)->self());
    }

    if (iter(node))
      break;
  }
}

void Node::TransformChange() {
  ForEachNode([](Node* node) {
    if (node->node_.GetActive()) {
      node->transform_dirty_ = true;
      return false;
    }
    return true;
  });
}

}  // namespace content

// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/node.h"

#include <algorithm>
#include <stack>

namespace content {

///
/// Node
///

// static
scoped_refptr<Node> Node::New(URGE_EXCEPTION) {
  return Object::Create<Node>();
}

Node::Node()
    : transform_(Object::Create<Transform>()),
      parent_(nullptr),
      active_(true),
      order_(0),
      layer_(0),
      world_(nullptr),
      root_node_(false),
      in_world_(false),
      transform_dirty_(true) {
  auto transform_handler =
      base::BindRepeating(&Node::TransformChange, base::Unretained(this));
  transform_->set_change_handler(transform_handler);
}

Node::~Node() {
  // Assure current node detached from parent:
  //  1. Parent hold the reference of children.
  //  2. When node detached from parent, we dont need to detach again.
}

void Node::SetupWorld(World* new_world, World* old_world) {
  if (new_world == old_world)
    return;

  ForEachNode([&](Node* node) {
    if (old_world)
      node->LeaveWorld(old_world);
    if (new_world)
      node->EnterWorld(new_world);
    node->world_ = new_world;
    return false;
  });
}

void Node::ResetParent(Node* parent) {
  if (parent_ == parent)
    return;

  // Remove from old parent's children
  if (parent_) {
    auto& old_children = parent_->children_;
    auto it =
        std::find_if(old_children.begin(), old_children.end(),
                     [this](const auto& ref) { return ref.get() == this; });
    if (it != old_children.end())
      old_children.erase(it);
  }

  // Set raw reference
  parent_ = parent;

  // Add to new parent's children and resort
  if (parent) {
    parent->children_.push_back(scoped_refptr<Node>(this));
    parent->ResortChildren(this);
  }

  // Transform notification
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
      if (parent_ == value.get())
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
      ResetParent(value.get());
    });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Active,
    bool,
    { return active_; },
    {
      if (active_ == value)
        return;

      // World change
      if (value) {
        // Active
        ForEachNode([&](Node* node) {
          node->EnterWorld(node->world_);
          return false;
        });
      } else {
        // Inactive
        ForEachNode([&](Node* node) {
          node->LeaveWorld(node->world_);
          return false;
        });
      }

      active_ = value;
      TransformChange();
    });

URGE_ATTRIBUTE_DEFINE(
    Node,
    Order,
    int64_t,
    { return order_; },
    {
      order_ = value;
      if (parent_)
        parent_->ResortChildren(this);
    });

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

uint32_t Node::GetChildrenCount(URGE_EXCEPTION) {
  return children_.size();
}

scoped_refptr<Node> Node::GetChildAt(uint32_t index, URGE_EXCEPTION) {
  if (index >= 0 && index < children_.size())
    return children_[index];
  return nullptr;
}

void Node::ForEachNode(std::function<bool(Node*)> iter) {
  std::stack<Node*> stack;
  stack.push(this);

  while (!stack.empty()) {
    Node* node = stack.top();
    stack.pop();

    for (auto& it : children_)
      stack.push(it.get());

    if (iter(node))
      break;
  }
}

void Node::EnterWorld(World* world) {
  if (!in_world_) {
    OnEnterWorld(world);
    in_world_ = true;
  }
}

void Node::LeaveWorld(World* world) {
  if (in_world_) {
    OnLeaveWorld(world);
    in_world_ = false;
  }
}

void Node::ResortChildren(Node* target) {
  if (children_.size() <= 1)
    return;

  if (!target) {
    // Full sort: sort all children by order_ ascending
    std::sort(
        children_.begin(), children_.end(),
        [](const auto& a, const auto& b) { return a->order_ < b->order_; });
    return;
  }

  // Bubble target to its correct position by swapping with neighbors
  auto it =
      std::find_if(children_.begin(), children_.end(),
                   [target](const auto& ref) { return ref.get() == target; });
  if (it == children_.end())
    return;

  size_t idx = std::distance(children_.begin(), it);

  // Bubble left while target->order_ < left neighbor's order_
  while (idx > 0 && target->order_ < children_[idx - 1]->order_) {
    std::swap(children_[idx - 1], children_[idx]);
    --idx;
  }

  // Bubble right while target->order_ > right neighbor's order_
  while (idx + 1 < children_.size() &&
         target->order_ > children_[idx + 1]->order_) {
    std::swap(children_[idx], children_[idx + 1]);
    ++idx;
  }
}

void Node::TransformChange() {
  ForEachNode([](Node* node) {
    if (node->active_) {
      node->transform_dirty_ = true;
      return false;
    }
    return true;
  });
}

}  // namespace content

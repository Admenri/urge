// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <optional>
#include <stack>
#include <typeindex>

#include "base/debug/logging.h"
#include "base/template/linked_list.h"

namespace content {

// Using polymorphic memory resources based map for drawable sort,
// the creation of drawable and texture query will cost some allocation time of
// cpu. Consider std::pmr:: domain method.

// DrawController <-> Drawable
// Engine use task-driver renderable structure:
// notify composite -> enumerate children -> called repeatable closure
// Graphics, Viewport hosted the controller, Sprite, Plane, Window, Tilemap...
// inhert drawable.

// A drawable based child render process view:
// <before render> <on render> <after render>
// 1. <before render>:
//   controller parameters:
//     a. renderDevice
//     b. no render target
// 2. <on render>: execute the render command
//   controller parameters:
//     a. renderDevice
//     b. immutable renderPass on <screenBuffer>
//     c. allow screenBuffer texture access
// 3. <after render>: after all render event notification
//     a. renderDevice
//     b. allow screenBuffer texture access

template <typename Ty>
class NodeLinkController;

class TypeID {
 public:
  template <typename Ty>
  static std::size_t Of() {
    static std::byte buffer;
    return reinterpret_cast<std::size_t>(&buffer);
  }
};

// Multi weight sorted key data structure.
class SortKey {
 public:
  int64_t weight[3];

  SortKey();
  SortKey(int64_t key1);
  SortKey(int64_t key1, int64_t key2);
  SortKey(int64_t key1, int64_t key2, int64_t key3);

  // Compare self with other for displaying under other object.
  inline bool operator<(const SortKey& other) const {
    for (int32_t i = 0; i < 3; ++i)
      if (weight[i] != other.weight[i])
        return weight[i] < other.weight[i];

    return false;
  }

  inline bool operator>(const SortKey& other) const {
    for (int32_t i = 0; i < 3; ++i)
      if (weight[i] != other.weight[i])
        return weight[i] > other.weight[i];

    return false;
  }
};

// Drawable child node,
// expect to be set in class as a node variable.
template <typename Ty>
class NodeLink final : public base::LinkNode<NodeLink<Ty>> {
 public:
  NodeLink(Ty* self,
           NodeLinkController<Ty>* controller,
           const SortKey& default_key,
           bool active = true)
      : self_(self),
        controller_(controller),
        key_(default_key),
        active_(active) {
    if (controller_) {
      controller_->InsertChildNodeInternal(this);
    }
  }

  ~NodeLink() { DisposeNode(); }

  NodeLink(NodeLink&&) = delete;
  NodeLink(const NodeLink&) = delete;
  NodeLink& operator=(const NodeLink&) = delete;

  Ty* self() { return self_; }
  Ty* self() const { return self_; }

  // Rebind parent controller
  void RebindController(NodeLinkController<Ty>* controller) {
    if (controller == controller_)
      return;

    // Setup new parent controller
    controller_ = controller;

    // Rebind orderly node link
    base::LinkNode<NodeLink>::RemoveFromList();
    controller_->InsertChildNodeInternal(this);
  }

  // Dispose drawable node, and close the connection with controller
  void DisposeNode() {
    base::LinkNode<NodeLink>::RemoveFromList();
    controller_ = nullptr;
  }

  // Set current node visibility
  void SetActive(bool active) { active_ = active; }
  bool GetActive() const { return active_; }

  // Set the key for requirement of the controller's map sort.
  // For the same weight scene, it can set multi weight value for sort.
  // Current support 3 weights for map sort.
  // No sort action after setting the weight.
  void SetNodeSortWeight(int64_t weight1) {
    if (!controller_ || key_.weight[0] == weight1) {
      return;
    }

    key_.weight[0] = weight1;
    ReorderDrawableNodeInternal();
  }

  void SetNodeSortWeight(int64_t weight1, int64_t weight2) {
    if (!controller_ ||
        (key_.weight[0] == weight1 && key_.weight[1] == weight2)) {
      return;
    }

    key_.weight[0] = weight1;
    key_.weight[1] = weight2;
    ReorderDrawableNodeInternal();
  }

  void SetNodeSortWeight(int64_t weight1, int64_t weight2, int64_t weight3) {
    if (!controller_ ||
        (key_.weight[0] == weight1 && key_.weight[1] == weight2 &&
         key_.weight[2] == weight3)) {
      return;
    }

    key_.weight[0] = weight1;
    key_.weight[1] = weight2;
    key_.weight[2] = weight3;
    ReorderDrawableNodeInternal();
  }

  // Get sort key
  SortKey* GetSortKey() { return &key_; }

  // Get current controller
  NodeLinkController<Ty>* GetController() const { return controller_; }

  // Get linked node (previous / next)
  NodeLink* GetPreviousSibling() {
    auto* node = base::LinkNode<NodeLink>::previous();
    return node ? static_cast<NodeLink<Ty>*>(node) : nullptr;
  }
  NodeLink* GetNextSibling() {
    auto* node = base::LinkNode<NodeLink>::next();
    return node ? static_cast<NodeLink<Ty>*>(node) : nullptr;
  }

 private:
  friend class NodeLinkController<Ty>;

  void ReorderDrawableNodeInternal() {
    DCHECK(controller_);

    // Fetch associate nodes
    auto* previous_node = GetPreviousSibling();
    auto* next_node = GetNextSibling();

    if (!previous_node && !next_node)
      return;

    // Check node reordering direction
    bool ascending_order =
        !previous_node || (next_node ? (next_node->key_ < key_) : false);
    bool descending_order =
        !next_node || (previous_node ? (previous_node->key_ > key_) : false);

    if (!ascending_order && !descending_order) {
      // No need to reorder, already in correct position
      return;
    }

    // min -> max
    if (ascending_order) {
      NodeLink* current_node = next_node;
      DCHECK(current_node);

      // Remove from current list, and insert before the first node with a
      // greater
      base::LinkNode<NodeLink>::RemoveFromList();
      while (current_node != controller_->children_list_.end()) {
        if (current_node->key_ > key_)
          return base::LinkNode<NodeLink>::InsertBefore(current_node);

        current_node = current_node->GetNextSibling();
      }

      // Reached the end of the list, append to the end.
      return controller_->children_list_.Append(this);
    }

    // max -> min
    if (descending_order) {
      NodeLink* current_node = previous_node;
      DCHECK(current_node);

      // Remove from current list, and insert before the first node with a
      // greater
      base::LinkNode<NodeLink>::RemoveFromList();
      while (current_node != controller_->children_list_.end()) {
        if (current_node->key_ < key_)
          return base::LinkNode<NodeLink>::InsertAfter(current_node);

        current_node = current_node->GetPreviousSibling();
      }

      // Reached the end of the list, append to the end.
      return controller_->children_list_.Prepend(this);
    }

    NOTREACHED();
  }

  Ty* self_;
  NodeLinkController<Ty>* controller_;

  SortKey key_;
  bool active_;
};

// Controller node implement with sorted-map contrainer.
// Can be used nestly in viewport class node.
template <typename Ty>
class NodeLinkController final {
 public:
  NodeLinkController() {}
  ~NodeLinkController() {
    for (auto* it = children_list_.head(); it != children_list_.end();
         it = it->next()) {
      // Reset child controller association.
      static_cast<NodeLink<Ty>*>(it)->controller_ = nullptr;
    }
  }

  NodeLinkController(NodeLinkController&&) = delete;
  NodeLinkController(const NodeLinkController&) = delete;
  NodeLinkController& operator=(const NodeLinkController&) = delete;

  base::LinkNode<NodeLink<Ty>>* head() const { return children_list_.head(); }
  base::LinkNode<NodeLink<Ty>>* tail() const { return children_list_.tail(); }
  const base::LinkNode<NodeLink<Ty>>* end() const {
    return children_list_.end();
  }

 private:
  friend class NodeLink<Ty>;

  // Insert child node to controller by sorted key.
  void InsertChildNodeInternal(NodeLink<Ty>* node) {
    // From min to max.
    for (auto* it = children_list_.head(); it != children_list_.end();
         it = it->next()) {
      // Insert before the first node with a greater key.
      // This will keep the list sorted in ascending order.
      if (node->key_ < static_cast<NodeLink<Ty>*>(it)->key_)
        return node->InsertBefore(it);
    }

    // Max key, append to the end.
    children_list_.Append(node);
  }

  // Nodes sorted by key boardcast list (in-order, Z min to max)
  base::LinkedList<NodeLink<Ty>> children_list_;
};

}  // namespace content

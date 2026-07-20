// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/scene/transform.h"

namespace content {

class World;

URGE_BINDING()
class Node : public Object {
 public:
  Node();
  ~Node() override;

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  const glm::dmat4x4& GetModelMatrix(const glm::dvec3& base_position);
  void SetupWorld(World* new_world, World* old_world);
  void ResetParent(Node* parent);

  Transform* transform() { return transform_.get(); }

  World* world() { return world_; }
  uint32_t layer() const { return layer_; }
  bool& root() { return root_node_; }

 public:
  URGE_BINDING()
  static scoped_refptr<Node> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Parent, scoped_refptr<Node>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Active, bool);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Order, int64_t);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Layer, uint32_t);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Name, std::string);

  URGE_BINDING()
  scoped_refptr<Transform> GetTransform(URGE_EXCEPTION);

  URGE_BINDING()
  uint32_t GetChildrenCount(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Node> GetChildAt(uint32_t index, URGE_EXCEPTION);

 protected:
  virtual void OnEnterWorld(World* new_world) {}

  virtual void OnLeaveWorld(World* old_world) {}

 private:
  void ForEachNode(std::function<bool(Node*)> iter);
  void EnterWorld(World* world);
  void LeaveWorld(World* world);
  void ResortChildren(Node* target = nullptr);
  void TransformChange();

  scoped_refptr<Transform> transform_;

  Node* parent_;
  std::vector<scoped_refptr<Node>> children_;

  bool active_;
  int64_t order_;
  uint32_t layer_;
  std::string name_;
  glm::dmat4x4 model_;

  World* world_;
  bool root_node_;
  bool in_world_;
  bool transform_dirty_;
  glm::dvec3 position_offset_;
};

}  // namespace content

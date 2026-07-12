// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/common/disposable.h"
#include "content/content_config.h"
#include "content/scene/node_link.h"
#include "content/scene/transform.h"

namespace content {

class World;

URGE_BINDING()
class Node : public Disposable {
 public:
  Node();
  ~Node() override;

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  virtual void SetupAsWorldRoot(World* world);
  const glm::dmat4x4& GetModelMatrix();
  World* GetWorld();

  uint32_t culling_layer() const { return layer_; }

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
  scoped_refptr<Node> FirstChild(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Node> LastChild(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Node> PreviousSibling(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Node> NextSibling(URGE_EXCEPTION);

 protected:
  virtual void OnEnterWorld(World* new_world);

  virtual void OnLeaveWorld(World* old_world);

 private:
  std::string_view ObjectName() override { return "URGE.Node"; }
  void OnObjectRelease() override;

  void OnTransformChange();

  NodeLink<Node> node_;
  NodeLinkController<Node> children_;

  scoped_refptr<Node> parent_;
  scoped_refptr<Transform> transform_;

  uint32_t layer_;
  std::string name_;
  glm::dmat4x4 model_;

  bool transform_dirty_;
  World* world_;
};

}  // namespace content

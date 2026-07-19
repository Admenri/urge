// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/common/vector.h"
#include "content/resource/material.h"
#include "content/resource/mesh.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
class MeshRenderer : public Node {
 public:
  MeshRenderer();
  ~MeshRenderer() override;

  MeshRenderer(const MeshRenderer&) = delete;
  MeshRenderer& operator=(const MeshRenderer&) = delete;

  Mesh* mesh() { return mesh_.get(); }
  const std::vector<scoped_refptr<Material>>& materials() const {
    return materials_;
  }

  const glm::vec3& bounds_min_data() const { return bounds_min_; }
  const glm::vec3& bounds_max_data() const { return bounds_max_; }

 public:
  URGE_BINDING()
  static scoped_refptr<MeshRenderer> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Mesh, scoped_refptr<Mesh>);

  URGE_BINDING()
  void ComputeAABB(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Vector3> GetBoundsMin(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Vector3> GetBoundsMax(URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Material> GetMaterialAtSlot(uint32_t slot, URGE_EXCEPTION);

  URGE_BINDING()
  void SetMaterialAtSlot(uint32_t slot,
                         scoped_refptr<Material> material,
                         URGE_EXCEPTION);

 protected:
  void OnEnterWorld(World* new_world) override;
  void OnLeaveWorld(World* old_world) override;

 private:
  scoped_refptr<Mesh> mesh_;
  std::vector<scoped_refptr<Material>> materials_;

  glm::vec3 bounds_min_;
  glm::vec3 bounds_max_;
};

}  // namespace content

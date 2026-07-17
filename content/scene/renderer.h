// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

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

 public:
  URGE_BINDING()
  static scoped_refptr<MeshRenderer> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Mesh, scoped_refptr<Mesh>);

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
};

}  // namespace content

// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/renderer.h"

#include "content/common/exception.h"
#include "content/scene/world.h"

namespace content {

// static
scoped_refptr<MeshRenderer> MeshRenderer::New(URGE_EXCEPTION) {
  return Object::Create<MeshRenderer>();
}

MeshRenderer::MeshRenderer() {}

MeshRenderer::~MeshRenderer() {}

URGE_ATTRIBUTE_DEFINE(
    MeshRenderer,
    Mesh,
    scoped_refptr<Mesh>,
    { return mesh_; },
    { mesh_ = value; });

scoped_refptr<Material> MeshRenderer::GetMaterialAtSlot(uint32_t slot,
                                                        URGE_EXCEPTION) {
  if (slot >= 0 && slot < materials_.size())
    return materials_[slot];
  return nullptr;
}

void MeshRenderer::SetMaterialAtSlot(uint32_t slot,
                                     scoped_refptr<Material> material,
                                     URGE_EXCEPTION) {
  if (slot >= materials_.size())
    materials_.resize(slot + 1);

  if (slot >= 0)
    materials_[slot] = material;
}

void MeshRenderer::ComputeAABB(URGE_EXCEPTION) {
  if (mesh_) {
    ExceptionState exception_state;
    auto submeshes = mesh_->GetSubMeshes(exception_state);

    glm::vec3 min(std::numeric_limits<float>::max());
    glm::vec3 max(std::numeric_limits<float>::lowest());
    bool valid = false;

    for (auto& submesh : submeshes) {
      if (submesh->boundsMin && submesh->boundsMax) {
        const glm::vec3& smin = submesh->boundsMin->data();
        const glm::vec3& smax = submesh->boundsMax->data();
        min = glm::min(min, smin);
        max = glm::max(max, smax);
        valid = true;
      }
    }

    if (valid) {
      bounds_min_ = min;
      bounds_max_ = max;
      return;
    }
  }

  bounds_min_ = glm::vec3(0.f);
  bounds_max_ = glm::vec3(0.f);
}

scoped_refptr<Vector3> MeshRenderer::GetBoundsMin(URGE_EXCEPTION) {
  return Object::Create<Vector3>(bounds_min_);
}

scoped_refptr<Vector3> MeshRenderer::GetBoundsMax(URGE_EXCEPTION) {
  return Object::Create<Vector3>(bounds_max_);
}

void MeshRenderer::OnEnterWorld(World* new_world) {
  new_world->RegisterRenderer(this);
}

void MeshRenderer::OnLeaveWorld(World* old_world) {
  old_world->UnregisterRenderer(this);
}

}  // namespace content

// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/renderer.h"

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

}  // namespace content

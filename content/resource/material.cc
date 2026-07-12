// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/resource/material.h"

namespace content {

// static
scoped_refptr<Material> Material::New(URGE_EXCEPTION) {
  return Object::Create<Material>();
}

Material::Material() : render_queue_(0) {}

URGE_ATTRIBUTE_DEFINE(
    Material,
    RenderQueue,
    uint32_t,
    { return render_queue_; },
    { render_queue_ = value; });

void Material::SetupPasses(earray<scoped_refptr<ShaderPass>> passes,
                           URGE_EXCEPTION) {
  passes_ = passes;
}

earray<scoped_refptr<ShaderPass>> Material::GetPasses(URGE_EXCEPTION) {
  return passes_;
}

void Material::SetupBindingData(uint32_t slot,
                                scoped_refptr<GPUBindGroup> group,
                                earray<uint32_t> offsets,
                                URGE_EXCEPTION) {
  if (slot >= bindings_.size())
    bindings_.resize(slot + 1);

  if (slot >= 0) {
    bindings_[slot].bind_group = group;
    bindings_[slot].offsets = offsets;
  }
}

}  // namespace content

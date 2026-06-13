// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/viewport.h"

namespace content {

///
/// Viewport
///

// static
scoped_refptr<Viewport> Viewport::New(scoped_refptr<Vector2i> size,
                                      URGE_EXCEPTION) {
  return base::MakeRefCounted<Viewport>(size->data());
}

Viewport::Viewport(const glm::ivec2& size) : size_(size) {}

URGE_ATTRIBUTE_DEFINE(
    Viewport,
    World,
    scoped_refptr<World>,
    { return world_; },
    { world_ = value; });

URGE_ATTRIBUTE_DEFINE(
    Viewport,
    Renderer,
    scoped_refptr<RendererProcess>,
    { return process_; },
    { process_ = value; });

scoped_refptr<Vector2i> Viewport::GetSize(URGE_EXCEPTION) {
  return base::MakeRefCounted<Vector2i>(size_);
}

}  // namespace content

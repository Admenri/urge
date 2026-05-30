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
    Camera,
    scoped_refptr<Camera>,
    { return camera_; },
    { camera_ = value; });

scoped_refptr<Vector2i> Viewport::GetSize(URGE_EXCEPTION) {
  return base::MakeRefCounted<Vector2i>(size_);
}

///
/// ViewportContainer
///

// static
scoped_refptr<ViewportContainer> ViewportContainer::New(URGE_EXCEPTION) {
  return base::MakeRefCounted<ViewportContainer>();
}

ViewportContainer::ViewportContainer() : Node() {}

URGE_ATTRIBUTE_DEFINE(
    ViewportContainer,
    Region,
    scoped_refptr<Rect>,
    { return region_; },
    { region_->Set(value, exception_state); });

URGE_ATTRIBUTE_DEFINE(
    ViewportContainer,
    Viewport,
    scoped_refptr<Viewport>,
    { return viewport_; },
    { viewport_ = value; });

}  // namespace content

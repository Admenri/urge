// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/world.h"

namespace content {

// static
scoped_refptr<World> World::New(URGE_EXCEPTION) {
  return Object::Create<World>();
}

World::World() {}

URGE_ATTRIBUTE_DEFINE(
    World,
    Root,
    scoped_refptr<Node>,
    { return root_; },
    {
      // Return if same root node
      if (root_ == value)
        return;

      // Unbind world from previous node if available
      if (root_)
        root_->SetupAsWorldRoot(nullptr);

      // Bind world to target node tree if available
      if (value)
        value->SetupAsWorldRoot(this);

      // Node reference
      root_ = value;
    });

}  // namespace content

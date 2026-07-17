// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/world.h"

#include <algorithm>

#include "content/scene/camera.h"
#include "content/scene/renderer.h"

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
      if (root_) {
        root_->root() = false;
        root_->SetupWorld(nullptr, root_->world());
      }

      // Reset target parent
      if (value)
        value->ResetParent(nullptr);

      // Bind world to target node tree if available
      if (value) {
        value->root() = true;
        value->SetupWorld(this, value->world());
      }

      // Node reference & root set
      root_ = value;
    });

void World::RegisterCamera(Camera* camera) {
  cameras_.push_back(camera);
}

void World::UnregisterCamera(Camera* camera) {
  auto it = std::find(cameras_.begin(), cameras_.end(), camera);
  if (it != cameras_.end())
    cameras_.erase(it);
}

void World::RegisterRenderer(MeshRenderer* renderer) {
  renderers_.push_back(renderer);
}

void World::UnregisterRenderer(MeshRenderer* renderer) {
  auto it = std::find(renderers_.begin(), renderers_.end(), renderer);
  if (it != renderers_.end())
    renderers_.erase(it);
}

}  // namespace content

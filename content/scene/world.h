// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/render/camera.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
class World : public Object {
 public:
  World();

  World(const World&) = delete;
  World& operator=(const World&) = delete;

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Root, scoped_refptr<Node>);

 private:
};

}  // namespace content

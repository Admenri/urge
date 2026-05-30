// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

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

 private:
};

}  // namespace content

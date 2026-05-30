// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/render/camera.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
class Viewport : public Object {
 public:
  Viewport(const glm::ivec2& size);

  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<Viewport> New(scoped_refptr<Vector2i> size,
                                     URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Camera, scoped_refptr<Camera>);

  URGE_BINDING()
  scoped_refptr<Vector2i> GetSize(URGE_EXCEPTION);

 private:
  glm::ivec2 size_;
  scoped_refptr<Camera> camera_;
};

URGE_BINDING()
class ViewportContainer : public Node {
 public:
  ViewportContainer();

  ViewportContainer(const ViewportContainer&) = delete;
  ViewportContainer& operator=(const ViewportContainer&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<ViewportContainer> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Region, scoped_refptr<Rect>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Viewport, scoped_refptr<Viewport>);

 private:
  std::string_view ObjectName() override { return "URGE.ViewportContainer"; }

  scoped_refptr<Rect> region_;
  scoped_refptr<Viewport> viewport_;
};

}  // namespace content

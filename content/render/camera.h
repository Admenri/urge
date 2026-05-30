// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/common/utility.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
class Camera : public Node {
 public:
  Camera();

  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

 public:
  const glm::mat4x4& GetProjectionMatrix();
  glm::mat4x4 GetViewMatrix();

 public:
  URGE_BINDING() URGE_ATTRIBUTE_DECLARE(Near, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Far, float);

 private:
  std::string_view ObjectName() override { return "URGE.Camera"; }

  glm::mat4x4 projection_;
  bool projection_dirty_;

 protected:
  virtual glm::mat4x4 GetProjection() = 0;
  void NotifyProjectionChange() { projection_dirty_ = true; }

  float near_;
  float far_;
};

URGE_BINDING()
class PerspectiveCamera : public Camera {
 public:
  PerspectiveCamera();

  PerspectiveCamera(const PerspectiveCamera&) = delete;
  PerspectiveCamera& operator=(const PerspectiveCamera&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<PerspectiveCamera> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(FOVY, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Aspect, float);

 private:
  std::string_view ObjectName() override { return "URGE.PerspectiveCamera"; }
  glm::mat4x4 GetProjection() override;

  float fovy_;
  float aspect_;
};

URGE_BINDING()
class OrthographicCamera : public Camera {
 public:
  OrthographicCamera();

  OrthographicCamera(const OrthographicCamera&) = delete;
  OrthographicCamera& operator=(const OrthographicCamera&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<OrthographicCamera> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Region, scoped_refptr<Rect>);

 private:
  std::string_view ObjectName() override { return "URGE.OrthographicCamera"; }
  glm::mat4x4 GetProjection() override;

  scoped_refptr<Rect> region_;
};

}  // namespace content

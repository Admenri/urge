// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

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
  glm::mat4x4 GetViewProjection();

  uint64_t culling_mask() const { return culling_mask_; }
  float near_plane() const { return near_; }
  float far_plane() const { return far_; }

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(CullingMask, uint64_t);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Near, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Far, float);

 protected:
  virtual glm::mat4x4 GetProjection() = 0;
  void NotifyProjectionChange() { projection_dirty_ = true; }

  void OnEnterWorld(World* new_world) override;
  void OnLeaveWorld(World* old_world) override;

 private:
  glm::mat4x4 projection_;
  bool projection_dirty_;

  uint64_t culling_mask_;
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
  URGE_ATTRIBUTE_DECLARE(Left, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Right, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Bottom, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Top, float);

 private:
  glm::mat4x4 GetProjection() override;

  float left_;
  float right_;
  float bottom_;
  float top_;
};

}  // namespace content

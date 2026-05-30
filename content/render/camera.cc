// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/camera.h"

#include "glm/gtc/matrix_inverse.hpp"

namespace content {

///
/// CameraBase
///

Camera::Camera() : Node(), projection_dirty_(true), near_(0.1f), far_(2000.f) {}

const glm::mat4x4& Camera::GetProjectionMatrix() {
  if (projection_dirty_) {
    projection_ = GetProjection();
    projection_dirty_ = false;
  }

  return projection_;
}

glm::mat4x4 Camera::GetViewMatrix() {
  auto model_matrix = GetModelMatrix();

  // glm::affineInverse instead of glm::inverse
  return glm::affineInverse(model_matrix);
}

URGE_ATTRIBUTE_DEFINE(
    Camera,
    Near,
    float,
    { return near_; },
    {
      near_ = value;
      NotifyProjectionChange();
    });

URGE_ATTRIBUTE_DEFINE(
    Camera,
    Far,
    float,
    { return far_; },
    {
      far_ = value;
      NotifyProjectionChange();
    });

///
/// PerspectiveCamera
///

// static
scoped_refptr<PerspectiveCamera> PerspectiveCamera::New(URGE_EXCEPTION) {
  return Object::Create<PerspectiveCamera>();
}

PerspectiveCamera::PerspectiveCamera() : Camera(), fovy_(50.f), aspect_(1.f) {}

URGE_ATTRIBUTE_DEFINE(
    PerspectiveCamera,
    FOVY,
    float,
    { return fovy_; },
    {
      fovy_ = value;
      NotifyProjectionChange();
    });

URGE_ATTRIBUTE_DEFINE(
    PerspectiveCamera,
    Aspect,
    float,
    { return aspect_; },
    {
      aspect_ = value;
      NotifyProjectionChange();
    });

glm::mat4x4 PerspectiveCamera::GetProjection() {
  return glm::perspective<float>(fovy_, aspect_, near_, far_);
}

///
/// OrthographicCamera
///

// static
scoped_refptr<OrthographicCamera> OrthographicCamera::New(URGE_EXCEPTION) {
  return Object::Create<OrthographicCamera>();
}

OrthographicCamera::OrthographicCamera()
    : Camera(), region_(Object::Create<Rect>(-1.f, 1.f, 1.f, -1.f)) {}

URGE_ATTRIBUTE_DEFINE(
    OrthographicCamera,
    Region,
    scoped_refptr<Rect>,
    { return region_; },
    {
      region_->Set(value, exception_state);
      NotifyProjectionChange();
    });

glm::mat4x4 OrthographicCamera::GetProjection() {
  const auto* bounds = region_->bounds();
  return glm::ortho<float>(bounds[Rect::AXIS_LEFT], bounds[Rect::AXIS_RIGHT],
                           bounds[Rect::AXIS_TOP], bounds[Rect::AXIS_BOTTOM]);
}

}  // namespace content

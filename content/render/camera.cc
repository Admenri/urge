// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/camera.h"

#include "glm/gtc/matrix_inverse.hpp"

namespace content {

///
/// CameraBase
///

Camera::Camera()
    : Node(),
      projection_dirty_(true),
      culling_mask_(std::numeric_limits<uint64_t>::max()),
      near_(0.1f),
      far_(2000.f) {}

const glm::mat4x4& Camera::GetProjectionMatrix() {
  if (projection_dirty_) {
    projection_ = GetProjection();
    projection_dirty_ = false;
  }

  return projection_;
}

URGE_ATTRIBUTE_DEFINE(
    Camera,
    CullingMask,
    uint64_t,
    { return culling_mask_; },
    { culling_mask_ = value; });

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
  return glm::perspective<float>(fovy_, aspect_, near_plane(), far_plane());
}

///
/// OrthographicCamera
///

// static
scoped_refptr<OrthographicCamera> OrthographicCamera::New(URGE_EXCEPTION) {
  return Object::Create<OrthographicCamera>();
}

OrthographicCamera::OrthographicCamera()
    : Camera(), left_(0.f), right_(0.f), bottom_(0.f), top_(0.f) {}

URGE_ATTRIBUTE_DEFINE(
    OrthographicCamera,
    Left,
    float,
    { return left_; },
    {
      left_ = value;
      NotifyProjectionChange();
    });

URGE_ATTRIBUTE_DEFINE(
    OrthographicCamera,
    Right,
    float,
    { return right_; },
    {
      right_ = value;
      NotifyProjectionChange();
    });

URGE_ATTRIBUTE_DEFINE(
    OrthographicCamera,
    Bottom,
    float,
    { return bottom_; },
    {
      bottom_ = value;
      NotifyProjectionChange();
    });

URGE_ATTRIBUTE_DEFINE(
    OrthographicCamera,
    Top,
    float,
    { return top_; },
    {
      top_ = value;
      NotifyProjectionChange();
    });

glm::mat4x4 OrthographicCamera::GetProjection() {
  return glm::ortho<float>(left_, right_, bottom_, top_, near_plane(),
                           far_plane());
}

}  // namespace content

// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/scene/transform.h"

#include <cmath>

#include "glm/gtc/matrix_transform.hpp"

namespace content {

///
/// Quaternion
///

Quaternion::Quaternion() : value_() {}

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    W,
    double,
    { return value_.w; },
    {
      value_.w = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    X,
    double,
    { return value_.x; },
    {
      value_.x = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    Y,
    double,
    { return value_.y; },
    {
      value_.y = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    Z,
    double,
    { return value_.z; },
    {
      value_.z = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    Euler,
    scoped_refptr<Vector3d>,
    { return Object::Create<Vector3d>(glm::eulerAngles(value_)); },
    {
      value_ = glm::dquat(value->data());
      on_change();
    });

Quaternion& Quaternion::Set(scoped_refptr<Quaternion> value, URGE_EXCEPTION) {
  value_ = value->value_;
  on_change();
  return *this;
}

///
/// Transform
///

Transform::Transform()
    : position_(Object::Create<Vector3d>()),
      quaternion_(Object::Create<Quaternion>()),
      scale_(Object::Create<Vector3d>(glm::dvec3(1.0))) {
  auto value_change_handler =
      base::BindRepeating(&Transform::on_change, base::Unretained(this));

  position_->set_change_handler(value_change_handler);
  quaternion_->set_change_handler(value_change_handler);
  scale_->set_change_handler(value_change_handler);
}

glm::dmat4x4 Transform::GetModelMatrix(const glm::dvec3& position_offset) {
  const auto& position = position_->data();
  const auto& quaternion = quaternion_->data();
  const auto& scale = scale_->data();

  glm::dmat4 model(1.0);
  model = glm::translate(model, position);
  model = model * glm::mat4_cast(quaternion);
  model = glm::scale(model, scale);

  return model;
}

glm::dmat4x4 Transform::GetForwardMatrix() {
  const auto& quaternion = quaternion_->data();
  const auto& scale = scale_->data();

  glm::dmat4 model(1.0);
  model = model * glm::mat4_cast(quaternion);
  model = glm::scale(model, scale);

  return model;
}

URGE_ATTRIBUTE_DEFINE(
    Transform,
    Position,
    scoped_refptr<Vector3d>,
    { return position_; },
    { position_->Set(value, exception_state); });

URGE_ATTRIBUTE_DEFINE(
    Transform,
    Quaternion,
    scoped_refptr<Quaternion>,
    { return quaternion_; },
    { quaternion_->Set(value, exception_state); });

URGE_ATTRIBUTE_DEFINE(
    Transform,
    Scale,
    scoped_refptr<Vector3d>,
    { return scale_; },
    { scale_->Set(value, exception_state); });

Transform& Transform::Set(scoped_refptr<Transform> value, URGE_EXCEPTION) {
  position_->Set(value->position_, exception_state);
  quaternion_->Set(value->quaternion_, exception_state);
  scale_->Set(value->scale_, exception_state);
  return *this;
}

}  // namespace content

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
    float,
    { return value_.w; },
    {
      value_.w = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    X,
    float,
    { return value_.x; },
    {
      value_.x = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    Y,
    float,
    { return value_.y; },
    {
      value_.y = value;
      on_change();
    });

URGE_ATTRIBUTE_DEFINE(
    Quaternion,
    Z,
    float,
    { return value_.z; },
    {
      value_.z = value;
      on_change();
    });

Quaternion& Quaternion::Set(scoped_refptr<Quaternion> value, URGE_EXCEPTION) {
  value_ = value->value_;
  on_change();
  return *this;
}

void Quaternion::SetEuler(scoped_refptr<Vector3> rotation, URGE_EXCEPTION) {
  value_ = glm::quat(rotation->data());
  on_change();
}

scoped_refptr<Vector3> Quaternion::GetEuler(URGE_EXCEPTION) {
  return Object::Create<Vector3>(glm::eulerAngles(value_));
}

///
/// Transform
///

Transform::Transform()
    : position_(Object::Create<Vector3>()),
      quaternion_(Object::Create<Quaternion>()),
      scale_(Object::Create<Vector3>(glm::vec3(1.0f))) {
  auto value_change_handler =
      base::BindRepeating(&Transform::on_change, base::Unretained(this));

  position_->set_change_handler(value_change_handler);
  quaternion_->set_change_handler(value_change_handler);
  scale_->set_change_handler(value_change_handler);
}

glm::mat4x4 Transform::GetModelMatrix() {
  const auto& position = position_->data();
  const auto& quaternion = quaternion_->data();
  const auto& scale = scale_->data();

  glm::mat4 model(1.0f);
  model = glm::translate(model, position);
  model = model * glm::mat4_cast(quaternion);
  model = glm::scale(model, scale);
  return model;
}

URGE_ATTRIBUTE_DEFINE(
    Transform,
    Position,
    scoped_refptr<Vector3>,
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
    scoped_refptr<Vector3>,
    { return scale_; },
    { scale_->Set(value, exception_state); });

}  // namespace content

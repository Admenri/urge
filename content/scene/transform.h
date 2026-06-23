// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "glm/gtc/quaternion.hpp"
#include "glm/mat4x4.hpp"

#include "content/common/object.h"
#include "content/common/vector.h"

namespace content {

URGE_BINDING()
class Quaternion : public Constant {
 public:
  Quaternion();

  Quaternion(const Quaternion&) = delete;
  Quaternion& operator=(const Quaternion&) = delete;

  const glm::dquat& data() const { return value_; }
  void set_data(const glm::dquat& value) {
    value_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(W, double);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(X, double);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Y, double);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Z, double);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Euler, scoped_refptr<Vector3d>);

  URGE_BINDING()
  Quaternion& Set(scoped_refptr<Quaternion> value, URGE_EXCEPTION);

 private:
  glm::dquat value_;
};

URGE_BINDING()
class Transform : public Constant {
 public:
  Transform();

  Transform(const Transform&) = delete;
  Transform& operator=(const Transform&) = delete;

  glm::dmat4x4 GetModelMatrix();

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Position, scoped_refptr<Vector3d>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Quaternion, scoped_refptr<Quaternion>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Scale, scoped_refptr<Vector3d>);

  URGE_BINDING()
  Transform& Set(scoped_refptr<Transform> value, URGE_EXCEPTION);

 private:
  scoped_refptr<Vector3d> position_;
  scoped_refptr<Quaternion> quaternion_;
  scoped_refptr<Vector3d> scale_;
};

}  // namespace content

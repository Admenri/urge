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

  const glm::quat& data() const { return value_; }
  void set_data(const glm::quat& value) {
    value_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(W, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(X, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Y, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Z, float);

  URGE_BINDING()
  Quaternion& Set(scoped_refptr<Quaternion> value, URGE_EXCEPTION);

  URGE_BINDING()
  void SetEuler(scoped_refptr<Vector3> rotation, URGE_EXCEPTION);

  URGE_BINDING()
  scoped_refptr<Vector3> GetEuler(URGE_EXCEPTION);

 private:
  glm::quat value_;
};

URGE_BINDING()
class Transform : public Constant {
 public:
  Transform();

  Transform(const Transform&) = delete;
  Transform& operator=(const Transform&) = delete;

  glm::mat4x4 GetModelMatrix();

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Position, scoped_refptr<Vector3>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Quaternion, scoped_refptr<Quaternion>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Scale, scoped_refptr<Vector3>);

 private:
  scoped_refptr<Vector3> position_;
  scoped_refptr<Quaternion> quaternion_;
  scoped_refptr<Vector3> scale_;
};

}  // namespace content

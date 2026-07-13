// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "glm/vec2.hpp"
#include "glm/vec3.hpp"
#include "glm/vec4.hpp"

#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"

namespace content {

// ================= Vector2 (float) =================
URGE_BINDING()
class Vector2 : public Constant {
 public:
  Vector2() : data_() {}
  Vector2(float x, float y) : data_(x, y) {}
  Vector2(const glm::vec2& data) : data_(data) {}

  const glm::vec2& data() const { return data_; }
  void set_data(const glm::vec2& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector2> New(URGE_EXCEPTION) {
    return Object::Create<Vector2>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector2> New(float x, float y, URGE_EXCEPTION) {
    return Object::Create<Vector2>(x, y);
  }

  URGE_BINDING()
  static scoped_refptr<Vector2> Copy(scoped_refptr<Vector2> other,
                                     URGE_EXCEPTION) {
    return Object::Create<Vector2>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector2> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      float,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      float,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  Vector2& Set(scoped_refptr<Vector2> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector2& Add(scoped_refptr<Vector2> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector2& Sub(scoped_refptr<Vector2> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector2& Mul(scoped_refptr<Vector2> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector2& Div(scoped_refptr<Vector2> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::vec2 data_;
};

// ================= Vector2d (double) =================
URGE_BINDING()
class Vector2d : public Constant {
 public:
  Vector2d() : data_() {}
  Vector2d(double x, double y) : data_(x, y) {}
  Vector2d(const glm::dvec2& data) : data_(data) {}

  const glm::dvec2& data() const { return data_; }
  void set_data(const glm::dvec2& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector2d> New(URGE_EXCEPTION) {
    return Object::Create<Vector2d>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector2d> New(double x, double y, URGE_EXCEPTION) {
    return Object::Create<Vector2d>(x, y);
  }

  URGE_BINDING()
  static scoped_refptr<Vector2d> Copy(scoped_refptr<Vector2d> other,
                                      URGE_EXCEPTION) {
    return Object::Create<Vector2d>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector2d> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      double,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      double,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  Vector2d& Set(scoped_refptr<Vector2d> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector2d& Add(scoped_refptr<Vector2d> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector2d& Sub(scoped_refptr<Vector2d> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector2d& Mul(scoped_refptr<Vector2d> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector2d& Div(scoped_refptr<Vector2d> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::dvec2 data_;
};

// ================= Vector2i (int) =================
URGE_BINDING()
class Vector2i : public Constant {
 public:
  Vector2i() : data_() {}
  Vector2i(int x, int y) : data_(x, y) {}
  Vector2i(const glm::ivec2& data) : data_(data) {}

  const glm::ivec2& data() const { return data_; }
  void set_data(const glm::ivec2& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector2i> New(URGE_EXCEPTION) {
    return Object::Create<Vector2i>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector2i> New(int x, int y, URGE_EXCEPTION) {
    return Object::Create<Vector2i>(x, y);
  }

  URGE_BINDING()
  static scoped_refptr<Vector2i> Copy(scoped_refptr<Vector2i> other,
                                      URGE_EXCEPTION) {
    return Object::Create<Vector2i>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector2i> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      int,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      int,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  Vector2i& Set(scoped_refptr<Vector2i> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector2i& Add(scoped_refptr<Vector2i> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector2i& Sub(scoped_refptr<Vector2i> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector2i& Mul(scoped_refptr<Vector2i> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector2i& Div(scoped_refptr<Vector2i> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::ivec2 data_;
};

// ================= Vector3 (float) =================
URGE_BINDING()
class Vector3 : public Constant {
 public:
  Vector3() : data_() {}
  Vector3(float x, float y, float z) : data_(x, y, z) {}
  Vector3(const glm::vec3& data) : data_(data) {}

  const glm::vec3& data() const { return data_; }
  void set_data(const glm::vec3& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector3> New(URGE_EXCEPTION) {
    return Object::Create<Vector3>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector3> New(float x, float y, float z, URGE_EXCEPTION) {
    return Object::Create<Vector3>(x, y, z);
  }

  URGE_BINDING()
  static scoped_refptr<Vector3> Copy(scoped_refptr<Vector3> other,
                                     URGE_EXCEPTION) {
    return Object::Create<Vector3>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector3> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      float,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      float,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Z,
      float,
      { return data_.z; },
      {
        data_.z = value;
        on_change();
      });

  URGE_BINDING()
  Vector3& Set(scoped_refptr<Vector3> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector3& Add(scoped_refptr<Vector3> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector3& Sub(scoped_refptr<Vector3> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector3& Mul(scoped_refptr<Vector3> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector3& Div(scoped_refptr<Vector3> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::vec3 data_;
};

// ================= Vector3d (double) =================
URGE_BINDING()
class Vector3d : public Constant {
 public:
  Vector3d() : data_() {}
  Vector3d(double x, double y, double z) : data_(x, y, z) {}
  Vector3d(const glm::dvec3& data) : data_(data) {}

  const glm::dvec3& data() const { return data_; }
  void set_data(const glm::dvec3& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector3d> New(URGE_EXCEPTION) {
    return Object::Create<Vector3d>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector3d> New(double x,
                                     double y,
                                     double z,
                                     URGE_EXCEPTION) {
    return Object::Create<Vector3d>(x, y, z);
  }

  URGE_BINDING()
  static scoped_refptr<Vector3d> Copy(scoped_refptr<Vector3d> other,
                                      URGE_EXCEPTION) {
    return Object::Create<Vector3d>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector3d> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      double,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      double,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Z,
      double,
      { return data_.z; },
      {
        data_.z = value;
        on_change();
      });

  URGE_BINDING()
  Vector3d& Set(scoped_refptr<Vector3d> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector3d& Add(scoped_refptr<Vector3d> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector3d& Sub(scoped_refptr<Vector3d> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector3d& Mul(scoped_refptr<Vector3d> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector3d& Div(scoped_refptr<Vector3d> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::dvec3 data_;
};

// ================= Vector3i (int) =================
URGE_BINDING()
class Vector3i : public Constant {
 public:
  Vector3i() : data_() {}
  Vector3i(int x, int y, int z) : data_(x, y, z) {}
  Vector3i(const glm::ivec3& data) : data_(data) {}

  const glm::ivec3& data() const { return data_; }
  void set_data(const glm::ivec3& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector3i> New(URGE_EXCEPTION) {
    return Object::Create<Vector3i>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector3i> New(int x, int y, int z, URGE_EXCEPTION) {
    return Object::Create<Vector3i>(x, y, z);
  }

  URGE_BINDING()
  static scoped_refptr<Vector3i> Copy(scoped_refptr<Vector3i> other,
                                      URGE_EXCEPTION) {
    return Object::Create<Vector3i>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector3i> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      int,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      int,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Z,
      int,
      { return data_.z; },
      {
        data_.z = value;
        on_change();
      });

  URGE_BINDING()
  Vector3i& Set(scoped_refptr<Vector3i> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector3i& Add(scoped_refptr<Vector3i> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector3i& Sub(scoped_refptr<Vector3i> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector3i& Mul(scoped_refptr<Vector3i> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector3i& Div(scoped_refptr<Vector3i> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::ivec3 data_;
};

// ================= Vector4 (float) =================
URGE_BINDING()
class Vector4 : public Constant {
 public:
  Vector4() : data_() {}
  Vector4(float x, float y, float z, float w) : data_(x, y, z, w) {}
  Vector4(const glm::vec4& data) : data_(data) {}

  const glm::vec4& data() const { return data_; }
  void set_data(const glm::vec4& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector4> New(URGE_EXCEPTION) {
    return Object::Create<Vector4>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector4> New(float x,
                                    float y,
                                    float z,
                                    float w,
                                    URGE_EXCEPTION) {
    return Object::Create<Vector4>(x, y, z, w);
  }

  URGE_BINDING()
  static scoped_refptr<Vector4> Copy(scoped_refptr<Vector4> other,
                                     URGE_EXCEPTION) {
    return Object::Create<Vector4>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector4> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      float,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      float,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Z,
      float,
      { return data_.z; },
      {
        data_.z = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      W,
      float,
      { return data_.w; },
      {
        data_.w = value;
        on_change();
      });

  URGE_BINDING()
  Vector4& Set(scoped_refptr<Vector4> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector4& Add(scoped_refptr<Vector4> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector4& Sub(scoped_refptr<Vector4> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector4& Mul(scoped_refptr<Vector4> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector4& Div(scoped_refptr<Vector4> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::vec4 data_;
};

// ================= Vector4d (double) =================
URGE_BINDING()
class Vector4d : public Constant {
 public:
  Vector4d() : data_() {}
  Vector4d(double x, double y, double z, double w) : data_(x, y, z, w) {}
  Vector4d(const glm::dvec4& data) : data_(data) {}

  const glm::dvec4& data() const { return data_; }
  void set_data(const glm::dvec4& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector4d> New(URGE_EXCEPTION) {
    return Object::Create<Vector4d>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector4d> New(double x,
                                     double y,
                                     double z,
                                     double w,
                                     URGE_EXCEPTION) {
    return Object::Create<Vector4d>(x, y, z, w);
  }

  URGE_BINDING()
  static scoped_refptr<Vector4d> Copy(scoped_refptr<Vector4d> other,
                                      URGE_EXCEPTION) {
    return Object::Create<Vector4d>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector4d> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      double,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      double,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Z,
      double,
      { return data_.z; },
      {
        data_.z = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      W,
      double,
      { return data_.w; },
      {
        data_.w = value;
        on_change();
      });

  URGE_BINDING()
  Vector4d& Set(scoped_refptr<Vector4d> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector4d& Add(scoped_refptr<Vector4d> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector4d& Sub(scoped_refptr<Vector4d> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector4d& Mul(scoped_refptr<Vector4d> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector4d& Div(scoped_refptr<Vector4d> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::dvec4 data_;
};

// ================= Vector4i (int) =================
URGE_BINDING()
class Vector4i : public Constant {
 public:
  Vector4i() : data_() {}
  Vector4i(int x, int y, int z, int w) : data_(x, y, z, w) {}
  Vector4i(const glm::ivec4& data) : data_(data) {}

  const glm::ivec4& data() const { return data_; }
  void set_data(const glm::ivec4& value) {
    data_ = value;
    on_change();
  }

 public:
  URGE_BINDING()
  static scoped_refptr<Vector4i> New(URGE_EXCEPTION) {
    return Object::Create<Vector4i>();
  }

  URGE_BINDING()
  static scoped_refptr<Vector4i> New(int x,
                                     int y,
                                     int z,
                                     int w,
                                     URGE_EXCEPTION) {
    return Object::Create<Vector4i>(x, y, z, w);
  }

  URGE_BINDING()
  static scoped_refptr<Vector4i> Copy(scoped_refptr<Vector4i> other,
                                      URGE_EXCEPTION) {
    return Object::Create<Vector4i>(other->data_);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Vector4i> other, URGE_EXCEPTION) {
    return data_ == other->data_;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      X,
      int,
      { return data_.x; },
      {
        data_.x = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Y,
      int,
      { return data_.y; },
      {
        data_.y = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Z,
      int,
      { return data_.z; },
      {
        data_.z = value;
        on_change();
      });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      W,
      int,
      { return data_.w; },
      {
        data_.w = value;
        on_change();
      });

  URGE_BINDING()
  Vector4i& Set(scoped_refptr<Vector4i> other, URGE_EXCEPTION) {
    data_ = other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = +)
  Vector4i& Add(scoped_refptr<Vector4i> other, URGE_EXCEPTION) {
    data_ += other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = -)
  Vector4i& Sub(scoped_refptr<Vector4i> other, URGE_EXCEPTION) {
    data_ -= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = *)
  Vector4i& Mul(scoped_refptr<Vector4i> other, URGE_EXCEPTION) {
    data_ *= other->data_;
    on_change();
    return *this;
  }

  URGE_BINDING(Name = /)
  Vector4i& Div(scoped_refptr<Vector4i> other, URGE_EXCEPTION) {
    data_ /= other->data_;
    on_change();
    return *this;
  }

 private:
  glm::ivec4 data_;
};

}  // namespace content

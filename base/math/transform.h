
////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2012 Laurent Gomila (laurent.gom@gmail.com)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the
// use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#ifndef BASE_MATH_TRANSFORM_H
#define BASE_MATH_TRANSFORM_H

#include <cmath>

#include "base/math/rectangle.h"

namespace base {

class TransformMatrix {
 public:
  TransformMatrix() : scale_(1.f, 1.f), rotation_(0), need_update_(true) {}

  virtual ~TransformMatrix() = default;

  TransformMatrix(const TransformMatrix&) = default;
  TransformMatrix& operator=(const TransformMatrix&) = default;

 public:
  const Vec2& GetPosition() const { return position_; }
  const Vec2& GetOrigin() const { return origin_; }
  const Vec2& GetScale() const { return scale_; }
  float GetRotation() const { return rotation_; }

  void SetPosition(const Vec2& value) {
    position_ = value;
    need_update_ = true;
  }

  void SetOrigin(const Vec2& value) {
    origin_ = value;
    need_update_ = true;
  }

  void SetScale(const Vec2& value) {
    scale_ = value;
    need_update_ = true;
  }

  void SetRotation(float value) {
    rotation_ = value;
    need_update_ = true;
  }

  void SetGlobalOffset(const Vec2i& value) {
    viewport_offset_ = value;
    need_update_ = true;
  }

  const float* GetMatrixDataUnsafe() {
    if (need_update_) {
      UpdateMatrix();
      need_update_ = false;
    }

    return transform_matrix_;
  }

  void NotifyUpdate() { need_update_ = true; }

 private:
  void UpdateMatrix() {
    float angle = rotation_;
    if (angle >= 360 || angle < -360)
      angle = std::fmod(angle, 360.f);
    if (angle < 0)
      angle += 360;

    angle = angle * 3.141592654f / 180.0f;
    float cosine = std::cos(angle);
    float sine = std::sin(angle);
    float sxc = scale_.x * cosine;
    float syc = scale_.y * cosine;
    float sxs = scale_.x * sine;
    float sys = scale_.y * sine;
    float tx =
        -origin_.x * sxc - origin_.y * sys + position_.x + viewport_offset_.x;
    float ty =
        origin_.x * sxs - origin_.y * syc + position_.y + viewport_offset_.y;

    transform_matrix_[0] = sxc;
    transform_matrix_[1] = sys;
    transform_matrix_[3] = tx;
    transform_matrix_[4] = -sxs;
    transform_matrix_[5] = syc;
    transform_matrix_[7] = ty;
  }

  Vec2 position_;
  Vec2 origin_;
  Vec2 scale_;
  float rotation_;
  Vec2i viewport_offset_;

  float transform_matrix_[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                 0, 0, 1, 0, 0, 0, 0, 1};

  bool need_update_ = false;
};

}  // namespace base

#endif  // BASE_MATH_TRANSFORM_H

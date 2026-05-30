// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/common/exception.h"
#include "content/common/object.h"
#include "content/content_config.h"

namespace content {

URGE_BINDING()
class Rect : public Constant {
 public:
  enum Axis {
    AXIS_LEFT = 0,
    AXIS_RIGHT,
    AXIS_TOP,
    AXIS_BOTTOM,
  };

  Rect(float left, float right, float top, float bottom)
      : bounds_{left, right, top, bottom} {}

  Rect(const Rect&) = delete;
  Rect operator=(const Rect&) = delete;

  const float* bounds() const { return bounds_; }

 public:
  URGE_BINDING()
  static scoped_refptr<Rect> New(URGE_EXCEPTION) {
    return Object::Create<Rect>(0.f, 0.f, 0.f, 0.f);
  }

  URGE_BINDING()
  static scoped_refptr<Rect> New(float left,
                                 float right,
                                 float top,
                                 float bottom,
                                 URGE_EXCEPTION) {
    return Object::Create<Rect>(left, right, top, bottom);
  }

  URGE_BINDING()
  static scoped_refptr<Rect> Copy(scoped_refptr<Rect> other, URGE_EXCEPTION) {
    return Object::Create<Rect>(
        other->bounds_[AXIS_LEFT], other->bounds_[AXIS_RIGHT],
        other->bounds_[AXIS_TOP], other->bounds_[AXIS_BOTTOM]);
  }

  URGE_BINDING()
  bool Equal(scoped_refptr<Rect> other, URGE_EXCEPTION) {
    return other->bounds_[AXIS_LEFT] == bounds_[AXIS_LEFT] &&
           other->bounds_[AXIS_RIGHT] == bounds_[AXIS_RIGHT] &&
           other->bounds_[AXIS_TOP] == bounds_[AXIS_TOP] &&
           other->bounds_[AXIS_BOTTOM] == bounds_[AXIS_BOTTOM];
  }

  URGE_BINDING()
  Rect& Set(scoped_refptr<Rect> other, URGE_EXCEPTION) {
    std::memcpy(bounds_, other->bounds_, sizeof(bounds_));
    return *this;
  }

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Left,
      float,
      { return bounds_[AXIS_LEFT]; },
      { bounds_[AXIS_LEFT] = value; });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Right,
      float,
      { return bounds_[AXIS_RIGHT]; },
      { bounds_[AXIS_RIGHT] = value; });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Top,
      float,
      { return bounds_[AXIS_TOP]; },
      { bounds_[AXIS_TOP] = value; });

  URGE_BINDING()
  URGE_ATTRIBUTE(
      Bottom,
      float,
      { return bounds_[AXIS_BOTTOM]; },
      { bounds_[AXIS_BOTTOM] = value; });

 private:
  float bounds_[4];
};

}  // namespace content

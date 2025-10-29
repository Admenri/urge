// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MATH_RECTANGLE_H_
#define BASE_MATH_RECTANGLE_H_

#include <stdint.h>

#include "base/math/vector.h"

#include "SDL3/SDL_rect.h"

namespace base {

class Rect;
class RectF;

class Rect {
 public:
  Rect() : x(0), y(0), width(0), height(0) {}
  Rect(const Vec2i& size) : x(0), y(0), width(size.x), height(size.y) {}
  Rect(int32_t ix, int32_t iy, int32_t iw, int32_t ih)
      : x(ix), y(iy), width(iw), height(ih) {}
  Rect(const Vec2i& pos, const Vec2i& size)
      : x(pos.x), y(pos.y), width(size.x), height(size.y) {}

  Rect(const Rect& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
  }

  bool operator()() const { return width && height; }

  Rect& operator=(const Rect& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
    return *this;
  }

  bool operator==(const Rect& other) const {
    return other.x == x && other.y == y && other.width == width &&
           other.height == height;
  }

  bool operator!=(const Rect& other) const {
    return other.x != x || other.y != y || other.width != width ||
           other.height != height;
  }

  Rect operator*(const Rect& value) const {
    return Rect(x * value.x, y * value.y, width * value.width,
                height * value.height);
  }

  SDL_Rect ToSDLRect() const { return SDL_Rect{x, y, width, height}; }

  base::Vec2i Position() const { return base::Vec2i(x, y); }
  base::Vec2i Size() const { return base::Vec2i(width, height); }

 public:
  int32_t x, y, width, height;
};

class RectF {
 public:
  RectF() : x(0.f), y(0.f), width(0.f), height(0.f) {}
  RectF(const Vec2& size) : x(0), y(0), width(size.x), height(size.y) {}
  RectF(float ix, float iy, float iw, float ih)
      : x(ix), y(iy), width(iw), height(ih) {}
  RectF(const Vec2& pos, const Vec2& size)
      : x(pos.x), y(pos.y), width(size.x), height(size.y) {}
  RectF(const Rect& ir)
      : x(static_cast<float>(ir.x)),
        y(static_cast<float>(ir.y)),
        width(static_cast<float>(ir.width)),
        height(static_cast<float>(ir.height)) {}

  RectF(const RectF& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
  }

  bool operator()() const { return width && height; }

  RectF& operator=(const RectF& other) {
    x = other.x;
    y = other.y;
    width = other.width;
    height = other.height;
    return *this;
  }

  bool operator==(const RectF& other) const {
    return other.x == x && other.y == y && other.width == width &&
           other.height == height;
  }

  bool operator!=(const RectF& other) const {
    return other.x != x || other.y != y || other.width != width ||
           other.height != height;
  }

  RectF operator*(const RectF& value) const {
    return RectF(x * value.x, y * value.y, width * value.width,
                 height * value.height);
  }

  base::Vec2 Position() const { return base::Vec2(x, y); }
  base::Vec2 Size() const { return base::Vec2(width, height); }

 public:
  float x, y, width, height;
};

base::Rect MakeIntersect(const base::Rect& A, const base::Rect& B);

}  // namespace base

#endif  //! BASE_MATH_RECTANGLE_H_

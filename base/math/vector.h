// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MATH_VECTOR_H_
#define BASE_MATH_VECTOR_H_

#include <iostream>

namespace base {

class Vec2;
class Vec3;
class Vec4;

class Vec2i;
class Vec3i;
class Vec4i;

class Vec2i {
 public:
  Vec2i() : x(0), y(0) {}
  Vec2i(int t) : x(t), y(t) {}
  Vec2i(int ix, int iy) : x(ix), y(iy) {}

  Vec2i(const Vec2i& other) {
    x = other.x;
    y = other.y;
  }

  Vec2i& operator=(const Vec2i& other) {
    x = other.x;
    y = other.y;
    return *this;
  }

  inline bool operator==(const Vec2i& other) const {
    return other.x == x && other.y == y;
  }
  inline Vec2i operator+(const Vec2i& value) const {
    return Vec2i(x + value.x, y + value.y);
  }
  inline Vec2i operator-(const Vec2i& value) const {
    return Vec2i(x - value.x, y - value.y);
  }
  inline Vec2i operator*(const Vec2i& value) const {
    return Vec2i(x * value.x, y * value.y);
  }
  inline Vec2i operator/(const Vec2i& value) const {
    return Vec2i(x / value.x, y / value.y);
  }
  inline Vec2i operator%(const Vec2i& value) const {
    return Vec2i(x % value.x, y % value.y);
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec2i& value);

 public:
  int x, y;
};

class Vec2 {
 public:
  Vec2() : x(0), y(0) {}
  Vec2(float t) : x(t), y(t) {}
  Vec2(float ix, float iy) : x(ix), y(iy) {}
  Vec2(const Vec2i& iv)
      : x(static_cast<float>(iv.x)), y(static_cast<float>(iv.y)) {}

  Vec2(const Vec2& other) {
    x = other.x;
    y = other.y;
  }

  Vec2& operator=(const Vec2& other) {
    x = other.x;
    y = other.y;
    return *this;
  }

  inline bool operator==(const Vec2& other) const {
    return other.x == x && other.y == y;
  }
  inline Vec2 operator+(const Vec2& value) const {
    return Vec2(x + value.x, y + value.y);
  }
  inline Vec2 operator-(const Vec2& value) const {
    return Vec2(x - value.x, y - value.y);
  }
  inline Vec2 operator*(const Vec2& value) const {
    return Vec2(x * value.x, y * value.y);
  }
  inline Vec2 operator/(const Vec2& value) const {
    return Vec2(x / value.x, y / value.y);
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec2& value);

 public:
  float x, y;
};

class Vec3i {
 public:
  Vec3i() : x(0), y(0), z(0) {}
  Vec3i(int t) : x(t), y(t), z(t) {}
  Vec3i(int ix, int iy, int iz) : x(ix), y(iy), z(iz) {}

  Vec3i(const Vec3i& other) {
    x = other.x;
    y = other.y;
    z = other.z;
  }

  Vec3i& operator=(const Vec3i& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
  }

  inline bool operator==(const Vec3i& other) const {
    return other.x == x && other.y == y && other.z == z;
  }
  inline Vec3i operator+(const Vec3i& value) const {
    return Vec3i(x + value.x, y + value.y, z + value.z);
  }
  inline Vec3i operator-(const Vec3i& value) const {
    return Vec3i(x - value.x, y - value.y, z - value.z);
  }
  inline Vec3i operator*(const Vec3i& value) const {
    return Vec3i(x * value.x, y * value.y, z * value.z);
  }
  inline Vec3i operator/(const Vec3i& value) const {
    return Vec3i(x / value.x, y / value.y, z / value.z);
  }
  inline Vec3i operator%(const Vec3i& value) const {
    return Vec3i(x % value.x, y % value.y, z % value.z);
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec3i& value);

 public:
  int x, y, z;
};

class Vec3 {
 public:
  Vec3() : x(0), y(0), z(0) {}
  Vec3(float t) : x(t), y(t), z(t) {}
  Vec3(float ix, float iy, float iz) : x(ix), y(iy), z(iz) {}
  Vec3(const Vec3i& iv)
      : x(static_cast<float>(iv.x)),
        y(static_cast<float>(iv.y)),
        z(static_cast<float>(iv.z)) {}

  Vec3(const Vec3& other) {
    x = other.x;
    y = other.y;
    z = other.z;
  }

  Vec3& operator=(const Vec3& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    return *this;
  }

  inline bool operator==(const Vec3& other) const {
    return other.x == x && other.y == y && other.z == z;
  }
  inline Vec3 operator+(const Vec3& value) const {
    return Vec3(x + value.x, y + value.y, z + value.z);
  }
  inline Vec3 operator-(const Vec3& value) const {
    return Vec3(x - value.x, y - value.y, z - value.z);
  }
  inline Vec3 operator*(const Vec3& value) const {
    return Vec3(x * value.x, y * value.y, z * value.z);
  }
  inline Vec3 operator/(const Vec3& value) const {
    return Vec3(x / value.x, y / value.y, z / value.z);
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec3& value);

 public:
  float x, y, z;
};

class Vec4i {
 public:
  Vec4i() : x(0), y(0), z(0), w(0) {}
  Vec4i(int t) : x(t), y(t), z(t), w(t) {}
  Vec4i(int ix, int iy, int iz, int iw) : x(ix), y(iy), z(iz), w(iw) {}

  Vec4i(const Vec4i& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    w = other.w;
  }

  Vec4i& operator=(const Vec4i& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    w = other.w;
    return *this;
  }

  inline bool operator==(const Vec4i& other) const {
    return other.x == x && other.y == y && other.z == z && other.w == w;
  }
  inline Vec4i operator+(const Vec4i& value) const {
    return Vec4i(x + value.x, y + value.y, z + value.z, w + value.w);
  }
  inline Vec4i operator-(const Vec4i& value) const {
    return Vec4i(x - value.x, y - value.y, z - value.z, w - value.w);
  }
  inline Vec4i operator*(const Vec4i& value) const {
    return Vec4i(x * value.x, y * value.y, z * value.z, w * value.w);
  }
  inline Vec4i operator/(const Vec4i& value) const {
    return Vec4i(x / value.x, y / value.y, z / value.z, w / value.w);
  }
  inline Vec4i operator%(const Vec4i& value) const {
    return Vec4i(x % value.x, y % value.y, z % value.z, w % value.w);
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec4i& value);

 public:
  int x, y, z, w;
};

class Vec4 {
 public:
  Vec4() : x(0), y(0), z(0), w(0) {}
  Vec4(float t) : x(t), y(t), z(t), w(t) {}
  Vec4(float ix, float iy, float iz, float iw) : x(ix), y(iy), z(iz), w(iw) {}
  Vec4(const Vec4i& iv)
      : x(static_cast<float>(iv.x)),
        y(static_cast<float>(iv.y)),
        z(static_cast<float>(iv.z)),
        w(static_cast<float>(iv.w)) {}

  Vec4(const Vec4& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    w = other.w;
  }

  Vec4& operator=(const Vec4& other) {
    x = other.x;
    y = other.y;
    z = other.z;
    w = other.w;
    return *this;
  }

  inline bool operator==(const Vec4& other) const {
    return other.x == x && other.y == y && other.z == z && other.w == w;
  }
  inline Vec4 operator+(const Vec4& value) const {
    return Vec4(x + value.x, y + value.y, z + value.z, w + value.w);
  }
  inline Vec4 operator-(const Vec4& value) const {
    return Vec4(x - value.x, y - value.y, z - value.z, w - value.w);
  }
  inline Vec4 operator*(const Vec4& value) const {
    return Vec4(x * value.x, y * value.y, z * value.z, w * value.w);
  }
  inline Vec4 operator/(const Vec4& value) const {
    return Vec4(x / value.x, y / value.y, z / value.z, w / value.w);
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec4& value);

 public:
  float x, y, z, w;
};

inline Vec4 MakeVec4(Vec2 xy, Vec2 zw) {
  Vec4 result;
  result.x = xy.x;
  result.y = xy.y;
  result.z = zw.x;
  result.w = zw.y;
  return result;
}

inline Vec4i MakeVec4i(Vec2i xy, Vec2i zw) {
  Vec4i result;
  result.x = xy.x;
  result.y = xy.y;
  result.z = zw.x;
  result.w = zw.y;
  return result;
}

inline base::Vec2 MakeInvert(const base::Vec2& value) {
  return base::Vec2(1.0f / value.x, 1.0f / value.y);
}

}  // namespace base

#endif  //! BASE_MATH_VECTOR_H_

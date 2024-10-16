// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/math/vector.h"

namespace base {

std::ostream& operator<<(std::ostream& os, const Vec2i& value) {
  os << "Vec2i<" << value.x << ", " << value.y << ">";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Vec2& value) {
  os << "Vec2<" << value.x << ", " << value.y << ">";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Vec3i& value) {
  os << "Vec3i<" << value.x << ", " << value.y << ", " << value.z << ">";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Vec3& value) {
  os << "Vec3<" << value.x << ", " << value.y << ", " << value.z << ">";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Vec4i& value) {
  os << "Vec4i<" << value.x << ", " << value.y << ", " << value.z << ", "
     << value.w << ">";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Vec4& value) {
  os << "Vec4<" << value.x << ", " << value.y << ", " << value.z << ", "
     << value.w << ">";
  return os;
}

}  // namespace base

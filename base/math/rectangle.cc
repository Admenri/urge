// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/math/rectangle.h"

namespace base {

std::ostream& operator<<(std::ostream& os, const Rect& value) {
  os << "Rect <" << value.x << ", " << value.y << ", " << value.width << ", "
     << value.height << ">";
  return os;
}

std::ostream& operator<<(std::ostream& os, const RectF& value) {
  os << "RectF <" << value.x << ", " << value.y << ", " << value.width << ", "
     << value.height << ">";
  return os;
}

base::Rect MakeIntersect(const base::Rect& A, const base::Rect& B) {
  int Amin, Amax, Bmin, Bmax;
  base::Rect result;

  // Horizontal intersection
  Amin = A.x;
  Amax = Amin + A.width;
  Bmin = B.x;
  Bmax = Bmin + B.width;
  if (Bmin > Amin) {
    Amin = Bmin;
  }
  result.x = Amin;
  if (Bmax < Amax) {
    Amax = Bmax;
  }
  result.width = std::max(0, Amax - Amin);

  // Vertical intersection
  Amin = A.y;
  Amax = Amin + A.height;
  Bmin = B.y;
  Bmax = Bmin + B.height;
  if (Bmin > Amin) {
    Amin = Bmin;
  }
  result.y = Amin;
  if (Bmax < Amax) {
    Amax = Bmax;
  }
  result.height = std::max(0, Amax - Amin);

  return result;
}

RectF Rect::ToFloatRect() const {
  return RectF(static_cast<float>(x), static_cast<float>(y),
               static_cast<float>(width), static_cast<float>(height));
}

}  // namespace base

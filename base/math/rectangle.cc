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

RectF Rect::ToFloatRect() const {
  return RectF(static_cast<float>(x), static_cast<float>(y),
               static_cast<float>(width), static_cast<float>(height));
}

}  // namespace base

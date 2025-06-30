// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/math/rectangle.h"

#include <stdint.h>

namespace base {

base::Rect MakeIntersect(const base::Rect& A, const base::Rect& B) {
  int32_t Amin, Amax, Bmin, Bmax;
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

}  // namespace base

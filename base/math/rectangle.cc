// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/math/rectangle.h"

namespace base {

RectF Rect::ToFloatRect() const {
  return RectF(static_cast<float>(x), static_cast<float>(y),
               static_cast<float>(width), static_cast<float>(height));
}

}  // namespace base

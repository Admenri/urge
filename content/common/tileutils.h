// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_TILEUTILS_H_
#define CONTENT_COMMON_TILEUTILS_H_

#include "content/public/drawable.h"
#include "content/public/graphics.h"
#include "content/public/table.h"
#include "renderer/drawable/quad_array.h"

namespace content {

template <typename T>
struct Sides {
  T left, right, top, bottom;
};

template <typename T>
struct Corners {
  T top_left, top_right, bottom_left, bottom_right;
};

inline int vwrap(int value, int range) {
  int res = value % range;
  return res < 0 ? res + range : res;
};

inline int16_t TableGetWrapped(scoped_refptr<content::Table> t,
                               int x,
                               int y,
                               int z = 0) {
  return t->Get(vwrap(x, t->GetXSize()), vwrap(y, t->GetYSize()), z);
}

inline int16_t TableGetFlag(scoped_refptr<content::Table> t, int x) {
  if (!t)
    return 0;

  if (x < 0 || x >= t->GetXSize())
    return 0;

  return t->At(x);
}

}  // namespace content

#endif  // !CONTENT_COMMON_TILEUTILS_H_

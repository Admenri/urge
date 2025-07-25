// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/layout/uniform_layout.h"

#include <cstring>

namespace renderer {

void MakeIdentityMatrix(float* out) {
  std::memset(out, 0, sizeof(float) * 16);
  out[0] = 1.0f;
  out[5] = 1.0f;
  out[10] = 1.0f;
  out[15] = 1.0f;
}

void MakeTransformMatrix(float* out,
                         const base::Vec2& size,
                         const base::Vec2& offset) {
  std::memset(out, 0, sizeof(float) * 16);
  out[0] = 1.0f;
  out[5] = 1.0f;
  out[10] = 1.0f;
  out[15] = 1.0f;

  out[3] = (2.0f * offset.x) / size.x;
  out[7] = (2.0f * offset.y) / size.y;
}

void MakeProjectionMatrix(float* out, const base::Vec2& size) {
  const float aa = 2.0f / size.x;
  const float bb = -2.0f / size.y;
  const float cc = 1.0f;

  std::memset(out, 0, sizeof(float) * 16);
  out[0] = aa;
  out[5] = bb;
  out[10] = cc;
  out[15] = 1.0f;

  out[3] = -1.0f;
  out[7] = 1.0f;
}

}  // namespace renderer

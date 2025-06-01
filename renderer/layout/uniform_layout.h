// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_LAYOUT_UNIFORM_LAYOUT_H_
#define RENDERER_LAYOUT_UNIFORM_LAYOUT_H_

#include "base/math/vector.h"

namespace renderer {

struct alignas(16) WorldTransform {
  float projection[16];
  float transform[16];
};

// Note: engine required row major data layout.
//
// Projection matrix data view: (transpose)
// | 2/w                  |
// |      -2/h            |
// |              1       |
// | -1     1     0     1 |
//
// wgpu coordinate system:
// pos:              tex:
// |      1      |      |(0,0) -+   |
// |      |      |      | |         |
// | -1 <-+->  1 |      | +         |
// |      |      |      |           |
// |     -1      |      |      (1,1)|
//
// Result calculate:
// ndc_x = (in_x * 2) / w - 1
// in_x = 100, w = 500, ndc_x = -0.6

void MakeIdentityMatrix(float* out);

void MakeTransformMatrix(float* out,
                         const base::Vec2& size,
                         const base::Vec2& offset);

void MakeProjectionMatrix(float* out, const base::Vec2& size);

}  // namespace renderer

#endif  //! RENDERER_LAYOUT_UNIFORM_LAYOUT_H_

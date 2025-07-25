// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MATH_VECTOR_H_
#define BASE_MATH_VECTOR_H_

#include <stdint.h>
#include <sstream>

#include "Common/interface/BasicMath.hpp"

namespace base {

using Vec2 = Diligent::float2;
using Vec3 = Diligent::float3;
using Vec4 = Diligent::float4;

using Vec2i = Diligent::int2;
using Vec3i = Diligent::int3;
using Vec4i = Diligent::int4;

using Mat3 = Diligent::float3x3;
using Mat4 = Diligent::float4x4;

}  // namespace base

#endif  //! BASE_MATH_VECTOR_H_

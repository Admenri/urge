// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <algorithm>

#include "glm/glm.hpp"

#include "core/common/definition.h"
#include "core/common/utility.h"
#include "core/scene3d/node3d.h"

namespace urge {

URGE_BINDING()
class Light3D : public Node3D {
 public:
  URGE_BINDING()
  Light3D();
  ~Light3D() override;

  URGE_BINDING()
  ATTR(uint32_t, Type) {
    if (value.has_value())
      type_ = *value;
    return type_;
  }

  URGE_BINDING()
  ATTR(MaybePtr<Color>, Color) {
    if (value.has_value()) {
      if (*value)
        color_ = *value;
      return std::nullopt;
    }
    return color_;
  }

  URGE_BINDING()
  ATTR(float, Intensity) {
    if (value.has_value())
      intensity_ = std::max(0.0f, *value);
    return intensity_;
  }

  URGE_BINDING()
  ATTR(float, Range) {
    if (value.has_value())
      range_ = std::max(0.01f, *value);
    return range_;
  }

  URGE_BINDING()
  ATTR(bool, CastShadow) {
    if (value.has_value())
      cast_shadow_ = *value;
    return cast_shadow_;
  }

  uint32_t type() const { return type_; }
  glm::vec3 color_value() const { return glm::vec3(color_->data) / 255.0f; }
  float intensity() const { return intensity_; }
  float range() const { return range_; }
  bool cast_shadow() const { return cast_shadow_; }

 private:
  uint32_t type_ = 0;
  RefPtr<Color> color_ = MakeRefCounted<Color>(255.0f, 255.0f, 255.0f, 255.0f);
  float intensity_ = 1.0f;
  float range_ = 10.0f;
  bool cast_shadow_ = false;
};

}

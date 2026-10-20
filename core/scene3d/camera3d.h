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
#include <cstdint>
#include <vector>

#include "glm/glm.hpp"

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"

namespace urge {

URGE_BINDING()
class Camera3D : public Object {
 public:
  URGE_BINDING()
  Camera3D();
  ~Camera3D() override;

  URGE_BINDING()
  ATTR(RefPtr<Vector3>, Position) {
    if (value.has_value()) {
      if (*value)
        position_ = *value;
      return std::nullopt;
    }
    return position_;
  }

  URGE_BINDING()
  ATTR(RefPtr<Vector4>, Rotation) {
    if (value.has_value()) {
      if (*value)
        rotation_ = *value;
      return std::nullopt;
    }
    return rotation_;
  }

  URGE_BINDING()
  ATTR(float, Fov) {
    if (value.has_value())
      fov_ = std::clamp(*value, 1.0f, 179.0f);
    return fov_;
  }

  URGE_BINDING()
  ATTR(float, Near) {
    if (value.has_value())
      near_ = std::max(*value, 0.001f);
    return near_;
  }

  URGE_BINDING()
  ATTR(float, Far) {
    if (value.has_value())
      far_ = std::max(*value, near_ + 0.001f);
    return far_;
  }

  URGE_BINDING(Name : "orthographic")
  ATTR(bool, Orthographic) {
    if (value.has_value())
      orthographic_ = *value;
    return orthographic_;
  }

  URGE_BINDING(Name : "ortho_size")
  ATTR(float, OrthoSize) {
    if (value.has_value())
      ortho_size_ = std::max(*value, 0.01f);
    return ortho_size_;
  }

  glm::mat4 transform() const;
  glm::mat4 view() const;
  glm::mat4 projection(const glm::ivec2& size) const;
  glm::mat4 view_projection(const glm::ivec2& size) const;

  std::vector<float> ComputeSplits(int32_t count,
                                   float lambda,
                                   float max_distance) const;
  std::vector<glm::mat4> ComputeCascades(const glm::vec3& light_direction,
                                         int32_t count,
                                         float lambda,
                                         const glm::ivec2& size,
                                         float max_distance,
                                         int32_t shadow_size) const;

  URGE_BINDING()
  std::vector<float> ShadowSplits(int32_t count,
                                  float lambda,
                                  float max_distance);
  URGE_BINDING()
  std::vector<float> ShadowCascades(RefPtr<Vector3> light_direction,
                                    int32_t count,
                                    float lambda,
                                    int32_t width,
                                    int32_t height,
                                    float max_distance,
                                    int32_t shadow_size);

  URGE_BINDING()
  std::vector<float> ViewProjection(int32_t width, int32_t height);

  bool IsVisible(const glm::vec3& center,
                 float radius,
                 const glm::ivec2& size) const;


 private:
  RefPtr<Vector3> position_;
  RefPtr<Vector4> rotation_;
  float fov_ = 50.0f;
  float near_ = 0.1f;
  float far_ = 1000.0f;
  bool orthographic_ = false;
  float ortho_size_ = 10.0f;

};

}

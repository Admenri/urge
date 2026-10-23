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
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
#include "core/scene3d/node3d.h"

namespace urge {

URGE_BINDING()
class Camera3D : public Node3D {
 public:
  URGE_BINDING()
  Camera3D();
  ~Camera3D() override;

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

  URGE_BINDING(Name : "culling_mask")
  ATTR(uint32_t, CullingMask) {
    if (value.has_value())
      culling_mask_ = *value;
    return culling_mask_;
  }

  URGE_BINDING(Name : "clear_flags")
  ATTR(uint32_t, ClearFlags) {
    if (value.has_value())
      clear_flags_ = *value;
    return clear_flags_;
  }

  URGE_BINDING(Name : "clear_color")
  ATTR(RefPtr<Color>, ClearColor) {
    if (value.has_value()) {
      if (*value)
        clear_color_ = *value;
      return std::nullopt;
    }
    return clear_color_;
  }

  URGE_BINDING(Name : "priority")
  ATTR(int32_t, Priority) {
    if (value.has_value())
      priority_ = *value;
    return priority_;
  }

  URGE_BINDING(Name : "target")
  std::string GetTarget() const { return target_; }
  URGE_BINDING(Name : "target=")
  void SetTarget(std::string value) { target_ = std::move(value); }

  glm::mat4 transform() const { return world_matrix(); }
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
  bool Culled(const glm::vec3& center,
              float radius,
              const glm::ivec2& size,
              uint32_t layers) const;

  bool clear_color_enabled() const { return (clear_flags_ & 1u) != 0; }
  bool clear_depth_enabled() const { return (clear_flags_ & 2u) != 0; }
  bool clear_stencil_enabled() const { return (clear_flags_ & 4u) != 0; }
  uint32_t culling_mask() const { return culling_mask_; }
  const RefPtr<Color>& clear_color() const { return clear_color_; }
  int32_t priority() const { return priority_; }
  const std::string& target() const { return target_; }

 private:
  float fov_ = 50.0f;
  float near_ = 0.1f;
  float far_ = 1000.0f;
  bool orthographic_ = false;
  float ortho_size_ = 10.0f;
  uint32_t culling_mask_ = 0xFFFFFFFFu;
  uint32_t clear_flags_ = 3;
  RefPtr<Color> clear_color_ =
      MakeRefCounted<Color>(26.0f, 28.0f, 36.0f, 255.0f);
  int32_t priority_ = 0;
  std::string target_;
};

}

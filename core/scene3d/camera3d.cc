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

#include "core/scene3d/camera3d.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace urge {

namespace {

float AspectOf(const glm::ivec2& size) {
  const float width = static_cast<float>(std::max(1, size.x));
  const float height = static_cast<float>(std::max(1, size.y));
  return width / height;
}

}

Camera3D::Camera3D() = default;

Camera3D::~Camera3D() = default;

glm::mat4 Camera3D::view() const {
  return glm::inverse(transform());
}

glm::mat4 Camera3D::projection(const glm::ivec2& size) const {
  const float aspect = AspectOf(size);

  if (orthographic_) {
    const float half_height = ortho_size_;
    const float half_width = half_height * aspect;

    glm::mat4 result(1.0f);
    result[0][0] = 1.0f / half_width;
    result[1][1] = 1.0f / half_height;
    result[2][2] = 1.0f / (near_ - far_);
    result[3][2] = near_ / (near_ - far_);
    return result;
  }

  const float focal = 1.0f / std::tan(glm::radians(fov_) * 0.5f);
  const float depth = near_ - far_;

  glm::mat4 result(0.0f);
  result[0][0] = focal / aspect;
  result[1][1] = focal;
  result[2][2] = far_ / depth;
  result[2][3] = -1.0f;
  result[3][2] = near_ * far_ / depth;
  return result;
}

glm::mat4 Camera3D::view_projection(const glm::ivec2& size) const {
  return projection(size) * view();
}

std::vector<float> Camera3D::ViewProjection(int32_t width, int32_t height) {
  const glm::ivec2 size(std::max(1, width), std::max(1, height));
  const glm::mat4 matrix = view_projection(size);

  std::vector<float> result(16);
  const float* data = glm::value_ptr(matrix);
  for (size_t i = 0; i < 16; ++i)
    result[i] = data[i];
  return result;
}

std::vector<float> Camera3D::ComputeSplits(int32_t count,
                                           float lambda,
                                           float max_distance) const {
  const int32_t cascades = std::clamp(count, 1, 4);
  const float near_plane = near_;
  const float far_plane = std::max(max_distance, near_plane + 0.01f);
  const float ratio = far_plane / near_plane;

  std::vector<float> splits(cascades);
  for (int32_t i = 0; i < cascades; ++i) {
    const float p = static_cast<float>(i + 1) / static_cast<float>(cascades);
    const float logarithmic = near_plane * std::pow(ratio, p);
    const float uniform = near_plane + (far_plane - near_plane) * p;
    splits[i] = lambda * logarithmic + (1.0f - lambda) * uniform;
  }
  splits[cascades - 1] = far_plane;
  return splits;
}

std::vector<glm::mat4> Camera3D::ComputeCascades(
    const glm::vec3& light_direction,
    int32_t count,
    float lambda,
    const glm::ivec2& size,
    float max_distance,
    int32_t shadow_size) const {
  const int32_t cascades = std::clamp(count, 1, 4);
  const std::vector<float> splits =
      ComputeSplits(cascades, lambda, max_distance);

  const float aspect = AspectOf(size);
  const float tan_half = std::tan(glm::radians(fov_) * 0.5f);
  const glm::mat4 camera_to_world = transform();

  glm::vec3 direction = light_direction;
  if (glm::length(direction) <= 0.0f)
    direction = glm::vec3(0.0f, -1.0f, 0.0f);
  direction = glm::normalize(direction);

  glm::vec3 up(0.0f, 1.0f, 0.0f);
  if (std::abs(glm::dot(direction, up)) > 0.99f)
    up = glm::vec3(0.0f, 0.0f, 1.0f);

  std::vector<glm::mat4> matrices(cascades, glm::mat4(1.0f));

  float near_plane = near_;
  for (int32_t cascade = 0; cascade < cascades; ++cascade) {
    const float far_plane = splits[cascade];

    glm::vec3 corners[8];
    int32_t index = 0;
    for (int32_t side = 0; side < 2; ++side) {
      const float depth = side == 0 ? near_plane : far_plane;
      const float half_height = tan_half * depth;
      const float half_width = half_height * aspect;

      for (int32_t corner = 0; corner < 4; ++corner) {
        const float x = (corner == 1 || corner == 2) ? half_width : -half_width;
        const float y =
            (corner == 2 || corner == 3) ? half_height : -half_height;
        const glm::vec4 world = camera_to_world * glm::vec4(x, y, -depth, 1.0f);
        corners[index++] = glm::vec3(world);
      }
    }

    glm::vec3 center(0.0f);
    for (const glm::vec3& corner : corners)
      center += corner;
    center /= 8.0f;

    float radius = 0.0f;
    for (const glm::vec3& corner : corners)
      radius = std::max(radius, glm::length(corner - center));
    radius = std::max(radius, 0.5f);
    radius = std::ceil(radius * 4.0f) / 4.0f;

    const glm::mat4 light_view =
        glm::lookAt(center - direction * (radius * 2.0f), center, up);

    glm::vec3 center_light = glm::vec3(light_view * glm::vec4(center, 1.0f));
    const float texel =
        (radius * 2.0f) / static_cast<float>(std::max(shadow_size, 16));
    center_light.x = std::floor(center_light.x / texel) * texel;
    center_light.y = std::floor(center_light.y / texel) * texel;

    glm::mat4 scale(1.0f);
    scale[0][0] = 1.0f / radius;
    scale[1][1] = 1.0f / radius;
    scale[2][2] = -1.0f / (radius * 2.0f);
    scale[3][2] = 0.5f;

    matrices[cascade] =
        scale * glm::translate(glm::mat4(1.0f), -center_light) * light_view;

    near_plane = far_plane;
  }

  return matrices;
}

std::vector<float> Camera3D::ShadowSplits(int32_t count,
                                          float lambda,
                                          float max_distance) {
  return ComputeSplits(count, lambda, max_distance);
}

std::vector<float> Camera3D::ShadowCascades(RefPtr<Vector3> light_direction,
                                            int32_t count,
                                            float lambda,
                                            int32_t width,
                                            int32_t height,
                                            float max_distance,
                                            int32_t shadow_size) {
  const glm::vec3 direction =
      light_direction ? light_direction->data : glm::vec3(0.0f, -1.0f, 0.0f);
  const std::vector<glm::mat4> matrices =
      ComputeCascades(direction, count, lambda,
                      glm::ivec2(std::max(1, width), std::max(1, height)),
                      max_distance, shadow_size);

  std::vector<float> result;
  result.reserve(matrices.size() * 16);
  for (const glm::mat4& matrix : matrices) {
    const float* data = glm::value_ptr(matrix);
    for (size_t i = 0; i < 16; ++i)
      result.push_back(data[i]);
  }
  return result;
}

bool Camera3D::IsVisible(const glm::vec3& center,
                         float radius,
                         const glm::ivec2& size) const {
  const glm::mat4 matrix = view_projection(size);

  const auto row = [&matrix](int index) {
    return glm::vec4(matrix[0][index], matrix[1][index], matrix[2][index],
                     matrix[3][index]);
  };

  const glm::vec4 planes[6] = {
      row(3) + row(0), row(3) - row(0), row(3) + row(1),
      row(3) - row(1), row(3) + row(2), row(3) - row(2),
  };

  for (const glm::vec4& plane : planes) {
    const float length = glm::length(glm::vec3(plane));
    if (length <= 0.0f)
      continue;

    const glm::vec3 normal = glm::vec3(plane) / length;
    const float distance = plane.w / length;
    if (glm::dot(normal, center) + distance < -radius)
      return false;
  }

  return true;
}

bool Camera3D::Culled(const glm::vec3& center,
                      float radius,
                      const glm::ivec2& size,
                      uint32_t layers) const {
  if ((layers & culling_mask_) == 0)
    return true;
  return !IsVisible(center, radius, size);
}

}

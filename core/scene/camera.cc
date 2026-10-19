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

#include "core/camera.h"

#include <algorithm>

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

#include "core/gpu_utils.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/shader.h"

namespace urge {

namespace {

float AspectOf(const glm::ivec2& size) {
  const float width = static_cast<float>(std::max(1, size.x));
  const float height = static_cast<float>(std::max(1, size.y));
  return width / height;
}

}  // namespace

Camera::Camera()
    : position_(MakeRefCounted<Vector3>(0.0f)),
      quaternion_(MakeRefCounted<Vector4>(0.0f, 0.0f, 0.0f, 1.0f)),
      scale_(MakeRefCounted<Vector3>(1.0f)) {}

Camera::~Camera() {
  scene_group_ = nullptr;
  scene_uniform_ = nullptr;
}

ATTR_DEF(Camera, RefPtr<Vector3>, Position) {
  if (value.has_value()) {
    if (*value)
      position_ = *value;
    return std::nullopt;
  } else {
    return position_;
  }
}

ATTR_DEF(Camera, RefPtr<Vector4>, Quaternion) {
  if (value.has_value()) {
    if (*value)
      quaternion_ = *value;
    return std::nullopt;
  } else {
    return quaternion_;
  }
}

ATTR_DEF(Camera, RefPtr<Vector3>, Scale) {
  if (value.has_value()) {
    if (*value)
      scale_ = *value;
    return std::nullopt;
  } else {
    return scale_;
  }
}

glm::mat4 Camera::transform() const {
  const auto& position = position_->data;
  const auto& quaternion = quaternion_->data;

  glm::mat4 result = glm::translate(glm::mat4(1.0f), position);
  result *= glm::mat4_cast(
      glm::quat(quaternion.w, quaternion.x, quaternion.y, quaternion.z));
  return glm::scale(result, scale_->data);
}

glm::mat4 Camera::view() const {
  return glm::inverse(transform());
}

glm::mat4 Camera::projection(const glm::ivec2& size) const {
  return glm::ortho(0.0f, static_cast<float>(std::max(1, size.x)),
                    static_cast<float>(std::max(1, size.y)), 0.0f);
}

wgpu::BindGroup Camera::AcquireScene(const glm::ivec2& size) {
  if (!scene_uniform_) {
    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    desc.size = sizeof(SceneData);
    scene_uniform_ = g_device.CreateBuffer(&desc);

    if (!scene_uniform_) {
      LOGGER_ERROR("the device rejected the scene uniform of a camera");
      return nullptr;
    }

    util::BufferSet binding(scene_uniform_);
    scene_group_ = util::CreateBindGroup(
        ShaderSet::Get().state.bitmap.texture_pma.GetBindGroupLayout(0),
        {{0, binding}});

    if (!scene_group_) {
      LOGGER_ERROR("the device rejected the scene set of a camera");
      return nullptr;
    }
  }

  const SceneData data = {projection(size) * view()};
  g_queue.WriteBuffer(scene_uniform_, 0, &data, sizeof(data));

  return scene_group_;
}

PerspectiveCamera::PerspectiveCamera() = default;
PerspectiveCamera::~PerspectiveCamera() = default;

ATTR_DEF(PerspectiveCamera, float, Fov) {
  if (value.has_value()) {
    fov_ = std::clamp(*value, 1.0f, 179.0f);
    return std::nullopt;
  } else {
    return fov_;
  }
}

ATTR_DEF(PerspectiveCamera, float, Near) {
  if (value.has_value()) {
    near_ = std::max(*value, 0.001f);
    return std::nullopt;
  } else {
    return near_;
  }
}

ATTR_DEF(PerspectiveCamera, float, Far) {
  if (value.has_value()) {
    far_ = std::max(*value, near_ + 0.001f);
    return std::nullopt;
  } else {
    return far_;
  }
}

glm::mat4 PerspectiveCamera::projection(const glm::ivec2& size) const {
  const glm::mat4 lens =
      glm::perspective(glm::radians(fov_), AspectOf(size), near_, far_);

  return lens * glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
}

OrthographicCamera::OrthographicCamera() = default;
OrthographicCamera::~OrthographicCamera() = default;

ATTR_DEF(OrthographicCamera, float, Size) {
  if (value.has_value()) {
    size_ = std::max(*value, 0.0f);
    return std::nullopt;
  } else {
    return size_;
  }
}

ATTR_DEF(OrthographicCamera, float, Near) {
  if (value.has_value()) {
    near_ = *value;
    return std::nullopt;
  } else {
    return near_;
  }
}

ATTR_DEF(OrthographicCamera, float, Far) {
  if (value.has_value()) {
    far_ = *value;
    return std::nullopt;
  } else {
    return far_;
  }
}

glm::mat4 OrthographicCamera::projection(const glm::ivec2& size) const {
  const float height = static_cast<float>(std::max(1, size.y));

  const float half_height = size_ > 0.0f ? size_ : height * 0.5f;
  const float half_width = half_height * AspectOf(size);

  return glm::ortho(0.0f, half_width * 2.0f, half_height * 2.0f, 0.0f, near_,
                    far_);
}

}  // namespace urge

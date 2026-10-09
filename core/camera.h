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

#include <cstdint>

#include "glm/glm.hpp"

#include "core/definition.h"
#include "core/device.h"
#include "core/object.h"
#include "core/utility.h"

namespace urge {

URGE_BINDING()
class Camera : public Object {
 public:
  URGE_BINDING()
  Camera();
  ~Camera() override;

  URGE_BINDING()
  ATTR(RefPtr<Vector3>, Position);
  URGE_BINDING()
  ATTR(RefPtr<Vector4>, Quaternion);
  URGE_BINDING()
  ATTR(RefPtr<Vector3>, Scale);

  glm::mat4 transform() const;
  glm::mat4 view() const;

  virtual glm::mat4 projection(const glm::ivec2& size) const;
  wgpu::BindGroup AcquireScene(const glm::ivec2& size);

 protected:
  RefPtr<Vector3> position_;
  RefPtr<Vector4> quaternion_;
  RefPtr<Vector3> scale_;

 private:
  wgpu::Buffer scene_uniform_ = nullptr;
  wgpu::BindGroup scene_group_ = nullptr;
};

URGE_BINDING()
class PerspectiveCamera : public Camera {
 public:
  URGE_BINDING()
  PerspectiveCamera();
  ~PerspectiveCamera() override;

  URGE_BINDING()
  ATTR(float, Fov);
  URGE_BINDING()
  ATTR(float, Near);
  URGE_BINDING()
  ATTR(float, Far);

  glm::mat4 projection(const glm::ivec2& size) const override;

 private:
  float fov_ = 60.0f;
  float near_ = 0.1f;
  float far_ = 1000.0f;
};

URGE_BINDING()
class OrthographicCamera : public Camera {
 public:
  URGE_BINDING()
  OrthographicCamera();
  ~OrthographicCamera() override;

  URGE_BINDING()
  ATTR(float, Size);
  URGE_BINDING()
  ATTR(float, Near);
  URGE_BINDING()
  ATTR(float, Far);

  glm::mat4 projection(const glm::ivec2& size) const override;

 private:
  float size_ = 0.0f;
  float near_ = -1000.0f;
  float far_ = 1000.0f;
};

}  // namespace urge

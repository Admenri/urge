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
#include <map>
#include <vector>

#include "glm/glm.hpp"

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/scene3d/material3d.h"
#include "core/scene3d/mesh3d.h"
#include "core/scene3d/node3d.h"

namespace urge {

URGE_BINDING()
class MeshRenderer3D : public Node3D {
 public:
  URGE_BINDING()
  MeshRenderer3D();
  ~MeshRenderer3D() override;

  URGE_BINDING(Name : "mesh")
  ATTR(MaybePtr<Mesh3D>, Mesh);

  URGE_BINDING()
  void SetMaterial(int32_t slot, MaybePtr<Material3D> material);
  URGE_BINDING()
  RefPtr<Material3D> GetMaterial(int32_t slot) const;
  URGE_BINDING(Name : "material_count")
  int32_t MaterialCount() const {
    return static_cast<int32_t>(materials_.size());
  }

  URGE_BINDING(Name : "cast_shadow")
  ATTR(bool, CastShadow) {
    if (value.has_value())
      cast_shadow_ = *value;
    return cast_shadow_;
  }

  URGE_BINDING(Name : "receive_shadow")
  ATTR(bool, ReceiveShadow) {
    if (value.has_value())
      receive_shadow_ = *value;
    return receive_shadow_;
  }

  URGE_BINDING(Name : "render_queue")
  ATTR(int32_t, RenderQueue) {
    if (value.has_value())
      render_queue_ = *value;
    return render_queue_;
  }

  URGE_BINDING()
  std::vector<float> BoundsCenter();
  URGE_BINDING()
  std::vector<float> BoundsRadius();

  URGE_BINDING()
  void SetInstanceMatrices(std::vector<float> data);
  URGE_BINDING(Name : "instance_count")
  int32_t InstanceCount() const { return instance_count_; }

  void UpdateBounds();
  glm::vec3 world_center() const { return world_center_; }
  float world_radius() const { return world_radius_; }
  const std::map<int32_t, RefPtr<Material3D>>& materials() const {
    return materials_;
  }
  RefPtr<Mesh3D> mesh() const { return mesh_; }
  bool cast_shadow() const { return cast_shadow_; }
  bool receive_shadow() const { return receive_shadow_; }
  int32_t render_queue() const { return render_queue_; }
  wgpu::Buffer instance_buffer() const { return instance_buffer_; }

 private:
  RefPtr<Mesh3D> mesh_;
  std::map<int32_t, RefPtr<Material3D>> materials_;

  bool cast_shadow_ = true;
  bool receive_shadow_ = true;
  int32_t render_queue_ = 2000;

  glm::vec3 world_center_ = glm::vec3(0.0f);
  float world_radius_ = 0.0f;

  std::vector<float> instance_matrices_;
  wgpu::Buffer instance_buffer_ = nullptr;
  int32_t instance_count_ = 0;
};

}

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
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/render/uniform.h"
#include "core/scene3d/material3d.h"
#include "core/scene3d/mesh3d.h"
#include "core/scene3d/mesh_renderer3d.h"

namespace urge {

class Camera3D;

URGE_BINDING()
class RenderQueue3D : public Object {
 public:
  struct Item {
    RefPtr<MeshRenderer3D> renderer;
    RefPtr<Mesh3D> mesh;
    RefPtr<MaterialPass3D> pass;
    std::string tag;
    uint32_t index_offset = 0;
    uint32_t index_count = 0;
    uint32_t layers = 1;
    int32_t queue = 2000;
    int32_t tag_id = 0;
    float distance = 0.0f;
    glm::mat4 model = glm::mat4(1.0f);
    uint64_t state_key = 0;
    UniformBlockPool::Slot slot;
  };

  struct LightItem {
    int32_t type = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
    bool cast_shadow = false;
  };

  URGE_BINDING()
  RenderQueue3D();
  ~RenderQueue3D() override;

  void Clear();
  void Collect(Node3D* root,
               Camera3D* camera,
               const glm::ivec2& target_size,
               UniformBlockPool& pool);

  const std::vector<Item>& items() const { return items_; }
  const std::vector<LightItem>& lights() const { return lights_; }

  std::vector<const Item*> Filter(const std::vector<std::string>& tags,
                                  const std::vector<int32_t>& tag_ids,
                                  int32_t queue_min,
                                  int32_t queue_max,
                                  uint32_t layers) const;
  static void SortByDistance(std::vector<const Item*>& items,
                             bool back_to_front);

  URGE_BINDING(Name : "count")
  int32_t Count() const { return static_cast<int32_t>(items_.size()); }
  URGE_BINDING(Name : "light_count")
  int32_t LightCount() const { return static_cast<int32_t>(lights_.size()); }
  URGE_BINDING()
  std::vector<std::string> Tags() const;

 private:
  void CollectNode(Node3D* node,
                   const glm::mat4& parent,
                   Camera3D* camera,
                   const glm::vec3& eye,
                   const glm::ivec2& target_size,
                   UniformBlockPool& pool);

  std::vector<Item> items_;
  std::vector<LightItem> lights_;
};

}

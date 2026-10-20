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

#include <map>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/render/uniform.h"

namespace urge {

class Camera3D;
class Mesh3D;
class Node3D;

URGE_BINDING()
class RenderQueue3D : public Object {
 public:
  struct LightItem {
    uint32_t type = 0;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    float range = 10.0f;
    bool cast_shadow = false;
  };

  struct Item {
    RefPtr<Mesh3D> mesh;
    std::string tag;
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 center = glm::vec3(0.0f);
    float distance = 0.0f;
    const void* pipeline_key = nullptr;
    UniformBlockPool::Slot slot = {};
  };

  RenderQueue3D() = default;
  ~RenderQueue3D() override = default;

  void Collect(Node3D* root,
               Camera3D* camera,
               const glm::ivec2& target_size,
               UniformBlockPool& pool);
  void Clear();

  const std::vector<Item>* Items(const std::string& tag) const;
  const Item* Find(Mesh3D* mesh, const std::string& tag) const;
  const std::vector<LightItem>& lights() const { return lights_; }

  URGE_BINDING()
  int32_t TagCount() const;
  URGE_BINDING()
  std::vector<std::string> Tags() const;
  URGE_BINDING()
  int32_t Count(std::string tag) const;
  URGE_BINDING()
  int32_t TotalCount() const;
  URGE_BINDING()
  int32_t LightCount() const {
    return static_cast<int32_t>(lights_.size());
  }
  URGE_BINDING()
  RefPtr<Mesh3D> MeshAt(std::string tag, int32_t index) const;
  URGE_BINDING()
  float DistanceAt(std::string tag, int32_t index) const;
  URGE_BINDING()
  void SortByDistance(std::string tag, bool back_to_front);

 private:
  void CollectNode(Node3D* node,
                   const glm::mat4& parent,
                   Camera3D* camera,
                   const glm::vec3& eye,
                   const glm::ivec2& target_size,
                   UniformBlockPool& pool);

  std::map<std::string, std::vector<Item>> tags_;
  std::vector<LightItem> lights_;
};

}

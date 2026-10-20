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

#include "core/scene3d/render_queue3d.h"

#include <algorithm>
#include <utility>

#include "core/scene3d/camera3d.h"
#include "core/scene3d/light3d.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/material3d.h"
#include "core/scene3d/mesh3d.h"
#include "core/scene3d/node3d.h"

namespace urge {

namespace {

float MaxAxisScale(const glm::mat4& matrix) {
  const float x = glm::length(glm::vec3(matrix[0]));
  const float y = glm::length(glm::vec3(matrix[1]));
  const float z = glm::length(glm::vec3(matrix[2]));
  return std::max(x, std::max(y, z));
}

}

void RenderQueue3D::Clear() {
  tags_.clear();
  lights_.clear();
}

void RenderQueue3D::Collect(Node3D* root,
                            Camera3D* camera,
                            const glm::ivec2& target_size,
                            UniformBlockPool& pool) {
  Clear();
  if (!root)
    return;

  glm::vec3 eye(0.0f);
  if (camera)
    eye = glm::vec3(camera->transform()[3]);

  CollectNode(root, glm::mat4(1.0f), camera, eye, target_size, pool);

  for (auto& entry : tags_) {
    std::stable_sort(entry.second.begin(), entry.second.end(),
                     [](const Item& left, const Item& right) {
                       if (left.pipeline_key != right.pipeline_key)
                         return left.pipeline_key < right.pipeline_key;
                       return left.distance < right.distance;
                     });
  }
}

void RenderQueue3D::CollectNode(Node3D* node,
                                const glm::mat4& parent,
                                Camera3D* camera,
                                const glm::vec3& eye,
                                const glm::ivec2& target_size,
                                UniformBlockPool& pool) {
  if (!node->visible())
    return;

  const glm::mat4 world = parent * node->local_matrix();

  {
    if (Light3D* light = dynamic_cast<Light3D*>(node)) {
      LightItem item;
      item.type = light->type();
      item.position = glm::vec3(world[3]);
      item.direction = -glm::normalize(glm::vec3(world[2]));
      item.color = light->color_value();
      item.intensity = light->intensity();
      item.range = light->range();
      item.cast_shadow = light->cast_shadow();
      lights_.push_back(item);
    }

    Mesh3D* mesh = dynamic_cast<Mesh3D*>(node);
    RefPtr<Material3D> material = mesh ? mesh->material() : nullptr;
    if (mesh && material && mesh->index_count() > 0 && mesh->VertexCount() > 0) {
      const glm::vec3 center =
          glm::vec3(world * glm::vec4(mesh->bounds_center(), 1.0f));
      const float radius = mesh->bounds_radius() * MaxAxisScale(world);

      if (!camera || camera->IsVisible(center, radius, target_size)) {
        const MeshData mesh_data = {world};
        const UniformBlockPool::Slot slot = pool.Acquire(mesh_data);
        const float distance = glm::length(center - eye);

        for (const std::string& tag : material->Tags()) {
          RefPtr<MaterialPass3D> pass = material->GetPass(tag);
          if (!pass)
            continue;

          Item item;
          item.mesh = RefPtr<Mesh3D>(mesh);
          item.tag = tag;
          item.model = world;
          item.center = center;
          item.distance = distance;
          item.pipeline_key = pass->AcquirePipeline().Get();
          item.slot = slot;

          tags_[tag].push_back(std::move(item));
        }
      }
    }
  }

  for (const RefPtr<Node3D>& child : node->children())
    CollectNode(child.get(), world, camera, eye, target_size, pool);
}

const std::vector<RenderQueue3D::Item>* RenderQueue3D::Items(
    const std::string& tag) const {
  auto it = tags_.find(tag);
  return it == tags_.end() ? nullptr : &it->second;
}

const RenderQueue3D::Item* RenderQueue3D::Find(
    Mesh3D* mesh,
    const std::string& tag) const {
  const std::vector<Item>* items = Items(tag);
  if (!items)
    return nullptr;
  for (const Item& item : *items)
    if (item.mesh.get() == mesh)
      return &item;
  return nullptr;
}

int32_t RenderQueue3D::TagCount() const {
  return static_cast<int32_t>(tags_.size());
}

std::vector<std::string> RenderQueue3D::Tags() const {
  std::vector<std::string> result;
  result.reserve(tags_.size());
  for (const auto& entry : tags_)
    result.push_back(entry.first);
  return result;
}

int32_t RenderQueue3D::Count(std::string tag) const {
  const std::vector<Item>* items = Items(tag);
  return items ? static_cast<int32_t>(items->size()) : 0;
}

int32_t RenderQueue3D::TotalCount() const {
  int32_t total = 0;
  for (const auto& entry : tags_)
    total += static_cast<int32_t>(entry.second.size());
  return total;
}

RefPtr<Mesh3D> RenderQueue3D::MeshAt(std::string tag, int32_t index) const {
  const std::vector<Item>* items = Items(tag);
  if (!items || index < 0 || index >= static_cast<int32_t>(items->size()))
    return nullptr;
  return (*items)[index].mesh;
}

float RenderQueue3D::DistanceAt(std::string tag, int32_t index) const {
  const std::vector<Item>* items = Items(tag);
  if (!items || index < 0 || index >= static_cast<int32_t>(items->size()))
    return 0.0f;
  return (*items)[index].distance;
}

void RenderQueue3D::SortByDistance(std::string tag, bool back_to_front) {
  auto it = tags_.find(tag);
  if (it == tags_.end())
    return;

  if (back_to_front) {
    std::stable_sort(it->second.begin(), it->second.end(),
                     [](const Item& left, const Item& right) {
                       return left.distance > right.distance;
                     });
  } else {
    std::stable_sort(it->second.begin(), it->second.end(),
                     [](const Item& left, const Item& right) {
                       return left.distance < right.distance;
                     });
  }
}

}

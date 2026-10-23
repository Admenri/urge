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

#include "glm/gtc/matrix_inverse.hpp"

#include "core/scene3d/camera3d.h"
#include "core/scene3d/light3d.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/node3d.h"

namespace urge {

RenderQueue3D::RenderQueue3D() = default;

RenderQueue3D::~RenderQueue3D() = default;

void RenderQueue3D::Clear() {
  items_.clear();
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
    eye = glm::vec3(camera->world_matrix()[3]);

  CollectNode(root, glm::mat4(1.0f), camera, eye, target_size, pool);

  std::stable_sort(items_.begin(), items_.end(),
                   [](const Item& left, const Item& right) {
                     if (left.queue != right.queue)
                       return left.queue < right.queue;
                     if (left.state_key != right.state_key)
                       return left.state_key < right.state_key;
                     return left.distance < right.distance;
                   });
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

  if (Light3D* light = dynamic_cast<Light3D*>(node)) {
    LightItem item;
    item.type = static_cast<int32_t>(light->type());
    item.position = glm::vec3(world[3]);
    item.direction = -glm::normalize(glm::vec3(world[2]));
    item.color = light->color_value();
    item.intensity = light->intensity();
    item.range = light->range();
    item.cast_shadow = light->cast_shadow();
    lights_.push_back(item);
  }

  if (MeshRenderer3D* renderer = dynamic_cast<MeshRenderer3D*>(node)) {
    RefPtr<Mesh3D> mesh = renderer->mesh();
    const bool drawable = mesh && mesh->index_count() > 0 &&
                          mesh->VertexCount() > 0 &&
                          !renderer->materials().empty();
    if (drawable) {
      renderer->UpdateBounds();
      const glm::vec3 center = renderer->world_center();
      const float radius = renderer->world_radius();

      const bool culled = camera && camera->Culled(center, radius, target_size,
                                                   renderer->layers());

      if (!culled) {
        const MeshData mesh_data = {world, glm::inverseTranspose(world)};
        const UniformBlockPool::Slot slot = pool.Acquire(mesh_data);
        const float distance = glm::length(center - eye);

        for (const SubMesh3D& submesh : mesh->submeshes()) {
          const auto found = renderer->materials().find(
              static_cast<int32_t>(submesh.mat_slot));
          if (found == renderer->materials().end() || !found->second)
            continue;

          for (const std::string& tag : found->second->Tags()) {
            RefPtr<MaterialPass3D> pass = found->second->GetPass(tag);
            if (!pass)
              continue;

            Item item;
            item.renderer = RefPtr<MeshRenderer3D>(renderer);
            item.mesh = mesh;
            item.pass = pass;
            item.tag = tag;
            item.index_offset = submesh.index_offset;
            item.index_count = submesh.index_count;
            item.layers = renderer->layers();
            item.queue = pass->queue();
            item.tag_id = pass->tag_id();
            item.distance = distance;
            item.model = world;
            item.state_key = reinterpret_cast<uint64_t>(pass.get());
            item.slot = slot;
            items_.push_back(std::move(item));
          }
        }
      }
    }
  }

  for (const RefPtr<Node3D>& child : node->children())
    CollectNode(child.get(), world, camera, eye, target_size, pool);
}

std::vector<const RenderQueue3D::Item*> RenderQueue3D::Filter(
    const std::vector<std::string>& tags,
    const std::vector<int32_t>& tag_ids,
    int32_t queue_min,
    int32_t queue_max,
    uint32_t layers) const {
  std::vector<const Item*> result;
  result.reserve(items_.size());

  const bool all_tags = tags.empty() && tag_ids.empty();

  for (const Item& item : items_) {
    if ((item.layers & layers) == 0)
      continue;
    if (item.queue < queue_min || item.queue > queue_max)
      continue;

    if (!all_tags) {
      bool matched = false;
      for (const std::string& tag : tags) {
        if (item.tag == tag) {
          matched = true;
          break;
        }
      }
      if (!matched) {
        for (const int32_t id : tag_ids) {
          if (item.tag_id == id) {
            matched = true;
            break;
          }
        }
      }
      if (!matched)
        continue;
    }

    result.push_back(&item);
  }

  return result;
}

void RenderQueue3D::SortByDistance(std::vector<const Item*>& items,
                                   bool back_to_front) {
  if (back_to_front) {
    std::stable_sort(items.begin(), items.end(),
                     [](const Item* left, const Item* right) {
                       return left->distance > right->distance;
                     });
  } else {
    std::stable_sort(items.begin(), items.end(),
                     [](const Item* left, const Item* right) {
                       return left->distance < right->distance;
                     });
  }
}

std::vector<std::string> RenderQueue3D::Tags() const {
  std::vector<std::string> result;
  for (const Item& item : items_) {
    if (std::find(result.begin(), result.end(), item.tag) == result.end())
      result.push_back(item.tag);
  }
  return result;
}

}

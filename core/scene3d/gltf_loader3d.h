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
#include "core/scene3d/material3d.h"
#include "core/scene3d/mesh3d.h"
#include "core/scene3d/node3d.h"

namespace urge {

class Bitmap;

struct MaterialSource3D {
  RefPtr<Bitmap> texture;
  glm::vec4 color = glm::vec4(1.0f);
  float roughness = 0.7f;
  float metallic = 0.0f;
  bool transparent = false;
};

URGE_BINDING()
class GLTFModel3D : public Object {
 public:
  URGE_BINDING()
  explicit GLTFModel3D(std::string path);
  ~GLTFModel3D() override;

  URGE_BINDING(Name : "root")
  RefPtr<Node3D> GetRoot() const { return root_; }

  URGE_BINDING(Name : "material_count")
  int32_t MaterialCount() const {
    return static_cast<int32_t>(materials_.size());
  }
  URGE_BINDING(Name : "mesh_count")
  int32_t MeshCount() const { return static_cast<int32_t>(meshes_.size()); }
  URGE_BINDING(Name : "image_count")
  int32_t ImageCount() const { return static_cast<int32_t>(images_.size()); }
  URGE_BINDING(Name : "node_count")
  int32_t NodeCount() const { return node_count_; }

  URGE_BINDING()
  std::vector<float> BoundsCenter() const;
  URGE_BINDING()
  std::vector<float> BoundsSize() const;
  URGE_BINDING(Name : "bounds_radius")
  float BoundsRadius() const;

  URGE_BINDING()
  void Fit(float size);
  URGE_BINDING(Name : "unlit")
  ATTR(bool, Unlit) {
    if (value.has_value()) {
      if (*value != unlit_) {
        unlit_ = *value;
        RebuildMaterials();
      }
    }
    return unlit_;
  }
  URGE_BINDING()
  void SetMaterial(int32_t index, MaybePtr<Material3D> material);
  URGE_BINDING()
  RefPtr<Material3D> GetMaterial(int32_t index) const;

  const std::vector<RefPtr<Material3D>>& materials() const {
    return materials_;
  }
  const std::vector<RefPtr<Mesh3D>>& meshes() const { return meshes_; }

 private:
  void Load(std::string path);
  void RebuildMaterials();

  RefPtr<Node3D> root_;
  std::vector<RefPtr<Material3D>> materials_;
  std::vector<RefPtr<Mesh3D>> meshes_;
  std::vector<RefPtr<Bitmap>> images_;
  glm::vec3 bounds_min_ = glm::vec3(0.0f);
  glm::vec3 bounds_max_ = glm::vec3(0.0f);
  int32_t node_count_ = 0;
  bool unlit_ = false;
  std::vector<MaterialSource3D> material_sources_;
};

}

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
#include "core/common/object.h"
#include "core/scene3d/skeleton3d.h"

namespace urge {

struct SubMesh3D {
  uint32_t index_offset = 0;
  uint32_t index_count = 0;
  uint32_t mat_slot = 0;
};

URGE_BINDING()
class Mesh3D : public Object {
 public:
  URGE_BINDING()
  Mesh3D();
  ~Mesh3D() override;

  URGE_BINDING()
  void SetVertices(std::vector<float> data);
  URGE_BINDING()
  void SetSkinnedVertices(std::vector<float> data,
                          std::vector<uint32_t> joints,
                          std::vector<float> weights);
  URGE_BINDING()
  void SetIndices(std::vector<uint32_t> data);

  URGE_BINDING()
  void AddSubmesh(int32_t index_offset, int32_t index_count, int32_t mat_slot);
  URGE_BINDING()
  void ClearSubmeshes();
  URGE_BINDING()
  int32_t SubmeshCount() const {
    return static_cast<int32_t>(submeshes_.size());
  }
  URGE_BINDING()
  int32_t SubmeshIndexOffset(int32_t index) const;
  URGE_BINDING()
  int32_t SubmeshIndexCount(int32_t index) const;
  URGE_BINDING()
  int32_t SubmeshMatSlot(int32_t index) const;

  URGE_BINDING()
  ATTR(MaybePtr<Skeleton3D>, Skeleton);

  URGE_BINDING()
  int32_t VertexCount() const { return vertex_count_; }
  URGE_BINDING()
  int32_t IndexCount() const { return index_count_; }
  URGE_BINDING()
  int32_t BoneCount() const;
  URGE_BINDING(Name : "skinned")
  bool Skinned() const { return skinned_; }
  URGE_BINDING()
  std::vector<float> BoundsCenter();
  URGE_BINDING()
  std::vector<float> BoundsMin();
  URGE_BINDING()
  std::vector<float> BoundsMax();
  URGE_BINDING(Name : "bounds_radius")
  float BoundsRadius() const { return bounds_radius_; }

  const wgpu::Buffer& vertex_buffer() const { return vertex_buffer_; }
  const wgpu::Buffer& index_buffer() const { return index_buffer_; }
  int32_t index_count() const { return index_count_; }
  bool skinned() const { return skinned_; }
  glm::vec3 bounds_center() const { return bounds_center_; }
  float bounds_radius() const { return bounds_radius_; }
  RefPtr<Skeleton3D> skeleton() const { return skeleton_; }
  const std::vector<SubMesh3D>& submeshes() const { return submeshes_; }

  wgpu::Buffer AcquireJointBuffer(uint64_t frame_token);
  wgpu::BindGroup AcquireGroup(uint32_t chunk_index,
                               wgpu::Buffer mesh_buffer,
                               wgpu::Buffer joints);

 private:
  struct GroupCache {
    wgpu::Buffer mesh_buffer = nullptr;
    wgpu::Buffer joints = nullptr;
    wgpu::BindGroup group = nullptr;
  };

  void Upload();
  void UpdateBounds();

  RefPtr<Skeleton3D> skeleton_;

  std::vector<uint8_t> vertex_bytes_;
  std::vector<uint32_t> indices_;
  std::vector<SubMesh3D> submeshes_;
  bool skinned_ = false;
  uint32_t vertex_stride_ = 32;

  wgpu::Buffer vertex_buffer_ = nullptr;
  wgpu::Buffer index_buffer_ = nullptr;
  int32_t vertex_count_ = 0;
  int32_t index_count_ = 0;
  glm::vec3 bounds_center_ = glm::vec3(0.0f);
  glm::vec3 bounds_min_ = glm::vec3(0.0f);
  glm::vec3 bounds_max_ = glm::vec3(0.0f);
  float bounds_radius_ = 0.0f;

  wgpu::Buffer joint_buffer_ = nullptr;
  size_t joint_bytes_ = 0;
  uint64_t joint_frame_ = 0;

  std::map<uint32_t, GroupCache> groups_;
};

}

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

#include "core/scene3d/mesh3d.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

#include "core/common/exception.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/skeleton3d.h"

namespace urge {

namespace {

constexpr size_t kFloatsPerVertex = 8;
constexpr size_t kStaticStride = kFloatsPerVertex * sizeof(float);
constexpr size_t kSkinnedStride = kStaticStride + 4 * sizeof(uint32_t) +
                                  4 * sizeof(float);

}

Mesh3D::Mesh3D() = default;

Mesh3D::~Mesh3D() = default;

ATTR_DEF(Mesh3D, RefPtr<Skeleton3D>, Skeleton) {
  if (value.has_value()) {
    skeleton_ = *value;
    groups_.clear();
    return std::nullopt;
  } else {
    return skeleton_;
  }
}

void Mesh3D::SetVertices(std::vector<float> data) {
  if (data.size() % kFloatsPerVertex != 0)
    throw Exception(Exception::kRGSSError,
                    "mesh3d: vertices are eight floats per vertex "
                    "(position, normal, texcoord).");

  vertex_bytes_.resize(data.size() * sizeof(float));
  if (!data.empty())
    std::memcpy(vertex_bytes_.data(), data.data(), vertex_bytes_.size());

  vertex_count_ = static_cast<int32_t>(data.size() / kFloatsPerVertex);
  vertex_stride_ = static_cast<uint32_t>(kStaticStride);
  skinned_ = false;
  groups_.clear();

  UpdateBounds();
  Upload();
}

void Mesh3D::SetSkinnedVertices(std::vector<float> data,
                                std::vector<uint32_t> joints,
                                std::vector<float> weights) {
  if (data.size() % kFloatsPerVertex != 0)
    throw Exception(Exception::kRGSSError,
                    "mesh3d: vertices are eight floats per vertex "
                    "(position, normal, texcoord).");

  const size_t count = data.size() / kFloatsPerVertex;
  if (joints.size() != count * 4 || weights.size() != count * 4)
    throw Exception(Exception::kRGSSError,
                    "mesh3d: skinned vertices need four joint indices and four "
                    "weights per vertex.");

  vertex_bytes_.resize(count * kSkinnedStride);
  for (size_t i = 0; i < count; ++i) {
    uint8_t* target = vertex_bytes_.data() + i * kSkinnedStride;
    std::memcpy(target, data.data() + i * kFloatsPerVertex, kStaticStride);
    std::memcpy(target + kStaticStride, joints.data() + i * 4,
                4 * sizeof(uint32_t));
    std::memcpy(target + kStaticStride + 4 * sizeof(uint32_t),
                weights.data() + i * 4, 4 * sizeof(float));
  }

  vertex_count_ = static_cast<int32_t>(count);
  vertex_stride_ = static_cast<uint32_t>(kSkinnedStride);
  skinned_ = true;
  groups_.clear();

  UpdateBounds();
  Upload();
}

void Mesh3D::SetIndices(std::vector<uint32_t> data) {
  indices_ = std::move(data);
  index_count_ = static_cast<int32_t>(indices_.size());

  submeshes_.clear();
  if (index_count_ > 0) {
    SubMesh3D submesh;
    submesh.index_offset = 0;
    submesh.index_count = static_cast<uint32_t>(index_count_);
    submesh.mat_slot = 0;
    submeshes_.push_back(submesh);
  }

  Upload();
}

void Mesh3D::AddSubmesh(int32_t index_offset,
                        int32_t index_count,
                        int32_t mat_slot) {
  if (index_offset < 0 || index_count <= 0)
    throw Exception(Exception::kRGSSError,
                    "mesh3d: a submesh needs a non-negative offset and a "
                    "positive index count.");
  if (index_offset + index_count > index_count_)
    throw Exception(Exception::kRGSSError,
                    "mesh3d: the submesh range [{}, {}) is outside the index "
                    "buffer ({} indices).",
                    index_offset, index_offset + index_count, index_count_);

  SubMesh3D submesh;
  submesh.index_offset = static_cast<uint32_t>(index_offset);
  submesh.index_count = static_cast<uint32_t>(index_count);
  submesh.mat_slot = static_cast<uint32_t>(std::max(0, mat_slot));
  submeshes_.push_back(submesh);
}

void Mesh3D::ClearSubmeshes() {
  submeshes_.clear();
}

int32_t Mesh3D::SubmeshIndexOffset(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(submeshes_.size()))
    return 0;
  return static_cast<int32_t>(submeshes_[index].index_offset);
}

int32_t Mesh3D::SubmeshIndexCount(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(submeshes_.size()))
    return 0;
  return static_cast<int32_t>(submeshes_[index].index_count);
}

int32_t Mesh3D::SubmeshMatSlot(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(submeshes_.size()))
    return 0;
  return static_cast<int32_t>(submeshes_[index].mat_slot);
}

int32_t Mesh3D::BoneCount() const {
  return skeleton_ ? skeleton_->BoneCount() : 0;
}

std::vector<float> Mesh3D::BoundsCenter() {
  return {bounds_center_.x, bounds_center_.y, bounds_center_.z};
}

std::vector<float> Mesh3D::BoundsMin() {
  return {bounds_min_.x, bounds_min_.y, bounds_min_.z};
}

std::vector<float> Mesh3D::BoundsMax() {
  return {bounds_max_.x, bounds_max_.y, bounds_max_.z};
}

void Mesh3D::UpdateBounds() {
  glm::vec3 minimum(std::numeric_limits<float>::max());
  glm::vec3 maximum(std::numeric_limits<float>::lowest());

  for (int32_t i = 0; i < vertex_count_; ++i) {
    float values[3];
    std::memcpy(values, vertex_bytes_.data() + i * vertex_stride_,
                sizeof(values));
    const glm::vec3 point(values[0], values[1], values[2]);
    minimum = glm::min(minimum, point);
    maximum = glm::max(maximum, point);
  }

  if (vertex_count_ > 0) {
    bounds_min_ = minimum;
    bounds_max_ = maximum;
    bounds_center_ = (minimum + maximum) * 0.5f;
    bounds_radius_ = glm::length(maximum - bounds_center_);
  } else {
    bounds_min_ = glm::vec3(0.0f);
    bounds_max_ = glm::vec3(0.0f);
    bounds_center_ = glm::vec3(0.0f);
    bounds_radius_ = 0.0f;
  }
}

void Mesh3D::Upload() {
  if (!vertex_bytes_.empty()) {
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
    descriptor.size = vertex_bytes_.size();
    vertex_buffer_ = GPUDevice::Get().device().CreateBuffer(&descriptor);
    if (!vertex_buffer_)
      throw Exception(Exception::kGPUError,
                      "mesh3d: the device rejected the vertex buffer.");
    GPUDevice::Get().queue().WriteBuffer(vertex_buffer_, 0,
                                         vertex_bytes_.data(),
                                         descriptor.size);
  }

  if (!indices_.empty()) {
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::Index | wgpu::BufferUsage::CopyDst;
    descriptor.size = indices_.size() * sizeof(uint32_t);
    index_buffer_ = GPUDevice::Get().device().CreateBuffer(&descriptor);
    if (!index_buffer_)
      throw Exception(Exception::kGPUError,
                      "mesh3d: the device rejected the index buffer.");
    GPUDevice::Get().queue().WriteBuffer(index_buffer_, 0, indices_.data(),
                                         descriptor.size);
  }
}

wgpu::Buffer Mesh3D::AcquireJointBuffer(uint64_t frame_token) {
  if (!skeleton_ || skeleton_->BoneCount() == 0)
    return nullptr;

  if (joint_buffer_ && joint_frame_ == frame_token)
    return joint_buffer_;

  const std::vector<glm::mat4>& matrices = skeleton_->joint_matrices();
  const size_t bytes = matrices.size() * sizeof(glm::mat4);

  bool recreated = false;
  if (!joint_buffer_ || joint_bytes_ < bytes) {
    wgpu::BufferDescriptor descriptor;
    descriptor.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
    descriptor.size = std::max<size_t>(bytes, sizeof(glm::mat4));
    joint_buffer_ = GPUDevice::Get().device().CreateBuffer(&descriptor);
    if (!joint_buffer_)
      throw Exception(Exception::kGPUError,
                      "mesh3d: the device rejected the joint buffer.");
    joint_bytes_ = descriptor.size;
    recreated = true;
  }

  GPUDevice::Get().queue().WriteBuffer(joint_buffer_, 0, matrices.data(), bytes);

  joint_frame_ = frame_token;
  if (recreated)
    groups_.clear();
  return joint_buffer_;
}

wgpu::BindGroup Mesh3D::AcquireGroup(uint32_t chunk_index,
                                     wgpu::Buffer mesh_buffer,
                                     wgpu::Buffer joints) {
  if (!joints)
    joints = AcquireDummyJointBuffer();

  auto it = groups_.find(chunk_index);
  if (it != groups_.end() && it->second.mesh_buffer.Get() == mesh_buffer.Get() &&
      it->second.joints.Get() == joints.Get())
    return it->second.group;

  wgpu::BindGroupEntry entries[2] = {};
  entries[0].binding = 0;
  entries[0].buffer = mesh_buffer;
  entries[0].offset = 0;
  entries[0].size = sizeof(MeshData);
  entries[1].binding = 1;
  entries[1].buffer = joints;
  entries[1].offset = 0;
  entries[1].size = WGPU_WHOLE_SIZE;

  wgpu::BindGroupDescriptor descriptor;
  descriptor.layout = AcquireMeshLayout();
  descriptor.entryCount = 2;
  descriptor.entries = entries;

  GroupCache cache;
  cache.mesh_buffer = mesh_buffer;
  cache.joints = joints;
  cache.group = GPUDevice::Get().device().CreateBindGroup(&descriptor);
  if (!cache.group)
    throw Exception(Exception::kGPUError,
                    "mesh3d: the device rejected the mesh group.");

  groups_[chunk_index] = std::move(cache);
  return groups_[chunk_index].group;
}

}

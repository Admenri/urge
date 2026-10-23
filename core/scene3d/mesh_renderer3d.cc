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

#include "core/scene3d/mesh_renderer3d.h"

#include <algorithm>
#include <utility>

#include "core/common/exception.h"
#include "core/render/device.h"

namespace urge {

namespace {

float MaxAxisScale(const glm::mat4& matrix) {
  const float x = glm::length(glm::vec3(matrix[0]));
  const float y = glm::length(glm::vec3(matrix[1]));
  const float z = glm::length(glm::vec3(matrix[2]));
  return std::max(x, std::max(y, z));
}

}

MeshRenderer3D::MeshRenderer3D() = default;

MeshRenderer3D::~MeshRenderer3D() = default;

ATTR_DEF(MeshRenderer3D, RefPtr<Mesh3D>, Mesh) {
  if (value.has_value()) {
    mesh_ = *value;
    UpdateBounds();
    return std::nullopt;
  }
  return mesh_;
}

void MeshRenderer3D::SetMaterial(int32_t slot, RefPtr<Material3D> material) {
  if (slot < 0)
    throw Exception(Exception::kRGSSError,
                    "mesh renderer 3d: a material slot cannot be negative.");
  if (material)
    materials_[slot] = std::move(material);
  else
    materials_.erase(slot);
}

RefPtr<Material3D> MeshRenderer3D::GetMaterial(int32_t slot) const {
  const auto it = materials_.find(slot);
  return it == materials_.end() ? nullptr : it->second;
}

std::vector<float> MeshRenderer3D::BoundsCenter() {
  UpdateBounds();
  return {world_center_.x, world_center_.y, world_center_.z};
}

std::vector<float> MeshRenderer3D::BoundsRadius() {
  UpdateBounds();
  return {world_radius_};
}

void MeshRenderer3D::UpdateBounds() {
  if (!mesh_) {
    world_center_ = glm::vec3(0.0f);
    world_radius_ = 0.0f;
    return;
  }

  const glm::mat4 world = world_matrix();
  world_center_ = glm::vec3(world * glm::vec4(mesh_->bounds_center(), 1.0f));
  world_radius_ = mesh_->bounds_radius() * MaxAxisScale(world);
}

void MeshRenderer3D::SetInstanceMatrices(std::vector<float> data) {
  if (data.size() % 16 != 0)
    throw Exception(Exception::kRGSSError,
                    "mesh renderer 3d: instance matrices are sixteen floats "
                    "per instance.");

  instance_matrices_ = std::move(data);
  instance_count_ = static_cast<int32_t>(instance_matrices_.size() / 16);

  if (instance_count_ == 0) {
    instance_buffer_ = nullptr;
    return;
  }

  wgpu::BufferDescriptor descriptor;
  descriptor.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  descriptor.size = instance_matrices_.size() * sizeof(float);
  instance_buffer_ = GPUDevice::Get().device().CreateBuffer(&descriptor);
  if (!instance_buffer_)
    throw Exception(Exception::kGPUError,
                    "mesh renderer 3d: the device rejected the instance "
                    "buffer.");

  GPUDevice::Get().queue().WriteBuffer(
      instance_buffer_, 0, instance_matrices_.data(), descriptor.size);
}

}

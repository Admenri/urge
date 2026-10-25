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

#include "core/scene3d/skeleton3d.h"

#include <cstring>
#include <utility>

#include "core/common/exception.h"

namespace urge {

Skeleton3D::Skeleton3D() = default;

Skeleton3D::~Skeleton3D() = default;

void Skeleton3D::SetBones(URGE_EXCEPT, std::vector<int32_t> parents,
                          std::vector<float> inverse_bind) {
  const size_t count = parents.size();
  if (inverse_bind.size() != count * 16) {
    exception_state.Throw(ExceptionState::kRGSSError, "skeleton3d: inverse bind matrices are sixteen floats per "
                    "bone.");
    return;
  }
  std::vector<RefPtr<Node3D>> bones;
  bones.reserve(count);
  for (size_t i = 0; i < count; ++i)
    bones.push_back(MakeRefCounted<Node3D>());

  for (size_t i = 0; i < count; ++i) {
    const int32_t parent = parents[i];
    if (parent < 0)
      continue;
    if (parent >= static_cast<int32_t>(count)) {
      exception_state.Throw(ExceptionState::kRGSSError, "skeleton3d: bone {} points at the missing parent {}.",
                      static_cast<int32_t>(i), parent);
      return;
    }
    if (parent == static_cast<int32_t>(i)) {
      exception_state.Throw(ExceptionState::kRGSSError, "skeleton3d: bone {} cannot parent itself.",
                      static_cast<int32_t>(i));
      return;
    }
    bones[i]->SetParent(bones[parent]);
  }

  inverse_bind_.resize(count);
  for (size_t i = 0; i < count; ++i)
    std::memcpy(&inverse_bind_[i], &inverse_bind[i * 16], sizeof(glm::mat4));

  bones_ = std::move(bones);
  joints_.clear();
}

RefPtr<Node3D> Skeleton3D::BoneAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(bones_.size()))
    return nullptr;
  return bones_[index];
}

int32_t Skeleton3D::BoneParent(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(bones_.size()))
    return -1;

  Node3D* parent = bones_[index]->GetParent().get();
  if (!parent)
    return -1;

  for (size_t i = 0; i < bones_.size(); ++i)
    if (bones_[i].get() == parent)
      return static_cast<int32_t>(i);
  return -1;
}

const std::vector<glm::mat4>& Skeleton3D::joint_matrices() {
  joints_.resize(bones_.size());
  for (size_t i = 0; i < bones_.size(); ++i)
    joints_[i] = bones_[i]->world_matrix() * inverse_bind_[i];
  return joints_;
}

std::vector<float> Skeleton3D::JointMatrices() {
  const std::vector<glm::mat4>& matrices = joint_matrices();

  std::vector<float> result(matrices.size() * 16);
  for (size_t i = 0; i < matrices.size(); ++i)
    std::memcpy(&result[i * 16], &matrices[i], sizeof(glm::mat4));
  return result;
}

void Skeleton3D::BeginPose() {
  pose_.assign(bones_.size(), PoseSample{});
  pose_open_ = true;
}

void Skeleton3D::BlendBone(int32_t index,
                           const glm::vec3& position,
                           const glm::quat& rotation,
                           const glm::vec3& scale,
                           float weight,
                           const std::vector<float>* mask) {
  if (index < 0 || index >= static_cast<int32_t>(pose_.size()))
    return;
  if (mask && index < static_cast<int32_t>(mask->size()))
    weight *= (*mask)[index];
  if (weight <= 0.0f)
    return;

  PoseSample& sample = pose_[index];
  if (!sample.sampled) {
    sample.position = position * weight;
    sample.rotation = rotation * weight;
    sample.scale = scale * weight;
    sample.weight = weight;
    sample.sampled = true;
    return;
  }

  const glm::quat aligned =
      glm::dot(sample.rotation, rotation) < 0.0f ? -rotation : rotation;
  sample.position += position * weight;
  sample.rotation += aligned * weight;
  sample.scale += scale * weight;
  sample.weight += weight;
}

void Skeleton3D::CaptureRestPose() {
  rest_.resize(bones_.size());
  for (size_t i = 0; i < bones_.size(); ++i) {
    const glm::vec4 rotation = bones_[i]->local_rotation();
    rest_[i].position = bones_[i]->local_position();
    rest_[i].rotation =
        glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);
    rest_[i].scale = bones_[i]->local_scale();
  }
}

void Skeleton3D::ResetPose() {
  if (rest_.size() != bones_.size())
    return;

  for (size_t i = 0; i < bones_.size(); ++i) {
    bones_[i]->SetLocalPosition(rest_[i].position);
    bones_[i]->SetLocalRotation(
        glm::vec4(rest_[i].rotation.x, rest_[i].rotation.y, rest_[i].rotation.z,
                  rest_[i].rotation.w));
    bones_[i]->SetLocalScale(rest_[i].scale);
  }
}

void Skeleton3D::EndPose() {
  for (size_t i = 0; i < pose_.size(); ++i) {
    const PoseSample& sample = pose_[i];
    if (!sample.sampled || sample.weight <= 0.0f)
      continue;

    const float inverse = 1.0f / sample.weight;
    bones_[i]->SetLocalPosition(sample.position * inverse);
    bones_[i]->SetLocalScale(sample.scale * inverse);

    glm::quat rotation = sample.rotation * inverse;
    if (glm::length(rotation) <= 0.0f)
      rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    else
      rotation = glm::normalize(rotation);
    bones_[i]->SetLocalRotation(
        glm::vec4(rotation.x, rotation.y, rotation.z, rotation.w));
  }

  pose_open_ = false;
}

}

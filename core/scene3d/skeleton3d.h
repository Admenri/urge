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
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/scene3d/node3d.h"

namespace urge {

URGE_BINDING()
class Skeleton3D : public Object {
 public:
  URGE_BINDING()
  Skeleton3D();
  ~Skeleton3D() override;

  URGE_BINDING()
  void SetBones(std::vector<int32_t> parents,
                std::vector<float> inverse_bind);

  URGE_BINDING()
  int32_t BoneCount() const { return static_cast<int32_t>(bones_.size()); }
  URGE_BINDING()
  RefPtr<Node3D> BoneAt(int32_t index) const;
  URGE_BINDING()
  int32_t BoneParent(int32_t index) const;

  URGE_BINDING()
  std::vector<float> JointMatrices();

  URGE_BINDING()
  void BeginPose();
  URGE_BINDING()
  void EndPose();

  URGE_BINDING()
  void CaptureRestPose();
  URGE_BINDING()
  void ResetPose();

  void BlendBone(int32_t index,
                 const glm::vec3& position,
                 const glm::quat& rotation,
                 const glm::vec3& scale,
                 float weight,
                 const std::vector<float>* mask = nullptr);
  bool pose_open() const { return pose_open_; }

  const std::vector<glm::mat4>& joint_matrices();
  const std::vector<RefPtr<Node3D>>& bones() const { return bones_; }

 private:
  struct BoneRest {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
  };

  struct PoseSample {
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    float weight = 0.0f;
    bool sampled = false;
  };

  std::vector<RefPtr<Node3D>> bones_;
  std::vector<glm::mat4> inverse_bind_;
  std::vector<glm::mat4> joints_;
  std::vector<PoseSample> pose_;
  std::vector<BoneRest> rest_;
  bool pose_open_ = false;
};

}

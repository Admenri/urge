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

#include <memory>
#include <optional>
#include <string>

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
#include "core/render/uniform.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/render_queue3d.h"
#include "core/scene3d/render_target3d.h"

namespace urge {

class Camera3D;
class GPURenderPassEncoder;
class Mesh3D;
class Node3D;

URGE_BINDING()
class RenderContext3D : public Object {
 public:
  RenderContext3D(wgpu::CommandEncoder encoder, RefPtr<RenderTarget3D> target);
  ~RenderContext3D() override;

  void Collect(Node3D* root, Camera3D* camera);
  void Flush();
  void SetShadow(RefPtr<RenderTarget3D> target, const glm::mat4& view_proj);
  void SetShadow(RefPtr<RenderTarget3D> target,
                 const glm::mat4& view_proj,
                 int32_t cascade);
  void SetShadowSplits(std::vector<float> splits);
  void SetShadowCascades(int32_t count);
  void SetShadowFade(float ratio);
  void SetPointShadow(RefPtr<RenderTarget3D> target,
                      const std::vector<glm::mat4>& face_matrices,
                      int32_t light_index,
                      float range);

  URGE_BINDING(Name : "target")
  RefPtr<RenderTarget3D> GetTarget() { return target_; }
  URGE_BINDING(Name : "camera")
  RefPtr<Camera3D> GetCamera() { return camera_; }
  URGE_BINDING(Name : "queue")
  RefPtr<RenderQueue3D> GetQueue() { return queue_; }

  URGE_BINDING(Name : "ambient")
  RefPtr<Color> GetAmbient() const { return ambient_; }
  URGE_BINDING(Name : "ambient=")
  void SetAmbient(RefPtr<Color> value);

  URGE_BINDING()
  void SetViewProjection(RefPtr<Camera3D> camera);
  void SetViewOverride(const glm::mat4& view_proj);

  URGE_BINDING()
  void BeginPass(RefPtr<RenderTarget3D> target,
                 RefPtr<Color> clear = nullptr,
                 bool clear_depth = true);
  void BeginPass(RefPtr<RenderTarget3D> target,
                 RefPtr<Color> clear,
                 bool clear_depth,
                 int32_t layer);
  URGE_BINDING()
  void EndPass();
  URGE_BINDING(Name : "pass")
  RefPtr<GPURenderPassEncoder> GetPass();

  URGE_BINDING()
  void Draw(RefPtr<Mesh3D> mesh, std::string tag);
  URGE_BINDING()
  int32_t DrawTag(std::string tag);

 private:
  void UpdateView(bool shadow_pass, int32_t cascade);
  void UpdateGlobals();

  wgpu::CommandEncoder encoder_ = nullptr;
  RefPtr<RenderTarget3D> target_;
  RefPtr<Camera3D> camera_;
  glm::mat4 view_override_ = glm::mat4(1.0f);
  bool view_override_enabled_ = false;
  RefPtr<RenderQueue3D> queue_;
  glm::ivec2 pass_size_ = glm::ivec2(1);
  wgpu::RenderPassEncoder pass_ = nullptr;
  uint64_t frame_token_ = 0;

  RefPtr<Color> ambient_;
  RefPtr<RenderTarget3D> shadow_target_;
  glm::mat4 shadow_matrix_ = glm::mat4(1.0f);
  glm::mat4 shadow_matrices_[kMaxCascades] = {};
  glm::vec4 shadow_splits_ = glm::vec4(0.0f);
  int32_t shadow_cascade_count_ = 1;
  float shadow_size_ = 1024.0f;
  float shadow_fade_ = 0.15f;
  RefPtr<RenderTarget3D> point_shadow_target_;
  glm::mat4 point_faces_[6] = {};
  int32_t point_light_index_ = -1;
  float point_range_ = 10.0f;
  bool view_dirty_ = true;

  wgpu::Buffer view_buffer_ = nullptr;
  wgpu::Buffer shadow_view_buffer_ = nullptr;
  uint32_t shadow_view_offset_ = 0;
  wgpu::BindGroup view_group_ = nullptr;
  wgpu::BindGroup shadow_view_group_ = nullptr;

  wgpu::Buffer globals_buffer_ = nullptr;
  wgpu::BindGroup globals_group_ = nullptr;

  std::unique_ptr<UniformBlockPool> mesh_pool_;
};

}

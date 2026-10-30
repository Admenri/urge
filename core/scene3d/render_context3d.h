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

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
#include "core/render/uniform.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/material3d.h"
#include "core/scene3d/render_queue3d.h"
#include "core/scene3d/render_target3d.h"

namespace urge {

class Camera3D;
class GPUBuffer;
class GPURenderPassEncoder;
class Node3D;

URGE_BINDING()
class RenderContext3D : public Object {
 public:
  RenderContext3D(URGE_EXCEPT,
                  wgpu::CommandEncoder encoder,
                  RefPtr<RenderTarget3D> target);
  ~RenderContext3D() override;

  void BeginFrame(wgpu::CommandEncoder encoder, RefPtr<RenderTarget3D> target);
  void Collect(Node3D* root, Camera3D* camera);
  void Flush();

  URGE_BINDING(Name : "target")
  RefPtr<RenderTarget3D> GetTarget() { return target_; }
  URGE_BINDING(Name : "camera")
  RefPtr<Camera3D> GetCamera() { return camera_; }
  URGE_BINDING(Name : "queue")
  RefPtr<RenderQueue3D> GetQueue() { return queue_; }

  URGE_BINDING(Name : "ambient")
  RefPtr<Color> GetAmbient() const { return ambient_; }
  URGE_BINDING(Name : "ambient=")
  void SetAmbient(MaybePtr<Color> value);

  URGE_BINDING()
  void SetViewProjection(MaybePtr<Camera3D> camera);
  void SetViewOverride(const glm::mat4& view_proj);

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

  URGE_BINDING()
  void BeginPass(URGE_EXCEPT,
                 RefPtr<RenderTarget3D> target,
                 MaybePtr<Color> clear = nullptr,
                 bool clear_depth = true);
  URGE_BINDING()
  void BeginPassEx(URGE_EXCEPT,
                   RefPtr<RenderTarget3D> target,
                   std::vector<RefPtr<Color>> clears,
                   bool clear_depth,
                   bool clear_stencil,
                   int32_t layer);
  URGE_BINDING()
  void EndPass();
  URGE_BINDING(Name : "pass")
  RefPtr<GPURenderPassEncoder> GetPass();

  URGE_BINDING()
  int32_t DrawFilter(URGE_EXCEPT,
                     std::vector<std::string> tags,
                     std::vector<int32_t> tag_ids,
                     int32_t queue_min,
                     int32_t queue_max,
                     uint32_t layers);
  URGE_BINDING()
  bool DrawItem(URGE_EXCEPT, int32_t index);

  void Draw(URGE_EXCEPT, const RenderQueue3D::Item& item);
  void DrawFullscreen(URGE_EXCEPT, RefPtr<MaterialPass3D> pass);
  void SetShadowPassMode(bool value) { shadow_pass_mode_ = value; }
  void SetCullOverride(int32_t value) { cull_override_ = value; }
  void SetSortBackToFront(bool value) { sort_back_to_front_ = value; }

  URGE_BINDING()
  void SetViewport(float x, float y, float width, float height);
  URGE_BINDING()
  void SetScissorRect(int32_t x, int32_t y, int32_t width, int32_t height);
  URGE_BINDING()
  void DrawIndexedIndirect(MaybePtr<GPUBuffer> buffer, int32_t offset);

  wgpu::CommandEncoder encoder() const { return encoder_; }
  wgpu::RenderPassEncoder pass() const { return pass_; }
  PipelineTargetState3D target_state() const;
  uint64_t frame_token() const { return frame_token_; }

 private:
  struct GlobalsSlot {
    wgpu::Buffer buffer = nullptr;
    wgpu::BindGroup group = nullptr;
    uint64_t frame = 0;
    glm::ivec2 viewport = glm::ivec2(0);
  };

  void UploadView(URGE_EXCEPT, int32_t slot);
  void EnsureShadowViewGroup(URGE_EXCEPT);
  void UpdateGlobals(URGE_EXCEPT, int32_t slot);
  wgpu::BindGroup GlobalsGroup(URGE_EXCEPT, int32_t slot);

  wgpu::CommandEncoder encoder_ = nullptr;
  RefPtr<RenderTarget3D> target_;
  RefPtr<Camera3D> camera_;
  glm::mat4 view_override_ = glm::mat4(1.0f);
  bool view_override_enabled_ = false;
  RefPtr<RenderQueue3D> queue_;
  glm::ivec2 pass_size_ = glm::ivec2(1);
  wgpu::RenderPassEncoder pass_ = nullptr;
  uint64_t frame_token_ = 0;
  int32_t pass_layer_ = 0;

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

  int32_t pass_serial_ = 0;
  bool shadow_pass_mode_ = false;
  int32_t cull_override_ = -1;
  bool sort_back_to_front_ = false;
  std::array<bool, 16> view_uploaded_ = {};

  wgpu::Buffer view_buffer_ = nullptr;
  uint32_t view_stride_ = 0;
  wgpu::BindGroup view_group_ = nullptr;
  wgpu::BindGroup shadow_view_group_ = nullptr;
  const void* view_group_shadow_target_ = nullptr;
  const void* view_group_point_target_ = nullptr;

  bool has_frame_clock_ = false;
  double last_frame_seconds_ = 0.0;
  float frame_time_ = 0.0f;
  float frame_delta_ = 0.0f;
  uint64_t globals_frame_ = 0;

  std::vector<GlobalsSlot> globals_slots_;

  std::unique_ptr<UniformBlockPool> mesh_pool_;
};

}

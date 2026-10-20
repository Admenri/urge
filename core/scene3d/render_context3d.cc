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

#include "core/scene3d/render_context3d.h"

#include <algorithm>
#include <chrono>
#include <utility>

#include "core/common/exception.h"
#include "core/graphics/gpu.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/scene3d/camera3d.h"
#include "core/scene3d/lighting3d.h"
#include "core/scene3d/material3d.h"
#include "core/scene3d/mesh3d.h"
#include "core/scene3d/render_target3d.h"

namespace urge {

namespace {

double ClockSeconds() {
  static const auto start = std::chrono::steady_clock::now();
  const auto now = std::chrono::steady_clock::now();
  return std::chrono::duration<double>(now - start).count();
}

uint64_t NextFrameToken() {
  static uint64_t token = 0;
  return ++token;
}

}

RenderContext3D::RenderContext3D(wgpu::CommandEncoder encoder,
                                 RefPtr<RenderTarget3D> target)
    : encoder_(encoder),
      target_(std::move(target)),
      ambient_(MakeRefCounted<Color>(26.0f, 28.0f, 36.0f, 255.0f)) {
  mesh_pool_ = std::make_unique<UniformBlockPool>(AcquireMeshUniformLayout(),
                                                  sizeof(MeshData), "mesh3d");
  mesh_pool_->BeginFrame();
}

RenderContext3D::~RenderContext3D() = default;

void RenderContext3D::Collect(Node3D* root, Camera3D* camera) {
  camera_ = camera;
  if (!queue_)
    queue_ = MakeRefCounted<RenderQueue3D>();

  frame_token_ = NextFrameToken();
  pass_size_ = target_ ? target_->size() : glm::ivec2(1);
  queue_->Collect(root, camera_.get(), pass_size_, *mesh_pool_);
  view_dirty_ = true;
}

void RenderContext3D::Flush() {
  mesh_pool_->Flush();
}

void RenderContext3D::SetAmbient(RefPtr<Color> value) {
  ambient_ = value ? value : MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 255.0f);
  view_dirty_ = true;
}

void RenderContext3D::SetViewProjection(RefPtr<Camera3D> camera) {
  camera_ = std::move(camera);
  view_override_enabled_ = false;
  view_dirty_ = true;
}

void RenderContext3D::SetViewOverride(const glm::mat4& view_proj) {
  view_override_ = view_proj;
  view_override_enabled_ = true;
  view_dirty_ = true;
}

void RenderContext3D::SetShadow(RefPtr<RenderTarget3D> target,
                                const glm::mat4& view_proj) {
  SetShadow(std::move(target), view_proj, 0);
  shadow_cascade_count_ = 1;
}

void RenderContext3D::SetShadow(RefPtr<RenderTarget3D> target,
                                const glm::mat4& view_proj,
                                int32_t cascade) {
  if (cascade < 0 || cascade >= static_cast<int32_t>(kMaxCascades))
    return;

  if (target) {
    shadow_target_ = target;
    shadow_size_ = static_cast<float>(std::max(1, target->Width()));
  }
  shadow_matrices_[cascade] = view_proj;
  shadow_matrix_ = shadow_matrices_[0];
  shadow_cascade_count_ = std::max(shadow_cascade_count_, cascade + 1);
  view_dirty_ = true;
}

void RenderContext3D::SetShadowSplits(std::vector<float> splits) {
  shadow_splits_ = glm::vec4(0.0f);
  for (size_t i = 0; i < splits.size() && i < kMaxCascades; ++i)
    shadow_splits_[static_cast<int>(i)] = splits[i];
  view_dirty_ = true;
}

void RenderContext3D::SetShadowFade(float ratio) {
  shadow_fade_ = std::clamp(ratio, 0.0f, 0.5f);
  view_dirty_ = true;
}

void RenderContext3D::SetPointShadow(
    RefPtr<RenderTarget3D> target,
    const std::vector<glm::mat4>& face_matrices,
    int32_t light_index,
    float range) {
  point_shadow_target_ = std::move(target);
  point_light_index_ = light_index;
  point_range_ = std::max(range, 0.01f);
  for (uint32_t i = 0; i < 6; ++i)
    point_faces_[i] = i < face_matrices.size() ? face_matrices[i]
                                               : glm::mat4(1.0f);
  view_dirty_ = true;
}

void RenderContext3D::SetShadowCascades(int32_t count) {
  shadow_cascade_count_ =
      std::clamp(count, 1, static_cast<int32_t>(kMaxCascades));
  view_dirty_ = true;
}

void RenderContext3D::UpdateView(bool shadow_pass, int32_t cascade) {
  const wgpu::Device device = GPUDevice::Get().device();

  constexpr uint32_t kViewStride = static_cast<uint32_t>(
      ((sizeof(ViewData) + 255u) / 256u) * 256u);
  const uint32_t cascade_index =
      static_cast<uint32_t>(std::clamp(cascade, 0, static_cast<int32_t>(kMaxCascades) - 1));

  wgpu::Buffer& buffer = shadow_pass ? shadow_view_buffer_ : view_buffer_;
  const uint64_t needed = shadow_pass ? kViewStride * kMaxCascades
                                      : sizeof(ViewData);
  if (buffer == nullptr || buffer.GetSize() < needed) {
    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    desc.size = needed;
    buffer = device.CreateBuffer(&desc);
    if (buffer == nullptr)
      throw Exception(Exception::kGPUError,
                      "render context 3d: the device rejected the view buffer.");
  }

  ViewData data = {};
  if (view_override_enabled_)
    data.view_proj = view_override_;
  else if (camera_)
    data.view_proj = camera_->view_projection(pass_size_);
  if (camera_)
    data.camera_position = glm::vec4(glm::vec3(camera_->transform()[3]), 0.0f);
  if (ambient_) {
    const glm::vec4 color = ambient_->Normalize();
    data.ambient = glm::vec4(color.r, color.g, color.b, 0.0f);
  }

  uint32_t count = 0;
  if (queue_) {
    const std::vector<RenderQueue3D::LightItem>& lights = queue_->lights();
    count = static_cast<uint32_t>(std::min<size_t>(lights.size(), kMaxLights));
    for (uint32_t i = 0; i < count; ++i) {
      const RenderQueue3D::LightItem& light = lights[i];
      LightData& target = data.lights[i];
      target.position_type =
          glm::vec4(light.position, static_cast<float>(light.type));
      target.direction_range = glm::vec4(light.direction, light.range);
      target.color_intensity = glm::vec4(light.color, light.intensity);
    }
  }
  data.ambient.w = static_cast<float>(count);
  data.light_view_proj = shadow_matrix_;

  data.cascade_splits = shadow_splits_;
  data.cascade_params =
      glm::vec4(static_cast<float>(shadow_cascade_count_),
                1.0f / std::max(shadow_size_, 1.0f), shadow_fade_, 0.0f);
  for (uint32_t i = 0; i < kMaxCascades; ++i)
    data.cascade_matrices[i] = shadow_matrices_[i];

  if (camera_)
    data.camera_forward = glm::vec4(-glm::vec3(camera_->transform()[2]), 0.0f);

  data.point_shadow_params =
      glm::vec4(static_cast<float>(point_light_index_), point_range_, 0.0f, 0.0f);
  for (uint32_t i = 0; i < 6; ++i)
    data.point_face_view_proj[i] = point_faces_[i];

  if (shadow_pass) {
    shadow_view_offset_ = cascade_index * kViewStride;
    GPUDevice::Get().queue().WriteBuffer(buffer, shadow_view_offset_, &data,
                                         sizeof(data));

    if (shadow_view_group_ != nullptr)
      return;

    wgpu::BindGroupEntry entry = {};
    entry.binding = 0;
    entry.buffer = buffer;
    entry.offset = 0;
    entry.size = sizeof(ViewData);

    wgpu::BindGroupDescriptor desc;
    desc.layout = AcquireShadowViewLayout();
    desc.entryCount = 1;
    desc.entries = &entry;
    shadow_view_group_ = device.CreateBindGroup(&desc);
    if (shadow_view_group_ == nullptr)
      throw Exception(Exception::kGPUError,
                      "render context 3d: the device rejected the shadow view "
                      "group.");
    return;
  }

  GPUDevice::Get().queue().WriteBuffer(buffer, 0, &data, sizeof(data));

  if (view_group_ != nullptr)
    return;

  const wgpu::TextureView shadow_view =
      shadow_target_ ? shadow_target_->depth_view() : AcquireEmptyShadowView();
  const wgpu::TextureView shadow_array =
      shadow_target_ ? shadow_target_->depth_array_view()
                     : AcquireEmptyShadowArrayView();

  wgpu::BindGroupEntry entries[7] = {};
  entries[0].binding = 0;
  entries[0].buffer = buffer;
  entries[0].offset = 0;
  entries[0].size = sizeof(ViewData);
  entries[1].binding = 1;
  entries[1].textureView = shadow_view;
  entries[2].binding = 2;
  entries[2].sampler = AcquireShadowSampler();
  entries[3].binding = 3;
  entries[3].textureView = shadow_array;
  entries[4].binding = 4;
  entries[4].sampler = AcquireShadowSampler();
  entries[5].binding = 5;
  entries[5].textureView = point_shadow_target_
                               ? point_shadow_target_->depth_cube_view()
                               : AcquireEmptyShadowCubeView();
  entries[6].binding = 6;
  entries[6].sampler = AcquireShadowSampler();

  wgpu::BindGroupDescriptor desc;
  desc.layout = AcquireViewLayout();
  desc.entryCount = 7;
  desc.entries = entries;
  view_group_ = device.CreateBindGroup(&desc);
  if (view_group_ == nullptr)
    throw Exception(Exception::kGPUError,
                    "render context 3d: the device rejected the view group.");
}

void RenderContext3D::UpdateGlobals() {
  if (globals_buffer_ == nullptr) {
    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    desc.size = sizeof(GlobalsData);
    globals_buffer_ = GPUDevice::Get().device().CreateBuffer(&desc);
    if (globals_buffer_ == nullptr)
      throw Exception(Exception::kGPUError,
                      "render context 3d: the device rejected the globals "
                      "buffer.");

    wgpu::BindGroupEntry entry = {};
    entry.binding = 0;
    entry.buffer = globals_buffer_;
    entry.offset = 0;
    entry.size = sizeof(GlobalsData);

    wgpu::BindGroupDescriptor desc_group;
    desc_group.layout = AcquireGlobalsLayout();
    desc_group.entryCount = 1;
    desc_group.entries = &entry;
    globals_group_ = GPUDevice::Get().device().CreateBindGroup(&desc_group);
    if (globals_group_ == nullptr)
      throw Exception(Exception::kGPUError,
                      "render context 3d: the device rejected the globals "
                      "group.");
  }

  static double last_seconds = 0.0;
  static uint64_t frame_index = 0;

  const double seconds = ClockSeconds();
  const float delta = static_cast<float>(seconds - last_seconds);
  last_seconds = seconds;

  GlobalsData data = {};
  data.time = glm::vec4(static_cast<float>(seconds), delta,
                        static_cast<float>(frame_index++), 0.0f);
  const glm::vec2 size(static_cast<float>(pass_size_.x),
                       static_cast<float>(pass_size_.y));
  data.viewport = glm::vec4(size, 1.0f / size.x, 1.0f / size.y);

  GPUDevice::Get().queue().WriteBuffer(globals_buffer_, 0, &data,
                                       sizeof(data));
}

void RenderContext3D::BeginPass(RefPtr<RenderTarget3D> target,
                                RefPtr<Color> clear,
                                bool clear_depth) {
  BeginPass(std::move(target), std::move(clear), clear_depth, 0);
}

void RenderContext3D::BeginPass(RefPtr<RenderTarget3D> target,
                                RefPtr<Color> clear,
                                bool clear_depth,
                                int32_t layer) {
  if (!target)
    throw Exception(Exception::kRGSSError, "render pass needs a target.");
  if (!encoder_)
    throw Exception(Exception::kRGSSError, "render context has no encoder.");

  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }

  std::optional<glm::vec4> color = std::nullopt;
  if (clear)
    color = clear->Normalize();

  target_ = target;
  pass_size_ = target->size();
  pass_ = target->BeginRendering(encoder_, color, clear_depth, layer);

  const bool shadow_pass =
      shadow_target_ && target.get() == shadow_target_.get();
  UpdateView(shadow_pass, layer);
}

void RenderContext3D::EndPass() {
  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }
}

RefPtr<GPURenderPassEncoder> RenderContext3D::GetPass() {
  if (!pass_)
    return nullptr;
  return MakeRefCounted<GPURenderPassEncoder>(pass_);
}

void RenderContext3D::Draw(RefPtr<Mesh3D> mesh, std::string tag) {
  if (!pass_)
    throw Exception(Exception::kRGSSError,
                    "no render pass is open, call begin_pass first.");
  if (!mesh)
    throw Exception(Exception::kRGSSError, "cannot draw a nil mesh.");

  if (!queue_)
    Collect(nullptr, camera_.get());

  const RenderQueue3D::Item* item = queue_->Find(mesh.get(), tag);
  if (!item)
    throw Exception(Exception::kRGSSError,
                    "the mesh has no render queue entry for the tag '{}'.",
                    tag);

  RefPtr<Material3D> material = mesh->material();
  RefPtr<MaterialPass3D> pass = material ? material->GetPass(tag) : nullptr;
  if (!pass)
    throw Exception(Exception::kRGSSError,
                    "the mesh material has no pass tagged '{}'.", tag);

  if (item->slot.chunk == UniformBlockPool::kInvalidChunk)
    throw Exception(Exception::kGPUError,
                    "the mesh has no mesh uniform slot.");

  const UniformBlockPool::Chunk& chunk =
      mesh_pool_->chunk(item->slot.chunk);

  const wgpu::Buffer joints = mesh->AcquireJointBuffer(frame_token_);

  if (pass->depth_only() && !shadow_view_group_)
    UpdateView(true, 0);
  else if (!pass->depth_only() && !view_group_)
    UpdateView(false, 0);

  pass_.SetPipeline(pass->AcquirePipeline());
  if (pass->depth_only())
    pass_.SetBindGroup(0, shadow_view_group_, 1, &shadow_view_offset_);
  else
    pass_.SetBindGroup(0, view_group_, 0, nullptr);
  pass_.SetBindGroup(1,
                     mesh->AcquireGroup(item->slot.chunk, chunk.buffer, joints),
                     1, &item->slot.offset);
  if (pass->HasGroup(2))
    pass_.SetBindGroup(2, pass->AcquireBindGroup(), 0, nullptr);
  if (pass->HasGroup(3)) {
    UpdateGlobals();
    pass_.SetBindGroup(3, globals_group_, 0, nullptr);
  }
  pass_.SetVertexBuffer(0, mesh->vertex_buffer(), 0, WGPU_WHOLE_SIZE);
  pass_.SetIndexBuffer(mesh->index_buffer(), wgpu::IndexFormat::Uint32, 0,
                       WGPU_WHOLE_SIZE);
  pass_.DrawIndexed(static_cast<uint32_t>(mesh->index_count()), 1, 0, 0, 0);
}

int32_t RenderContext3D::DrawTag(std::string tag) {
  if (!queue_)
    return 0;

  const std::vector<RenderQueue3D::Item>* items = queue_->Items(tag);
  if (!items)
    return 0;

  for (const RenderQueue3D::Item& item : *items)
    Draw(item.mesh, tag);

  return static_cast<int32_t>(items->size());
}

}

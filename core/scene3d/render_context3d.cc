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

#include "glm/gtc/matrix_inverse.hpp"

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

constexpr int32_t kMaxViewSlots = 16;

uint32_t ViewStride() {
  return static_cast<uint32_t>(((sizeof(ViewData) + 255u) / 256u) * 256u);
}

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

RenderContext3D::RenderContext3D(URGE_EXCEPT,
                                 wgpu::CommandEncoder encoder,
                                 RefPtr<RenderTarget3D> target)
    : encoder_(encoder),
      target_(std::move(target)),
      ambient_(MakeRefCounted<Color>(26.0f, 28.0f, 36.0f, 255.0f)) {
  mesh_pool_ = std::make_unique<UniformBlockPool>(exception_state, 
      AcquireMeshUniformLayout(), static_cast<uint32_t>(sizeof(MeshData)),
      "mesh3d");
  view_stride_ = ViewStride();
  mesh_pool_->BeginFrame();
}

RenderContext3D::~RenderContext3D() = default;

void RenderContext3D::BeginFrame(wgpu::CommandEncoder encoder,
                                 RefPtr<RenderTarget3D> target) {
  encoder_ = encoder;
  target_ = std::move(target);
  pass_ = nullptr;
  pass_serial_ = 0;
  view_uploaded_.fill(false);
  mesh_pool_->BeginFrame();
  view_override_enabled_ = false;
}

void RenderContext3D::Collect(Node3D* root, Camera3D* camera) {
  camera_ = camera;
  if (!queue_)
    queue_ = MakeRefCounted<RenderQueue3D>();

  frame_token_ = NextFrameToken();
  pass_size_ = target_ ? target_->size() : glm::ivec2(1);
  queue_->Collect(root, camera_.get(), pass_size_, *mesh_pool_);
}

void RenderContext3D::Flush() {
  mesh_pool_->Flush();
}

void RenderContext3D::SetAmbient(RefPtr<Color> value) {
  ambient_ = value ? value : MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 255.0f);
}

void RenderContext3D::SetViewProjection(RefPtr<Camera3D> camera) {
  camera_ = std::move(camera);
  view_override_enabled_ = false;
}

void RenderContext3D::SetViewOverride(const glm::mat4& view_proj) {
  view_override_ = view_proj;
  view_override_enabled_ = true;
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
    if (shadow_target_.get() != target.get()) {
      shadow_target_ = target;
      view_group_ = nullptr;
      view_uploaded_.fill(false);
    }
    shadow_size_ = static_cast<float>(std::max(1, target->Width()));
  }
  shadow_matrices_[cascade] = view_proj;
  shadow_matrix_ = shadow_matrices_[0];
  shadow_cascade_count_ = std::max(shadow_cascade_count_, cascade + 1);
}

void RenderContext3D::SetShadowSplits(std::vector<float> splits) {
  shadow_splits_ = glm::vec4(0.0f);
  for (size_t i = 0; i < splits.size() && i < kMaxCascades; ++i)
    shadow_splits_[static_cast<int>(i)] = splits[i];
}

void RenderContext3D::SetShadowFade(float ratio) {
  shadow_fade_ = std::clamp(ratio, 0.0f, 0.5f);
}

void RenderContext3D::SetShadowCascades(int32_t count) {
  shadow_cascade_count_ =
      std::clamp(count, 1, static_cast<int32_t>(kMaxCascades));
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
    point_faces_[i] =
        i < face_matrices.size() ? face_matrices[i] : glm::mat4(1.0f);
}

void RenderContext3D::UploadView(URGE_EXCEPT, int32_t slot) {
  if (slot < 0 || slot >= kMaxViewSlots)
    return;
  if (view_uploaded_[slot])
    return;

  const wgpu::Device device = GPUDevice::Get().device();

  const uint64_t needed = static_cast<uint64_t>(view_stride_) * kMaxViewSlots;
  if (view_buffer_ == nullptr || view_buffer_.GetSize() < needed) {
    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    desc.size = needed;
    view_buffer_ = device.CreateBuffer(&desc);
    if (view_buffer_ == nullptr) {
      exception_state.Throw(
          ExceptionState::kGPUError,
          "render context 3d: the device rejected the view buffer.");
      return;
    }
    view_group_ = nullptr;
    shadow_view_group_ = nullptr;
  }

  ViewData data = {};
  if (view_override_enabled_)
    data.view_proj = view_override_;
  else if (camera_)
    data.view_proj = camera_->view_projection(pass_size_);
  if (camera_) {
    data.view = camera_->view();
    data.camera_position =
        glm::vec4(glm::vec3(camera_->world_matrix()[3]), 0.0f);
    data.camera_forward =
        glm::vec4(-glm::vec3(camera_->world_matrix()[2]), 0.0f);
  }
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

  data.point_shadow_params = glm::vec4(static_cast<float>(point_light_index_),
                                       point_range_, 0.0f, 0.0f);
  for (uint32_t i = 0; i < 6; ++i)
    data.point_face_view_proj[i] = point_faces_[i];

  GPUDevice::Get().queue().WriteBuffer(
      view_buffer_, static_cast<uint64_t>(slot) * view_stride_, &data,
      sizeof(data));
  view_uploaded_[slot] = true;

  if (view_group_ != nullptr &&
      view_group_shadow_target_ == shadow_target_.get())
    return;

  const wgpu::TextureView shadow_view =
      shadow_target_ ? shadow_target_->depth_view() : AcquireEmptyShadowView();
  const wgpu::TextureView shadow_array =
      shadow_target_ ? shadow_target_->depth_array_view()
                     : AcquireEmptyShadowArrayView();

  wgpu::BindGroupEntry entries[7] = {};
  entries[0].binding = 0;
  entries[0].buffer = view_buffer_;
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
  if (view_group_ == nullptr) {
    exception_state.Throw(ExceptionState::kGPUError, "render context 3d: the device rejected the view group.");
    return;
  }
  view_group_shadow_target_ = shadow_target_.get();
}

void RenderContext3D::UploadShadowView(URGE_EXCEPT, int32_t slot) {
  if (slot < 0 || slot >= kMaxViewSlots)
    return;
  if (view_uploaded_[slot])
    return;

  UploadView(exception_state, slot);

  if (shadow_view_group_ != nullptr)
    return;

  wgpu::BindGroupEntry entry = {};
  entry.binding = 0;
  entry.buffer = view_buffer_;
  entry.offset = 0;
  entry.size = sizeof(ViewData);

  wgpu::BindGroupDescriptor desc;
  desc.layout = AcquireShadowViewLayout();
  desc.entryCount = 1;
  desc.entries = &entry;
  shadow_view_group_ = GPUDevice::Get().device().CreateBindGroup(&desc);
  if (shadow_view_group_ == nullptr) {
    exception_state.Throw(ExceptionState::kGPUError, "render context 3d: the device rejected the shadow view "
                    "group.");
    return;
  }}

void RenderContext3D::UpdateGlobals(URGE_EXCEPT, int32_t slot) {
  if (slot < 0 || slot >= kMaxViewSlots)
    return;

  if (static_cast<size_t>(slot) >= globals_slots_.size())
    globals_slots_.resize(kMaxViewSlots);

  GlobalsSlot& target = globals_slots_[slot];
  if (target.buffer == nullptr) {
    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    desc.size = sizeof(GlobalsData);
    target.buffer = GPUDevice::Get().device().CreateBuffer(&desc);
    if (target.buffer == nullptr) {
      exception_state.Throw(ExceptionState::kGPUError, "render context 3d: the device rejected the globals "
                      "buffer.");
      return;
    }
    wgpu::BindGroupEntry entry = {};
    entry.binding = 0;
    entry.buffer = target.buffer;
    entry.offset = 0;
    entry.size = sizeof(GlobalsData);

    wgpu::BindGroupDescriptor desc_group;
    desc_group.layout = AcquireGlobalsLayout();
    desc_group.entryCount = 1;
    desc_group.entries = &entry;
    target.group = GPUDevice::Get().device().CreateBindGroup(&desc_group);
    if (target.group == nullptr) {
      exception_state.Throw(ExceptionState::kGPUError, "render context 3d: the device rejected the globals "
                      "group.");
      return;
    }  }

  static double last_seconds = 0.0;
  static uint64_t frame_index = 0;

  const double seconds = ClockSeconds();
  const float delta = static_cast<float>(seconds - last_seconds);
  last_seconds = seconds;

  GlobalsData data = {};
  data.time = glm::vec4(static_cast<float>(seconds), delta,
                        static_cast<float>(frame_index++), 0.0f);
  const glm::vec2 size(static_cast<float>(std::max(1, pass_size_.x)),
                       static_cast<float>(std::max(1, pass_size_.y)));
  data.viewport = glm::vec4(size, 1.0f / size.x, 1.0f / size.y);

  GPUDevice::Get().queue().WriteBuffer(target.buffer, 0, &data, sizeof(data));
}

wgpu::BindGroup RenderContext3D::GlobalsGroup(URGE_EXCEPT, int32_t slot) {
  UpdateGlobals(exception_state, slot);
  if (slot < 0 || static_cast<size_t>(slot) >= globals_slots_.size())
    return nullptr;
  return globals_slots_[slot].group;
}

PipelineTargetState3D RenderContext3D::target_state() const {
  PipelineTargetState3D state;
  if (!target_)
    return state;
  state.colors = target_->color_formats();
  state.depth = target_->depth_format();
  state.samples = target_->samples();
  state.cull = cull_override_;
  return state;
}

void RenderContext3D::BeginPass(URGE_EXCEPT, RefPtr<RenderTarget3D> target,
                                RefPtr<Color> clear,
                                bool clear_depth) {
  std::vector<RefPtr<Color>> clears;
  if (clear)
    clears.push_back(clear);
  BeginPassEx(exception_state, std::move(target), std::move(clears),
              clear_depth, clear_depth, 0);
}

void RenderContext3D::BeginPassEx(URGE_EXCEPT, RefPtr<RenderTarget3D> target,
                                  std::vector<RefPtr<Color>> clears,
                                  bool clear_depth,
                                  bool clear_stencil,
                                  int32_t layer) {
  if (!encoder_) {
    exception_state.Throw(ExceptionState::kRGSSError, "render context has no encoder.");
    return;
  }
  if (pass_) {
    pass_.End();
    pass_ = nullptr;
  }

  std::vector<std::optional<glm::vec4>> colors;
  colors.reserve(clears.size());
  for (const RefPtr<Color>& clear : clears)
    colors.push_back(clear ? std::optional<glm::vec4>(clear->Normalize())
                           : std::nullopt);

  target_ = target;
  pass_size_ = target->size();
  pass_layer_ = layer;
  pass_ = target->BeginRendering(encoder_, colors, clear_depth, clear_stencil,
                                 layer);

  pass_serial_ = (pass_serial_ + 1) % kMaxViewSlots;

  const bool shadow_pass =
      shadow_target_ && target.get() == shadow_target_.get();
  if (shadow_pass)
    UploadShadowView(exception_state, pass_serial_);
  else
    UploadView(exception_state, pass_serial_);
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

void RenderContext3D::SetViewport(float x, float y, float width, float height) {
  if (pass_)
    pass_.SetViewport(x, y, width, height, 0.0f, 1.0f);
}

void RenderContext3D::SetScissorRect(int32_t x,
                                     int32_t y,
                                     int32_t width,
                                     int32_t height) {
  if (pass_)
    pass_.SetScissorRect(static_cast<uint32_t>(std::max(0, x)),
                         static_cast<uint32_t>(std::max(0, y)),
                         static_cast<uint32_t>(std::max(0, width)),
                         static_cast<uint32_t>(std::max(0, height)));
}

void RenderContext3D::DrawIndexedIndirect(RefPtr<GPUBuffer> buffer,
                                          int32_t offset) {
  if (!pass_ || !buffer)
    return;
  pass_.DrawIndexedIndirect(buffer->handle(),
                            static_cast<uint64_t>(std::max(0, offset)));
}

void RenderContext3D::Draw(URGE_EXCEPT, const RenderQueue3D::Item& item) {
  if (!pass_) {
    exception_state.Throw(ExceptionState::kRGSSError, "no render pass is open, call begin_pass first.");
    return;
  }
  if (!item.mesh || !item.pass)
    return;
  if (item.slot.chunk == UniformBlockPool::kInvalidChunk) {
    exception_state.Throw(ExceptionState::kGPUError, "the mesh has no mesh uniform slot.");
    return;
  }
  const bool shadow_mode = shadow_pass_mode_ || item.pass->depth_only();

  PipelineTargetState3D state = target_state();
  state.flip_winding = glm::determinant(glm::mat3(item.model)) < 0.0f;
  const wgpu::RenderPipeline pipeline =
      shadow_mode ? item.pass->AcquireDepthOnlyPipeline(state)
                  : item.pass->AcquirePipeline(exception_state, state);

  const wgpu::Buffer joints = item.mesh->AcquireJointBuffer(exception_state, frame_token_);
  const UniformBlockPool::Chunk& chunk = mesh_pool_->chunk(item.slot.chunk);

  uint32_t view_offset = static_cast<uint32_t>(pass_serial_) * view_stride_;

  pass_.SetPipeline(pipeline);

  if (shadow_mode) {
    pass_.SetBindGroup(0, shadow_view_group_, 1, &view_offset);
  } else {
    pass_.SetBindGroup(0, view_group_, 1, &view_offset);
  }

  uint32_t mesh_offset = item.slot.offset;
  pass_.SetBindGroup(
      1, item.mesh->AcquireGroup(exception_state, item.slot.chunk, chunk.buffer, joints), 1,
      &mesh_offset);

  if (!shadow_mode) {
    if (item.pass->HasGroup(2))
      pass_.SetBindGroup(2, item.pass->AcquireBindGroup(exception_state), 0,
                         nullptr);
    if (item.pass->HasGroup(3)) {
      const wgpu::BindGroup globals =
          GlobalsGroup(exception_state, pass_serial_);
      if (globals)
        pass_.SetBindGroup(3, globals, 0, nullptr);
    }
  }

  pass_.SetVertexBuffer(0, item.mesh->vertex_buffer(), 0, WGPU_WHOLE_SIZE);
  if (item.pass->instanced() && item.renderer &&
      item.renderer->instance_buffer()) {
    pass_.SetVertexBuffer(1, item.renderer->instance_buffer(), 0,
                          WGPU_WHOLE_SIZE);
  }
  pass_.SetIndexBuffer(item.mesh->index_buffer(), wgpu::IndexFormat::Uint32, 0,
                       WGPU_WHOLE_SIZE);

  if (item.pass->stencil())
    pass_.SetStencilReference(item.pass->stencil_ref());

  uint32_t instances = 1;
  if (item.pass->instanced() && item.renderer)
    instances =
        static_cast<uint32_t>(std::max(1, item.renderer->InstanceCount()));

  pass_.DrawIndexed(item.index_count, instances, item.index_offset, 0, 0);
}

int32_t RenderContext3D::DrawFilter(URGE_EXCEPT,
                                    std::vector<std::string> tags,
                                    std::vector<int32_t> tag_ids,
                                    int32_t queue_min,
                                    int32_t queue_max,
                                    uint32_t layers) {
  if (!pass_ || !queue_)
    return 0;

  const uint32_t mask = layers == 0 ? 0xFFFFFFFFu : layers;
  std::vector<const RenderQueue3D::Item*> items =
      queue_->Filter(tags, tag_ids, queue_min, queue_max, mask);

  if (shadow_pass_mode_) {
    items.erase(std::remove_if(items.begin(), items.end(),
                               [](const RenderQueue3D::Item* item) {
                                 return !item->renderer ||
                                        !item->renderer->cast_shadow();
                               }),
                items.end());
  }

  if (sort_back_to_front_)
    RenderQueue3D::SortByDistance(items, true);

  for (const RenderQueue3D::Item* item : items)
    Draw(exception_state, *item);

  return static_cast<int32_t>(items.size());
}

void RenderContext3D::DrawFullscreen(URGE_EXCEPT,
                                     RefPtr<MaterialPass3D> pass) {
  if (!pass_ || !pass)
    return;

  const PipelineTargetState3D state = target_state();
  pass_.SetPipeline(pass->AcquirePipeline(exception_state, state));

  uint32_t view_offset = static_cast<uint32_t>(pass_serial_) * view_stride_;
  pass_.SetBindGroup(0, view_group_, 1, &view_offset);

  if (pass->HasGroup(2))
    pass_.SetBindGroup(2, pass->AcquireBindGroup(exception_state), 0, nullptr);
  if (pass->HasGroup(3)) {
    const wgpu::BindGroup globals =
        GlobalsGroup(exception_state, pass_serial_);
    if (globals)
      pass_.SetBindGroup(3, globals, 0, nullptr);
  }

  if (pass->stencil())
    pass_.SetStencilReference(pass->stencil_ref());

  pass_.Draw(3, 1, 0, 0);
}

bool RenderContext3D::DrawItem(URGE_EXCEPT, int32_t index) {
  if (!queue_)
    return false;
  const std::vector<RenderQueue3D::Item>& items = queue_->items();
  if (index < 0 || index >= static_cast<int32_t>(items.size()))
    return false;
  Draw(exception_state, items[index]);
  return true;
}

}

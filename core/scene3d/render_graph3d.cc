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

#include "core/scene3d/render_graph3d.h"

#include <algorithm>
#include <set>
#include <utility>

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/graphics/gpu.h"
#include "core/render/device.h"
#include "core/render/gpu_utils.h"
#include "core/render/texture_format.h"
#include "glm/gtc/matrix_transform.hpp"

#include "core/scene3d/render_context3d.h"
#include "core/scene3d/render_queue3d.h"

namespace urge {

namespace {

bool IsFinalTarget(const std::string& name) {
  return name.empty() || name == "screen" || name == "output";
}

glm::vec3 LightDirection(const Light3D* light) {
  return -glm::normalize(glm::vec3(light->world_matrix()[2]));
}

}

ComputePass3D::ComputePass3D(std::string wgsl, std::string entry)
    : wgsl_(std::move(wgsl)), entry_(std::move(entry)) {
  if (wgsl_.empty())
    throw Exception(Exception::kRGSSError,
                    "compute pass 3d: a compute pass needs a source.");
  if (entry_.empty())
    entry_ = "cs_main";

  const std::vector<wgsl::BindingInfo> infos = wgsl::ReflectBindings(wgsl_, "");
  for (const wgsl::BindingInfo& info : infos) {
    if (info.group != 0)
      throw Exception(Exception::kRGSSError,
                      "compute pass 3d: '{}' is in group({}); a compute pass "
                      "may only use group 0.",
                      info.name, info.group);

    Slot slot;
    slot.name = info.name;
    slot.binding = info.binding;
    slot.kind = info.kind;
    slot.size = info.size;
    slots_.push_back(std::move(slot));
  }

  std::sort(slots_.begin(), slots_.end(),
            [](const Slot& left, const Slot& right) {
              return left.binding < right.binding;
            });
}

ComputePass3D::~ComputePass3D() = default;

void ComputePass3D::SetUniform(std::string name, std::vector<float> values) {
  uniform_map_[std::move(name)] = std::move(values);
  binding_dirty_ = true;
}

void ComputePass3D::SetBuffer(std::string name, std::vector<float> values) {
  buffer_data_[std::move(name)] = std::move(values);
  storage_buffers_.clear();
  binding_dirty_ = true;
}

void ComputePass3D::SetTexture(std::string name, RefPtr<Bitmap> texture) {
  texture_map_[std::move(name)] = std::move(texture);
  binding_dirty_ = true;
}

void ComputePass3D::SetTarget(std::string name,
                              RefPtr<RenderTarget3D> target,
                              int32_t index) {
  target_map_[std::move(name)] = {std::move(target), std::max(0, index)};
  binding_dirty_ = true;
}

void ComputePass3D::EnsurePipeline() {
  if (pipeline_)
    return;

  wgpu::ShaderSourceWGSL source;
  source.code = wgpu::StringView(wgsl_);

  wgpu::ShaderModuleDescriptor module_desc;
  module_desc.nextInChain = &source;
  const wgpu::ShaderModule module =
      GPUDevice::Get().device().CreateShaderModule(&module_desc);
  if (!module)
    throw Exception(Exception::kGPUError,
                    "compute pass 3d: the device rejected the WGSL.");

  wgpu::ComputePipelineDescriptor desc;
  desc.layout = nullptr;
  desc.compute.module = module;
  desc.compute.entryPoint = wgpu::StringView(entry_);
  pipeline_ = GPUDevice::Get().device().CreateComputePipeline(&desc);
  if (!pipeline_)
    throw Exception(Exception::kGPUError,
                    "compute pass 3d: the device rejected the pipeline.");
}

void ComputePass3D::RebuildBindGroup() {
  EnsurePipeline();

  std::vector<wgpu::BindGroupEntry> entries;
  entries.reserve(slots_.size());

  for (const Slot& slot : slots_) {
    wgpu::BindGroupEntry entry = {};
    entry.binding = slot.binding;

    switch (slot.kind) {
      case wgsl::BindingKind::kUniform: {
        const uint64_t size = std::max<uint64_t>(slot.size, 16);
        wgpu::Buffer& buffer = uniform_buffers_[slot.name];
        if (buffer == nullptr || buffer.GetSize() < size) {
          wgpu::BufferDescriptor desc;
          desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
          desc.size = size;
          buffer = GPUDevice::Get().device().CreateBuffer(&desc);
          if (buffer == nullptr)
            throw Exception(Exception::kGPUError,
                            "compute pass 3d: the device rejected a uniform "
                            "buffer.");
        }

        std::vector<float> data(static_cast<size_t>(size / 4), 0.0f);
        const auto found = uniform_map_.find(slot.name);
        if (found != uniform_map_.end()) {
          const size_t count = std::min(found->second.size(), data.size());
          for (size_t i = 0; i < count; ++i)
            data[i] = found->second[i];
        }
        GPUDevice::Get().queue().WriteBuffer(buffer, 0, data.data(), size);

        entry.buffer = buffer;
        entry.offset = 0;
        entry.size = size;
        break;
      }
      case wgsl::BindingKind::kStorage:
      case wgsl::BindingKind::kStorageReadWrite: {
        wgpu::Buffer& buffer = storage_buffers_[slot.name];
        if (buffer == nullptr) {
          const auto found = buffer_data_.find(slot.name);
          const size_t count =
              found == buffer_data_.end() ? 16 : found->second.size();
          const uint64_t bytes = std::max<size_t>(count, 16) * sizeof(float);

          wgpu::BufferDescriptor desc;
          desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
          desc.size = bytes;
          buffer = GPUDevice::Get().device().CreateBuffer(&desc);
          if (buffer == nullptr)
            throw Exception(Exception::kGPUError,
                            "compute pass 3d: the device rejected a storage "
                            "buffer.");

          if (found != buffer_data_.end())
            GPUDevice::Get().queue().WriteBuffer(buffer, 0, found->second.data(),
                                                 found->second.size() *
                                                     sizeof(float));
        }

        entry.buffer = buffer;
        entry.offset = 0;
        entry.size = WGPU_WHOLE_SIZE;
        break;
      }
      case wgsl::BindingKind::kSampler:
      case wgsl::BindingKind::kSamplerComparison: {
        wgpu::SamplerDescriptor sampler_desc;
        sampler_desc.magFilter = wgpu::FilterMode::Linear;
        sampler_desc.minFilter = wgpu::FilterMode::Linear;
        entry.sampler = GPUDevice::Get().device().CreateSampler(&sampler_desc);
        break;
      }
      case wgsl::BindingKind::kStorageTexture: {
        const auto found = target_map_.find(slot.name);
        entry.textureView =
            found == target_map_.end()
                ? nullptr
                : found->second.first->color_view(found->second.second);
        break;
      }
      default: {
        const auto target = target_map_.find(slot.name);
        if (target != target_map_.end()) {
          entry.textureView =
              target->second.first->sampled_view(target->second.second);
          break;
        }
        const auto found = texture_map_.find(slot.name);
        entry.textureView = found == texture_map_.end() || !found->second
                                ? nullptr
                                : found->second->texture_view();
        break;
      }
    }

    entries.push_back(entry);
  }

  wgpu::BindGroupDescriptor desc;
  desc.layout = pipeline_.GetBindGroupLayout(0);
  desc.entryCount = entries.size();
  desc.entries = entries.empty() ? nullptr : entries.data();
  binding_ = GPUDevice::Get().device().CreateBindGroup(&desc);
  if (binding_ == nullptr)
    throw Exception(Exception::kGPUError,
                    "compute pass 3d: the device rejected the bind group.");
  binding_dirty_ = false;
}

void ComputePass3D::Execute(RenderContext3D* context) {
  if (!context || !context->encoder())
    return;

  EnsurePipeline();
  if (binding_dirty_ || binding_ == nullptr)
    RebuildBindGroup();

  wgpu::ComputePassDescriptor desc;
  const wgpu::ComputePassEncoder encoder =
      context->encoder().BeginComputePass(&desc);
  encoder.SetPipeline(pipeline_);
  encoder.SetBindGroup(0, binding_, 0, nullptr);
  encoder.DispatchWorkgroups(static_cast<uint32_t>(std::max(0, dispatch_x_)),
                             static_cast<uint32_t>(std::max(0, dispatch_y_)),
                             static_cast<uint32_t>(std::max(0, dispatch_z_)));
  encoder.End();
}

RenderGraphPass3D::RenderGraphPass3D(std::string name)
    : name_(std::move(name)) {}

RenderGraphPass3D::~RenderGraphPass3D() = default;

void RenderGraphPass3D::BindTexture(std::string slot, std::string texture) {
  if (slot.empty() || texture.empty())
    return;
  bindings_[std::move(slot)] = std::move(texture);
}

std::vector<std::string> RenderGraphPass3D::GetBindings() const {
  std::vector<std::string> result;
  result.reserve(bindings_.size());
  for (const auto& entry : bindings_)
    result.push_back(entry.first + "=" + entry.second);
  return result;
}

RefPtr<GPUTextureView> RenderGraphPass3D::GetTexture(std::string name) {
  if (!graph_)
    return nullptr;
  RefPtr<RenderTarget3D> target = graph_->Target(name);
  return target ? MakeRefCounted<GPUTextureView>(target->sampled_view(0))
                : nullptr;
}

RefPtr<GPURenderPassEncoder> RenderGraphPass3D::GetPass() {
  return context_ ? context_->GetPass() : nullptr;
}

RefPtr<RenderContext3D> RenderGraphPass3D::GetContext() {
  return context_;
}

int32_t RenderGraphPass3D::Width() {
  if (!context_ || !context_->GetTarget())
    return 0;
  return context_->GetTarget()->size().x;
}

int32_t RenderGraphPass3D::Height() {
  if (!context_ || !context_->GetTarget())
    return 0;
  return context_->GetTarget()->size().y;
}

RenderGraph3D::RenderGraph3D() = default;

RenderGraph3D::~RenderGraph3D() = default;

RefPtr<RenderGraphPass3D> RenderGraph3D::AddPass(std::string name) {
  if (name.empty())
    throw Exception(Exception::kRGSSError,
                    "render graph 3d: a pass needs a name.");

  for (const RefPtr<RenderGraphPass3D>& pass : passes_) {
    if (pass->GetName() == name)
      throw Exception(Exception::kRGSSError,
                      "render graph 3d: the pass '{}' already exists.", name);
  }

  auto pass = MakeRefCounted<RenderGraphPass3D>(std::move(name));
  pass->SetOwner(this);
  passes_.push_back(pass);
  return pass;
}

RefPtr<RenderGraphPass3D> RenderGraph3D::GetPass(std::string name) {
  for (const RefPtr<RenderGraphPass3D>& pass : passes_)
    if (pass->GetName() == name)
      return pass;
  return nullptr;
}

void RenderGraph3D::RemovePass(std::string name) {
  passes_.erase(
      std::remove_if(passes_.begin(), passes_.end(),
                     [&name](const RefPtr<RenderGraphPass3D>& pass) {
                       return pass->GetName() == name;
                     }),
      passes_.end());
}

void RenderGraph3D::Clear() {
  passes_.clear();
  targets_.clear();
  textures_.clear();
}

RefPtr<RenderGraphPass3D> RenderGraph3D::PassAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(passes_.size()))
    return nullptr;
  return passes_[index];
}

void RenderGraph3D::AddTexture(std::string name,
                               std::string format,
                               int32_t samples) {
  const wgpu::TextureFormat parsed = ParseTextureFormat(format);
  if (parsed == wgpu::TextureFormat::Undefined)
    throw Exception(Exception::kRGSSError,
                    "render graph 3d: '{}' is not a texture format.", format);

  TextureDesc desc;
  desc.format = parsed;
  desc.samples = std::max(1, samples);
  textures_[std::move(name)] = desc;
  targets_.clear();
}

void RenderGraph3D::AddStorageTexture(std::string name, std::string format) {
  AddTexture(name, std::move(format), 1);
  const auto it = textures_.find(name);
  if (it != textures_.end())
    it->second.storage = true;
  targets_.clear();
}

void RenderGraph3D::SetTextureScale(std::string name, float scale) {
  const auto it = textures_.find(name);
  if (it == textures_.end())
    throw Exception(Exception::kRGSSError,
                    "render graph 3d: the texture '{}' does not exist.", name);
  it->second.scale = std::clamp(scale, 0.05f, 4.0f);
  targets_.clear();
}

RefPtr<RenderTarget3D> RenderGraph3D::GetTexture(std::string name) {
  return Target(name);
}

std::vector<std::string> RenderGraph3D::TextureNames() const {
  std::vector<std::string> names;
  names.reserve(textures_.size());
  for (const auto& entry : textures_)
    names.push_back(entry.first);
  return names;
}

RefPtr<RenderTarget3D> RenderGraph3D::ShadowTarget(const std::string& name,
                                                   int32_t layers) {
  if (IsFinalTarget(name))
    return nullptr;

  const int32_t clamped = std::clamp(layers, 1, 6);
  auto it = shadow_targets_.find(name);
  if (it != shadow_targets_.end() &&
      it->second->LayerCount() == clamped &&
      it->second->Width() == shadow_size_)
    return it->second;

  auto target = MakeRefCounted<RenderTarget3D>(shadow_size_, shadow_size_,
                                               false, clamped);
  shadow_targets_[name] = target;
  return target;
}

RefPtr<RenderTarget3D> RenderGraph3D::Target(const std::string& name) {
  if (IsFinalTarget(name))
    return final_target_;

  const auto shadow = shadow_targets_.find(name);
  if (shadow != shadow_targets_.end())
    return shadow->second;

  const auto desc = textures_.find(name);
  if (desc == textures_.end())
    return nullptr;

  if (!final_target_)
    return nullptr;

  const glm::ivec2 base = final_target_->size();
  const int32_t width =
      std::max(1, static_cast<int32_t>(static_cast<float>(base.x) *
                                       desc->second.scale));
  const int32_t height =
      std::max(1, static_cast<int32_t>(static_cast<float>(base.y) *
                                       desc->second.scale));

  auto it = targets_.find(name);
  if (it != targets_.end() && it->second->Width() == width &&
      it->second->Height() == height)
    return it->second;

  RenderTargetDesc3D target_desc;
  target_desc.width = width;
  target_desc.height = height;
  target_desc.samples = desc->second.samples;
  target_desc.storage = desc->second.storage;

  if (IsDepthFormat(desc->second.format)) {
    target_desc.depth = desc->second.format;
  } else {
    target_desc.colors.push_back(desc->second.format);
    target_desc.depth = wgpu::TextureFormat::Depth32Float;
  }

  auto target = MakeRefCounted<RenderTarget3D>(target_desc);
  targets_[name] = target;
  return target;
}

RefPtr<RenderTarget3D> RenderGraph3D::TargetForPass(RenderGraphPass3D* pass) {
  const std::vector<std::string>& colors = pass->colors();
  if (colors.size() <= 1)
    return Target(colors.empty() ? std::string() : colors[0]);

  if (!final_target_)
    return nullptr;

  std::string key = "mrt:";
  for (const std::string& color : colors)
    key += color + "|";

  const glm::ivec2 base = final_target_->size();
  auto cached = mrt_targets_.find(key);
  if (cached != mrt_targets_.end() &&
      cached->second->Width() == base.x &&
      cached->second->Height() == base.y)
    return cached->second;

  RenderTargetDesc3D desc;
  desc.width = base.x;
  desc.height = base.y;
  desc.depth = wgpu::TextureFormat::Depth32Float;

  std::vector<std::string> names;
  for (const std::string& color : colors) {
    const auto texture = textures_.find(color);
    if (texture == textures_.end())
      throw Exception(Exception::kRGSSError,
                      "render graph 3d: the pass '{}' writes '{}', which is "
                      "not a declared texture.",
                      pass->name(), color);
    if (IsDepthFormat(texture->second.format))
      continue;
    desc.colors.push_back(texture->second.format);
    desc.samples = texture->second.samples;
    if (texture->second.storage)
      desc.storage = true;
    names.push_back(color);
  }

  if (desc.colors.empty())
    return nullptr;

  auto target = MakeRefCounted<RenderTarget3D>(desc);
  mrt_targets_[key] = target;
  for (size_t i = 0; i < names.size(); ++i)
    attachment_views_[names[i]] = {target, static_cast<int32_t>(i)};
  return target;
}

wgpu::TextureView RenderGraph3D::ResolveColorView(const std::string& name) {
  const auto found = attachment_views_.find(name);
  if (found != attachment_views_.end() && found->second.first)
    return found->second.first->sampled_view(found->second.second);

  RefPtr<RenderTarget3D> target = Target(name);
  return target ? target->sampled_view(0) : nullptr;
}

std::vector<size_t> RenderGraph3D::BuildOrder() const {
  std::vector<size_t> order;
  order.reserve(passes_.size());

  std::vector<bool> emitted(passes_.size(), false);
  std::set<std::string> written;

  for (size_t step = 0; step < passes_.size(); ++step) {
    bool progressed = false;
    for (size_t index = 0; index < passes_.size(); ++index) {
      if (emitted[index])
        continue;

      const RenderGraphPass3D* pass = passes_[index].get();
      bool ready = true;
      for (const std::string& read : pass->reads()) {
        if (IsFinalTarget(read))
          continue;
        if (written.find(read) == written.end()) {
          ready = false;
          break;
        }
      }

      if (!ready)
        continue;

      order.push_back(index);
      emitted[index] = true;
      for (const std::string& color : pass->colors())
        if (!IsFinalTarget(color))
          written.insert(color);
      progressed = true;
    }

    if (!progressed)
      break;
  }

  for (size_t index = 0; index < passes_.size(); ++index)
    if (!emitted[index])
      order.push_back(index);

  return order;
}

void RenderGraph3D::ExecuteShadowPass(RenderContext3D* context,
                                      RenderGraphPass3D* pass) {
  const std::string target_name =
      pass->colors().empty() ? std::string("shadow") : pass->colors()[0];

  if (pass->point_shadow_light()) {
    RefPtr<Light3D> light = pass->point_shadow_light();
    const glm::vec3 eye(light->world_matrix()[3]);
    const float range = std::max(light->range(), 0.1f);

    int32_t light_index = -1;
    if (context->GetQueue()) {
      const std::vector<RenderQueue3D::LightItem>& lights =
          context->GetQueue()->lights();
      for (size_t i = 0; i < lights.size(); ++i) {
        if (lights[i].type == 1 &&
            glm::length(lights[i].position - eye) < 0.001f) {
          light_index = static_cast<int32_t>(i);
          break;
        }
      }
    }

    RefPtr<RenderTarget3D> target = ShadowTarget(target_name, 6);
    if (!target)
      return;

    const float near_plane = 0.05f;
    const float far_plane = std::max(range, near_plane + 0.1f);
    const float depth = near_plane - far_plane;

    glm::mat4 projection(0.0f);
    projection[0][0] = 1.0f;
    projection[1][1] = 1.0f;
    projection[2][2] = far_plane / depth;
    projection[2][3] = -1.0f;
    projection[3][2] = near_plane * far_plane / depth;

    const glm::vec3 directions[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                     {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
    const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1},
                              {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

    std::vector<glm::mat4> faces(6, glm::mat4(1.0f));
    for (int32_t i = 0; i < 6; ++i)
      faces[i] = projection * glm::lookAt(eye, eye + directions[i], ups[i]);

    context->SetPointShadow(target, faces, light_index, range);

    for (int32_t face = 0; face < 6; ++face) {
      context->SetViewOverride(faces[face]);
      context->BeginPassEx(target, {}, (pass->clear_flags() & 2u) != 0,
                           (pass->clear_flags() & 4u) != 0, face);
      context->SetShadowPassMode(true);
      context->DrawFilter(pass->tags(), pass->tag_ids(), pass->queue_min(),
                          pass->queue_max(), pass->layers());
      context->EndPass();
    }
    return;
  }

  const int32_t cascades = std::clamp(pass->cascades(), 1, 4);
  RefPtr<RenderTarget3D> target = ShadowTarget(target_name, cascades);
  if (!target)
    return;

  RefPtr<Camera3D> camera = pass->camera() ? pass->camera()
                                           : (camera_ ? camera_ : context->GetCamera());

  glm::vec3 light_direction(0.0f, -1.0f, 0.0f);
  bool has_directional_light = false;

  if (pass->shadow_light()) {
    light_direction = LightDirection(pass->shadow_light().get());
    has_directional_light = true;
  } else if (context->GetQueue()) {
    for (const RenderQueue3D::LightItem& item : context->GetQueue()->lights()) {
      if (item.type == 0 && item.cast_shadow) {
        light_direction = item.direction;
        has_directional_light = true;
        break;
      }
    }
  }

  if (!camera)
    return;

  if (has_directional_light) {
    const std::vector<glm::mat4> matrices = camera->ComputeCascades(
        light_direction, cascades, pass->split_lambda(),
        final_target_ ? final_target_->size() : glm::ivec2(1),
        pass->shadow_distance(), target->Width());

    context->SetShadowCascades(cascades);
    context->SetShadowFade(pass->cascade_fade());
    context->SetShadowSplits(
        camera->ComputeSplits(cascades, pass->split_lambda(),
                              pass->shadow_distance()));

    for (int32_t cascade = 0; cascade < cascades; ++cascade) {
      context->SetShadow(target, matrices[cascade], cascade);
      context->SetViewOverride(matrices[cascade]);
      context->BeginPassEx(target, {}, (pass->clear_flags() & 2u) != 0,
                           (pass->clear_flags() & 4u) != 0, cascade);
      context->SetShadowPassMode(true);
      context->DrawFilter(pass->tags(), pass->tag_ids(), pass->queue_min(),
                          pass->queue_max(), pass->layers());
      context->EndPass();
    }
    return;
  }

  context->SetShadow(target, camera->view_projection(target->size()));
  context->BeginPassEx(target, {}, (pass->clear_flags() & 2u) != 0,
                       (pass->clear_flags() & 4u) != 0, 0);
  context->SetShadowPassMode(true);
  context->DrawFilter(pass->tags(), pass->tag_ids(), pass->queue_min(),
                      pass->queue_max(), pass->layers());
  context->EndPass();
}

void RenderGraph3D::ExecuteRasterPass(RenderContext3D* context,
                                      RenderGraphPass3D* pass) {
  RefPtr<Camera3D> camera = pass->camera() ? pass->camera()
                                           : (camera_ ? camera_ : context->GetCamera());
  if (camera)
    context->SetViewProjection(camera);

  RefPtr<RenderTarget3D> target = TargetForPass(pass);
  if (!target)
    return;

  const bool clear_color = (pass->clear_flags() & 1u) != 0;
  std::vector<RefPtr<Color>> clears(
      static_cast<size_t>(target->ColorCount()),
      clear_color ? pass->clear_color() : RefPtr<Color>());

  context->SetShadowPassMode(false);
  context->SetCullOverride(pass->cull_override());
  context->SetSortBackToFront(pass->sort_back_to_front());
  context->BeginPassEx(target, clears, (pass->clear_flags() & 2u) != 0,
                       (pass->clear_flags() & 4u) != 0, 0);

  const uint32_t layers =
      pass->layers() != 0
          ? pass->layers()
          : (camera ? camera->culling_mask() : 0xFFFFFFFFu);

  context->DrawFilter(pass->tags(), pass->tag_ids(), pass->queue_min(),
                      pass->queue_max(), layers);
  context->EndPass();
}

void RenderGraph3D::ExecuteFullscreenPass(RenderContext3D* context,
                                          RenderGraphPass3D* pass) {
  RefPtr<MaterialPass3D> material = pass->material();
  if (!material)
    return;

  const std::string target_name =
      pass->colors().empty() ? std::string() : pass->colors()[0];
  RefPtr<RenderTarget3D> target = Target(target_name);
  if (!target)
    return;

  material->set_fullscreen(true);

  for (const auto& binding : pass->bindings()) {
    const wgpu::TextureView view = ResolveColorView(binding.second);
    if (!view)
      continue;
    material->SetTextureView(binding.first,
                             MakeRefCounted<GPUTextureView>(view));
  }

  const bool clear_color = (pass->clear_flags() & 1u) != 0;
  std::vector<RefPtr<Color>> clears(
      static_cast<size_t>(target->ColorCount()),
      clear_color ? pass->clear_color() : RefPtr<Color>());

  context->BeginPassEx(target, clears, (pass->clear_flags() & 2u) != 0,
                       (pass->clear_flags() & 4u) != 0, 0);
  context->DrawFullscreen(material);
  context->EndPass();
}

void RenderGraph3D::Execute(RenderContext3D* context) {
  if (!context || !context->GetTarget())
    return;

  final_target_ = context->GetTarget();
  if (camera_)
    context->SetViewProjection(camera_);
  if (ambient_)
    context->SetAmbient(ambient_);

  const std::vector<size_t> order = BuildOrder();

  for (size_t index : order) {
    RenderGraphPass3D* pass = passes_[index].get();
    if (!pass->enabled())
      continue;
    pass->SetContext(context);

    switch (pass->kind()) {
      case 2:
        if (pass->compute())
          pass->compute()->Execute(context);
        break;
      case 1:
        ExecuteFullscreenPass(context, pass);
        break;
      default:
        if (pass->depth_only())
          ExecuteShadowPass(context, pass);
        else
          ExecuteRasterPass(context, pass);
        break;
    }

    pass->SetContext(nullptr);
  }

  final_target_ = nullptr;
}

}

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
#include <cmath>
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
#include "core/scene3d/render_graph_order3d.h"
#include "core/scene3d/render_queue3d.h"

namespace urge {

namespace {

bool IsFinalTarget(const std::string& name) {
  return name.empty() || name == "screen" || name == "output";
}

}

ComputePass3D::ComputePass3D(URGE_EXCEPT, std::string wgsl, std::string entry)
    : wgsl_(std::move(wgsl)), entry_(std::move(entry)) {
  if (wgsl_.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "compute pass 3d: a compute pass needs a source.");
    return;
  }
  if (entry_.empty())
    entry_ = "cs_main";

  const std::vector<wgsl::BindingInfo> infos = wgsl::ReflectBindings(wgsl_, "");
  for (const wgsl::BindingInfo& info : infos) {
    if (info.group != 0) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "compute pass 3d: '{}' is in group({}); a compute "
                            "pass may only use group 0.",
                            info.name, info.group);
      return;
    }

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
  storage_buffers_.erase(name);
  buffer_data_[std::move(name)] = std::move(values);
  binding_dirty_ = true;
}

void ComputePass3D::SetTexture(std::string name, RefPtr<Bitmap> texture) {
  target_map_.erase(name);
  texture_map_[std::move(name)] = std::move(texture);
  binding_dirty_ = true;
}

void ComputePass3D::SetTarget(std::string name,
                              RefPtr<RenderTarget3D> target,
                              int32_t index) {
  const auto found = target_map_.find(name);
  if (found != target_map_.end() && found->second.first.get() == target.get() &&
      found->second.second == index)
    return;
  texture_map_.erase(name);
  target_map_[std::move(name)] = {std::move(target), index};
  binding_dirty_ = true;
}

void ComputePass3D::EnsurePipeline(URGE_EXCEPT) {
  if (pipeline_)
    return;

  wgpu::ShaderSourceWGSL source;
  source.code = wgpu::StringView(wgsl_);

  wgpu::ShaderModuleDescriptor module_desc;
  module_desc.nextInChain = &source;
  const wgpu::ShaderModule module =
      GPUDevice::Get().device().CreateShaderModule(&module_desc);
  if (!module) {
    exception_state.Throw(ExceptionState::kGPUError,
                          "compute pass 3d: the device rejected the WGSL.");
    return;
  }
  wgpu::ComputePipelineDescriptor desc;
  desc.layout = nullptr;
  desc.compute.module = module;
  desc.compute.entryPoint = wgpu::StringView(entry_);
  pipeline_ = GPUDevice::Get().device().CreateComputePipeline(&desc);
  if (!pipeline_) {
    exception_state.Throw(ExceptionState::kGPUError,
                          "compute pass 3d: the device rejected the pipeline.");
    return;
  }
}

void ComputePass3D::RebuildBindGroup(URGE_EXCEPT) {
  EnsurePipeline(exception_state);
  if (exception_state.HadException() || !pipeline_)
    return;

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
          if (buffer == nullptr) {
            exception_state.Throw(
                ExceptionState::kGPUError,
                "compute pass 3d: the device rejected a uniform "
                "buffer.");
            return;
          }
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
          const uint64_t bytes = std::max<uint64_t>(
              std::max<size_t>(count, 16) * sizeof(float), slot.size);

          wgpu::BufferDescriptor desc;
          desc.usage = wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
          desc.size = bytes;
          buffer = GPUDevice::Get().device().CreateBuffer(&desc);
          if (buffer == nullptr) {
            exception_state.Throw(
                ExceptionState::kGPUError,
                "compute pass 3d: the device rejected a storage "
                "buffer.");
            return;
          }
          if (found != buffer_data_.end())
            GPUDevice::Get().queue().WriteBuffer(
                buffer, 0, found->second.data(),
                found->second.size() * sizeof(float));
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
        if (slot.kind == wgsl::BindingKind::kSamplerComparison)
          sampler_desc.compare = wgpu::CompareFunction::LessEqual;
        entry.sampler = GPUDevice::Get().device().CreateSampler(&sampler_desc);
        break;
      }
      case wgsl::BindingKind::kStorageTexture: {
        const auto found = target_map_.find(slot.name);
        entry.textureView =
            found == target_map_.end() || !found->second.first
                ? nullptr
                : found->second.first->color_view(found->second.second);
        break;
      }
      default: {
        const auto target = target_map_.find(slot.name);
        if (target != target_map_.end() && target->second.first) {
          entry.textureView =
              slot.kind == wgsl::BindingKind::kTextureDepth
                  ? target->second.first->depth_view()
                  : target->second.first->sampled_view(target->second.second);
          break;
        }
        const auto found = texture_map_.find(slot.name);
        entry.textureView = found == texture_map_.end() || !found->second ||
                                    found->second->IsDisposed()
                                ? nullptr
                                : found->second->texture_view();
        break;
      }
    }

    if (slot.kind != wgsl::BindingKind::kUniform &&
        slot.kind != wgsl::BindingKind::kStorage &&
        slot.kind != wgsl::BindingKind::kStorageReadWrite && !entry.sampler &&
        !entry.textureView) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "compute pass 3d: resource '{}' is not bound.",
                            slot.name);
      return;
    }

    entries.push_back(entry);
  }

  wgpu::BindGroupDescriptor desc;
  desc.layout = pipeline_.GetBindGroupLayout(0);
  desc.entryCount = entries.size();
  desc.entries = entries.empty() ? nullptr : entries.data();
  binding_ = GPUDevice::Get().device().CreateBindGroup(&desc);
  if (binding_ == nullptr) {
    exception_state.Throw(
        ExceptionState::kGPUError,
        "compute pass 3d: the device rejected the bind group.");
    return;
  }
  binding_dirty_ = false;
}

void ComputePass3D::Execute(URGE_EXCEPT, RenderContext3D* context) {
  if (!context || !context->encoder())
    return;

  EnsurePipeline(exception_state);
  if (exception_state.HadException())
    return;
  if (!slots_.empty() && (binding_dirty_ || binding_ == nullptr))
    RebuildBindGroup(exception_state);
  if (exception_state.HadException() || (!slots_.empty() && !binding_))
    return;
  if (context->pass()) {
    exception_state.Throw(
        ExceptionState::kRGSSError,
        "compute pass 3d: end the render pass before dispatch.");
    return;
  }

  wgpu::ComputePassDescriptor desc;
  const wgpu::ComputePassEncoder encoder =
      context->encoder().BeginComputePass(&desc);
  encoder.SetPipeline(pipeline_);
  if (!slots_.empty())
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

RefPtr<GPUTextureView> RenderGraphPass3D::GetTexture(URGE_EXCEPT,
                                                     std::string name) {
  if (!graph_)
    return nullptr;
  const wgpu::TextureView view =
      graph_->ResolveColorView(exception_state, name);
  return view ? MakeRefCounted<GPUTextureView>(view) : nullptr;
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

RenderGraph3D::~RenderGraph3D() {
  Clear();
}

RefPtr<RenderGraphPass3D> RenderGraph3D::AddPass(URGE_EXCEPT,
                                                 std::string name) {
  if (name.empty()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: a pass needs a name.");
    return {};
  }
  for (const RefPtr<RenderGraphPass3D>& pass : passes_) {
    if (pass->GetName() == name) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "render graph 3d: the pass '{}' already exists.",
                            name);
      return {};
    }
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
  passes_.erase(std::remove_if(passes_.begin(), passes_.end(),
                               [&name](const RefPtr<RenderGraphPass3D>& pass) {
                                 if (pass->GetName() != name)
                                   return false;
                                 pass->SetOwner(nullptr);
                                 pass->SetContext(nullptr);
                                 return true;
                               }),
                passes_.end());
}

void RenderGraph3D::Clear() {
  for (const RefPtr<RenderGraphPass3D>& pass : passes_) {
    pass->SetOwner(nullptr);
    pass->SetContext(nullptr);
  }
  passes_.clear();
  targets_.clear();
  shadow_targets_.clear();
  textures_.clear();
  final_target_ = nullptr;
}

RefPtr<RenderGraphPass3D> RenderGraph3D::PassAt(int32_t index) const {
  if (index < 0 || index >= static_cast<int32_t>(passes_.size()))
    return nullptr;
  return passes_[index];
}

void RenderGraph3D::AddTexture(URGE_EXCEPT,
                               std::string name,
                               std::string format,
                               int32_t samples) {
  const wgpu::TextureFormat parsed = ParseTextureFormat(format);
  if (IsFinalTarget(name)) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: texture names must not be empty "
                          "or use the reserved names screen/output.");
    return;
  }
  if (samples != 1 && samples != 4) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: sample count must be 1 or 4.");
    return;
  }
  if (parsed == wgpu::TextureFormat::Undefined) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: '{}' is not a texture format.",
                          format);
    return;
  }
  TextureDesc desc;
  desc.format = parsed;
  desc.samples = std::max(1, samples);
  shadow_targets_.erase(name);
  textures_[std::move(name)] = desc;
  targets_.clear();
}

void RenderGraph3D::AddStorageTexture(URGE_EXCEPT,
                                      std::string name,
                                      std::string format) {
  if (ParseTextureFormat(format) == wgpu::TextureFormat::RGBA8Snorm) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: storage render targets also need "
                          "a renderable format; rgba8snorm is storage-only.");
    return;
  }
  if (!IsStorageCapable(ParseTextureFormat(format))) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: '{}' does not support storage.",
                          format);
    return;
  }
  AddTexture(exception_state, name, std::move(format), 1);
  if (exception_state.HadException())
    return;
  const auto it = textures_.find(name);
  if (it != textures_.end())
    it->second.storage = true;
  targets_.clear();
}

void RenderGraph3D::SetTextureScale(URGE_EXCEPT,
                                    std::string name,
                                    float scale) {
  const auto it = textures_.find(name);
  if (it == textures_.end()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: the texture '{}' does not exist.",
                          name);
    return;
  }
  if (!std::isfinite(scale)) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: texture scale must be finite.");
    return;
  }
  it->second.scale = std::clamp(scale, 0.05f, 4.0f);
  targets_.clear();
}

RefPtr<RenderTarget3D> RenderGraph3D::GetTexture(URGE_EXCEPT,
                                                 std::string name) {
  return Target(exception_state, name);
}

std::vector<std::string> RenderGraph3D::TextureNames() const {
  std::vector<std::string> names;
  names.reserve(textures_.size());
  for (const auto& entry : textures_)
    names.push_back(entry.first);
  return names;
}

RefPtr<RenderTarget3D> RenderGraph3D::ShadowTarget(URGE_EXCEPT,
                                                   const std::string& name,
                                                   int32_t layers) {
  if (IsFinalTarget(name))
    return nullptr;

  const int32_t clamped = std::clamp(layers, 1, 6);
  auto it = shadow_targets_.find(name);
  if (it != shadow_targets_.end() && it->second->LayerCount() == clamped &&
      it->second->Width() == shadow_size_)
    return it->second;

  auto target = MakeRefCounted<RenderTarget3D>(exception_state, shadow_size_,
                                               shadow_size_, false, clamped);
  if (exception_state.HadException())
    return nullptr;
  shadow_targets_[name] = target;
  return target;
}

RefPtr<RenderTarget3D> RenderGraph3D::Target(URGE_EXCEPT,
                                             const std::string& name) {
  if (IsFinalTarget(name))
    return final_target_;

  const auto shadow = shadow_targets_.find(name);
  if (shadow != shadow_targets_.end())
    return shadow->second;

  const auto desc = textures_.find(name);
  if (desc == textures_.end()) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "render graph 3d: texture '{}' is not declared.",
                          name);
    return nullptr;
  }

  if (!final_target_)
    return nullptr;

  const glm::ivec2 base = final_target_->size();
  const int32_t width = std::max(
      1, static_cast<int32_t>(static_cast<float>(base.x) * desc->second.scale));
  const int32_t height = std::max(
      1, static_cast<int32_t>(static_cast<float>(base.y) * desc->second.scale));

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

  auto target = MakeRefCounted<RenderTarget3D>(exception_state, target_desc);
  if (exception_state.HadException())
    return nullptr;
  targets_[name] = target;
  return target;
}

RefPtr<RenderTarget3D> RenderGraph3D::TargetForPass(URGE_EXCEPT,
                                                    RenderGraphPass3D* pass) {
  const std::vector<std::string>& colors = pass->colors();
  if (colors.size() <= 1)
    return Target(exception_state, colors.empty() ? std::string() : colors[0]);

  std::vector<RefPtr<RenderTarget3D>> attachments;
  std::set<std::string> attached;
  RefPtr<RenderTarget3D> depth;
  for (const std::string& color : colors) {
    if (!attached.insert(color).second) {
      exception_state.Throw(
          ExceptionState::kRGSSError,
          "render graph 3d: MRT pass '{}' repeats attachment '{}'.",
          pass->name(), color);
      return {};
    }
    if (IsFinalTarget(color) || textures_.find(color) == textures_.end()) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "render graph 3d: MRT pass '{}' needs declared "
                            "texture names; '{}' is not one.",
                            pass->name(), color);
      return {};
    }
    RefPtr<RenderTarget3D> target = Target(exception_state, color);
    if (exception_state.HadException() || !target)
      return {};
    if (target->HasColor()) {
      attachments.push_back(target);
    } else {
      if (depth) {
        exception_state.Throw(ExceptionState::kRGSSError,
                              "render graph 3d: MRT pass '{}' has multiple "
                              "depth attachments.",
                              pass->name());
        return {};
      }
      depth = target;
    }
  }
  if (!depth && !attachments.empty())
    depth = attachments.front();
  // Compose views of the named resources instead of allocating another set of
  // textures. Subsequent single-output and compute passes see the same pixels.
  return MakeRefCounted<RenderTarget3D>(exception_state, attachments, depth);
}

wgpu::TextureView RenderGraph3D::ResolveColorView(URGE_EXCEPT,
                                                  const std::string& name) {
  RefPtr<RenderTarget3D> target = Target(exception_state, name);
  return target ? (target->HasColor() ? target->sampled_view(0)
                                      : target->depth_view())
                : nullptr;
}

std::vector<size_t> RenderGraph3D::BuildOrder(URGE_EXCEPT) const {
  const auto canonical = [](const std::string& name) {
    return IsFinalTarget(name) ? std::string("screen") : name;
  };
  std::vector<RenderGraphAccess3D> accesses;
  for (const RefPtr<RenderGraphPass3D>& pass : passes_) {
    if (pass->enabled() && pass->kind() > 2) {
      exception_state.Throw(ExceptionState::kRGSSError,
                            "render graph 3d: pass '{}' has unknown kind {}.",
                            pass->name(), pass->kind());
      return {};
    }
    if (pass->enabled() && pass->kind() == 0 && pass->depth_only()) {
      const std::string shadow_name =
          pass->colors().empty() ? "shadow" : pass->colors()[0];
      if (pass->colors().size() > 1 || IsFinalTarget(shadow_name)) {
        exception_state.Throw(ExceptionState::kRGSSError,
                              "render graph 3d: shadow pass '{}' needs exactly "
                              "one named depth output.",
                              pass->name());
        return {};
      }
      if (textures_.count(shadow_name)) {
        exception_state.Throw(ExceptionState::kRGSSError,
                              "render graph 3d: shadow output '{}' collides "
                              "with a declared texture.",
                              shadow_name);
        return {};
      }
    }
    RenderGraphAccess3D access;
    access.name = pass->name();
    access.enabled = pass->enabled();
    for (const std::string& read : pass->reads())
      access.reads.insert(canonical(read));
    for (const auto& [slot, texture] : pass->bindings()) {
      // Compute bindings may be writable storage textures. Their access is
      // explicitly declared by reads/colors; raster bindings are always reads.
      if (pass->kind() != 2)
        access.reads.insert(canonical(texture));
    }
    for (const std::string& color : pass->colors())
      access.writes.insert(canonical(color));
    if (access.writes.empty() && pass->kind() != 2)
      access.writes.insert(pass->kind() == 0 && pass->depth_only() ? "shadow"
                                                                   : "screen");
    if (access.enabled && pass->kind() != 2) {
      for (const auto& [slot, texture] : pass->bindings()) {
        if (access.writes.count(canonical(texture))) {
          exception_state.Throw(
              ExceptionState::kRGSSError,
              "render graph 3d: pass '{}' samples its own "
              "attachment '{}'; use separate input/output textures.",
              access.name, texture);
          return {};
        }
      }
    }
    accesses.push_back(std::move(access));
  }
  std::string error;
  std::vector<size_t> order = OrderRenderGraph3D(accesses, error);
  if (!error.empty())
    exception_state.Throw(ExceptionState::kRGSSError, "render graph 3d: {}.",
                          error);
  return order;
}

void RenderGraph3D::ExecuteShadowPass(URGE_EXCEPT,
                                      RenderContext3D* context,
                                      RenderGraphPass3D* pass) {
  const std::string target_name =
      pass->colors().empty() ? std::string("shadow") : pass->colors()[0];
  context->SetCullOverride(pass->cull_override());
  context->SetSortBackToFront(false);

  if (pass->point_shadow_light()) {
    RefPtr<Light3D> light = pass->point_shadow_light();
    const glm::vec3 eye(light->world_matrix()[3]);
    const float range = std::max(light->range(), 0.1f);

    int32_t light_index = -1;
    if (context->GetQueue()) {
      const std::vector<RenderQueue3D::LightItem>& lights =
          context->GetQueue()->lights();
      for (size_t i = 0; i < std::min<size_t>(lights.size(), kMaxLights); ++i) {
        if (lights[i].type == 1 && lights[i].light.get() == light.get()) {
          light_index = static_cast<int32_t>(i);
          break;
        }
      }
    }

    if (light_index < 0 || !light->cast_shadow())
      return;
    RefPtr<RenderTarget3D> target =
        ShadowTarget(exception_state, target_name, 6);
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
      if (exception_state.HadException())
        break;
      context->SetViewOverride(faces[face]);
      context->BeginPassEx(exception_state, target, {},
                           (pass->clear_flags() & 2u) != 0,
                           (pass->clear_flags() & 4u) != 0, face);
      context->SetShadowPassMode(true);
      context->DrawFilter(exception_state, pass->tags(), pass->tag_ids(),
                          pass->queue_min(), pass->queue_max(), pass->layers());
      context->EndPass();
    }
    return;
  }

  const int32_t cascades = std::clamp(pass->cascades(), 1, 4);
  RefPtr<RenderTarget3D> target =
      ShadowTarget(exception_state, target_name, cascades);
  if (!target)
    return;

  RefPtr<Camera3D> camera = pass->camera()
                                ? pass->camera()
                                : (camera_ ? camera_ : context->GetCamera());

  glm::vec3 light_direction(0.0f, -1.0f, 0.0f);
  int32_t light_index = -1;
  if (context->GetQueue()) {
    const auto& lights = context->GetQueue()->lights();
    for (size_t i = 0; i < std::min<size_t>(lights.size(), kMaxLights); ++i) {
      const auto& item = lights[i];
      if (item.type == 0 && item.cast_shadow &&
          (!pass->shadow_light() ||
           item.light.get() == pass->shadow_light().get())) {
        light_direction = item.direction;
        light_index = static_cast<int32_t>(i);
        break;
      }
    }
  }

  if (!camera)
    return;

  if (light_index >= 0) {
    context->SetShadowLightIndex(light_index);
    const std::vector<glm::mat4> matrices = camera->ComputeCascades(
        light_direction, cascades, pass->split_lambda(),
        final_target_ ? final_target_->size() : glm::ivec2(1),
        pass->shadow_distance(), target->Width());

    context->SetShadowCascades(cascades);
    context->SetShadowFade(pass->cascade_fade());
    context->SetShadowSplits(camera->ComputeSplits(
        cascades, pass->split_lambda(), pass->shadow_distance()));

    for (int32_t cascade = 0; cascade < cascades; ++cascade) {
      if (exception_state.HadException())
        break;
      context->SetShadow(target, matrices[cascade], cascade);
      context->SetViewOverride(matrices[cascade]);
      context->BeginPassEx(exception_state, target, {},
                           (pass->clear_flags() & 2u) != 0,
                           (pass->clear_flags() & 4u) != 0, cascade);
      context->SetShadowPassMode(true);
      context->DrawFilter(exception_state, pass->tags(), pass->tag_ids(),
                          pass->queue_min(), pass->queue_max(), pass->layers());
      context->EndPass();
    }
    return;
  }

  // No eligible directional light means no directional shadow this frame.
  // BeginFrame clears the old binding; do not substitute the camera as a light.
}

void RenderGraph3D::ExecuteRasterPass(URGE_EXCEPT,
                                      RenderContext3D* context,
                                      RenderGraphPass3D* pass) {
  RefPtr<Camera3D> camera = pass->camera()
                                ? pass->camera()
                                : (camera_ ? camera_ : context->GetCamera());
  if (camera)
    context->SetViewProjection(camera);

  RefPtr<RenderTarget3D> target = TargetForPass(exception_state, pass);
  if (exception_state.HadException() || !target)
    return;

  if (context->GetQueue()) {
    const auto items = context->GetQueue()->Filter(
        pass->tags(), pass->tag_ids(), pass->queue_min(), pass->queue_max(),
        pass->layers() ? pass->layers() : 0xFFFFFFFFu);
    for (const auto& [slot, texture] : pass->bindings()) {
      const wgpu::TextureView view = ResolveColorView(exception_state, texture);
      if (exception_state.HadException() || !view)
        return;
      const RefPtr<GPUTextureView> wrapped =
          MakeRefCounted<GPUTextureView>(view);
      for (const auto* item : items)
        item->pass->SetTextureView(slot, wrapped);
    }
  }

  const bool clear_color = (pass->clear_flags() & 1u) != 0;
  std::vector<RefPtr<Color>> clears(
      static_cast<size_t>(target->ColorCount()),
      clear_color ? pass->clear_color() : RefPtr<Color>());

  context->SetShadowPassMode(false);
  context->SetCullOverride(pass->cull_override());
  context->SetSortBackToFront(pass->sort_back_to_front());
  context->BeginPassEx(exception_state, target, clears,
                       (pass->clear_flags() & 2u) != 0,
                       (pass->clear_flags() & 4u) != 0, 0);

  const uint32_t layers = pass->layers() != 0
                              ? pass->layers()
                              : (camera ? camera->culling_mask() : 0xFFFFFFFFu);

  context->DrawFilter(exception_state, pass->tags(), pass->tag_ids(),
                      pass->queue_min(), pass->queue_max(), layers);
  context->EndPass();
}

void RenderGraph3D::ExecuteFullscreenPass(URGE_EXCEPT,
                                          RenderContext3D* context,
                                          RenderGraphPass3D* pass) {
  RefPtr<MaterialPass3D> material = pass->material();
  if (!material)
    return;

  RefPtr<RenderTarget3D> target = TargetForPass(exception_state, pass);
  if (exception_state.HadException() || !target)
    return;

  material->set_fullscreen(true);

  for (const auto& binding : pass->bindings()) {
    const wgpu::TextureView view =
        ResolveColorView(exception_state, binding.second);
    if (exception_state.HadException() || !view)
      return;
    material->SetTextureView(binding.first,
                             MakeRefCounted<GPUTextureView>(view));
  }

  const bool clear_color = (pass->clear_flags() & 1u) != 0;
  std::vector<RefPtr<Color>> clears(
      static_cast<size_t>(target->ColorCount()),
      clear_color ? pass->clear_color() : RefPtr<Color>());

  context->SetShadowPassMode(false);
  context->SetCullOverride(pass->cull_override());
  context->SetSortBackToFront(false);
  context->BeginPassEx(exception_state, target, clears,
                       (pass->clear_flags() & 2u) != 0,
                       (pass->clear_flags() & 4u) != 0, 0);
  context->DrawFullscreen(exception_state, material);
  context->EndPass();
}

void RenderGraph3D::Execute(URGE_EXCEPT, RenderContext3D* context) {
  if (!context || !context->GetTarget())
    return;

  final_target_ = context->GetTarget();
  if (camera_)
    context->SetViewProjection(camera_);
  if (ambient_)
    context->SetAmbient(ambient_);

  const RefPtr<Camera3D> default_camera =
      camera_ ? camera_ : context->GetCamera();
  const std::vector<size_t> order = BuildOrder(exception_state);

  for (size_t index : order) {
    if (exception_state.HadException())
      break;
    RenderGraphPass3D* pass = passes_[index].get();
    if (!pass->enabled())
      continue;
    pass->SetContext(context);
    context->SetViewProjection(pass->camera() ? pass->camera()
                                              : default_camera);
    if (pass->kind() == 2 && pass->compute()) {
      for (const auto& [slot, texture] : pass->bindings()) {
        RefPtr<RenderTarget3D> target = Target(exception_state, texture);
        if (target)
          pass->compute()->SetTarget(slot, target, 0);
      }
    }

    if (!exception_state.HadException()) {
      switch (pass->kind()) {
        case 2:
          if (pass->compute())
            pass->compute()->Execute(exception_state, context);
          break;
        case 1:
          ExecuteFullscreenPass(exception_state, context, pass);
          break;
        default:
          if (pass->depth_only())
            ExecuteShadowPass(exception_state, context, pass);
          else
            ExecuteRasterPass(exception_state, context, pass);
          break;
      }
    }

    context->EndPass();
    pass->SetContext(nullptr);
  }

  context->SetViewProjection(default_camera);
  context->SetShadowPassMode(false);
  context->SetCullOverride(-1);
  context->SetSortBackToFront(false);
  final_target_ = nullptr;
}

}

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

#include "core/graphics/effect.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/render/device.h"
#include "core/render/pipeline.h"
#include "core/render/wgsl_reflect.h"

namespace urge {

namespace {

constexpr std::string_view kVertexEntry = "vs_main";
constexpr std::string_view kFragmentEntry = "fs_main";

uint64_t AlignUniformSize(uint64_t size) {
  constexpr uint64_t kAlignment = 16;
  return ((size + kAlignment - 1) / kAlignment) * kAlignment;
}

class EffectDefaults {
 public:
  static EffectDefaults& Get() {
    static EffectDefaults instance;
    return instance;
  }

  wgpu::TextureView white_view() const { return white_view_; }

  wgpu::Sampler sampler() const { return sampler_; }

 private:
  EffectDefaults() {
    wgpu::TextureDescriptor texture_desc;
    texture_desc.usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    texture_desc.dimension = wgpu::TextureDimension::e2D;
    texture_desc.size.width = 1;
    texture_desc.size.height = 1;
    texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    texture_ = GPUDevice::Get().device().CreateTexture(&texture_desc);

    const std::uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = texture_;

    wgpu::TexelCopyBufferLayout layout;
    layout.bytesPerRow = sizeof(white);
    layout.rowsPerImage = 1;

    wgpu::Extent3D size;
    size.width = 1;
    size.height = 1;
    size.depthOrArrayLayers = 1;
    GPUDevice::Get().queue().WriteTexture(&destination, white, sizeof(white),
                                          &layout, &size);

    white_view_ = texture_.CreateView(nullptr);

    wgpu::SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_desc.magFilter = wgpu::FilterMode::Nearest;
    sampler_desc.minFilter = wgpu::FilterMode::Nearest;
    sampler_ = GPUDevice::Get().device().CreateSampler(&sampler_desc);
  }

  wgpu::Texture texture_;
  wgpu::TextureView white_view_;
  wgpu::Sampler sampler_;
};

}

Effect::Effect(URGE_EXCEPT, std::string vs_wgsl,
               std::string fs_wgsl,
               std::string blend_states) {
  CreateInternal(exception_state, std::move(vs_wgsl), std::move(fs_wgsl),
                 std::move(blend_states));
}

Effect::Effect(URGE_EXCEPT, RefPtr<Effect> other) {
  holder_ = other->holder_;
  resources_ = other->resources_;
  vs_wgsl_ = other->vs_wgsl_;
  fs_wgsl_ = other->fs_wgsl_;
  blend_states_ = other->blend_states_;
  binding_dirty_ = true;
}

Effect::~Effect() = default;

void Effect::SetFloat(URGE_EXCEPT, uint32_t slot, std::vector<float> data) {
  SetBufferBytes(exception_state, slot, data.data(),
                 static_cast<uint32_t>(data.size() * sizeof(float)));
}

void Effect::SetInt(URGE_EXCEPT, uint32_t slot, std::vector<int32_t> data) {
  SetBufferBytes(exception_state, slot, data.data(),
                 static_cast<uint32_t>(data.size() * sizeof(int32_t)));
}

void Effect::SetTexture(uint32_t slot, RefPtr<Bitmap> texture) {
  SlotResource& resource = resources_[slot];
  resource.kind = SlotKind::kTexture;
  resource.bitmap = texture;
  binding_dirty_ = true;
}

void Effect::SetSampler(uint32_t slot, RefPtr<Bitmap> texture) {
  SlotResource& resource = resources_[slot];
  resource.kind = SlotKind::kSampler;
  resource.bitmap = texture;
  binding_dirty_ = true;
}

void Effect::SetBlock(URGE_EXCEPT, uint32_t slot, std::string uniform_block) {
  SetBufferBytes(exception_state, slot, uniform_block.data(),
                 static_cast<uint32_t>(uniform_block.size()));
}

wgpu::RenderPipeline Effect::AcquirePipeline(URGE_EXCEPT) {
  if (!holder_)
    RebuildPipeline(exception_state);
  return holder_->pipeline;
}

wgpu::BindGroup Effect::AcquireBindGroup(URGE_EXCEPT) {
  if (binding_dirty_ || binding_ == nullptr)
    RebuildBindGroup(exception_state);
  return binding_;
}

void Effect::SetFilterSource(RefPtr<Bitmap> texture) {
  const auto stage = [&](uint32_t slot, SlotKind kind) {
    SlotResource& resource = resources_[slot];
    if (resource.bitmap.get() == texture.get() && resource.kind == kind)
      return;

    resource.kind = kind;
    resource.bitmap = texture;
    binding_dirty_ = true;
  };

  stage(0, SlotKind::kTexture);
  stage(1, SlotKind::kSampler);
}

void Effect::CreateInternal(URGE_EXCEPT, std::string vs_wgsl,
                            std::string fs_wgsl,
                            std::string blend_states) {
  vs_wgsl_ = std::move(vs_wgsl);
  fs_wgsl_ = std::move(fs_wgsl);
  blend_states_ = std::move(blend_states);

  RebuildPipeline(exception_state);
}

void Effect::RebuildPipeline(URGE_EXCEPT) {
  using util::BufferLayout;
  using util::LayoutEntry;

  const auto vertex_stage = wgpu::ShaderStage::Vertex;

  std::vector<std::pair<uint32_t, std::vector<LayoutEntry>>> groups;
  groups.push_back(
      {0,
       {LayoutEntry{0, vertex_stage,
                    BufferLayout::Uniform(false, sizeof(SceneData))}}});
  groups.push_back(
      {1,
       {LayoutEntry{0, vertex_stage,
                    BufferLayout::Uniform(true, sizeof(ObjectData))}}});

  const std::vector<wgsl::LayoutGroup> reflected =
      wgsl::Reflect(vs_wgsl_, fs_wgsl_);
  uint32_t max_group = 1;
  for (const wgsl::LayoutGroup& group : reflected)
    max_group = std::max(max_group, group.first);

  for (uint32_t index = 2; index <= max_group; ++index) {
    std::vector<LayoutEntry> entries;
    for (const wgsl::LayoutGroup& group : reflected)
      if (group.first == index)
        entries = group.second;
    groups.push_back({index, std::move(entries)});
  }

  const std::optional<wgpu::BlendState> blend = ParseBlendState(exception_state, blend_states_);

  const ShaderSource vertex{vs_wgsl_, kVertexEntry, groups};
  const ShaderSource fragment{fs_wgsl_, kFragmentEntry, {}};
  Pipeline shader(vertex, fragment, QuadVertexLayout(), {1});

  wgpu::PrimitiveState primitive = GetDefaultPrimitiveState();

  wgpu::ColorTargetState target;
  target.format = kColorTargetFormat;
  target.blend = blend ? &*blend : nullptr;

  const wgpu::RenderPipeline pipeline =
      shader.MakeState(primitive, *GetDepthStencilState(), {target});

  auto holder = MakeRefCounted<PipelineHolder>();
  holder->pipeline = pipeline;

  holder_ = std::move(holder);
  binding_dirty_ = true;

  LOGGER_DEBUG("effect: pipeline built from the reflected layout");
}

void Effect::SetBufferBytes(URGE_EXCEPT, uint32_t slot, const void* data, uint32_t size) {
  const uint64_t buffer_size = AlignUniformSize(std::max<uint64_t>(size, 16));

  wgpu::BufferDescriptor desc;
  desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  desc.size = buffer_size;
  wgpu::Buffer buffer = GPUDevice::Get().device().CreateBuffer(&desc);
  if (buffer == nullptr) {
    exception_state.Throw(ExceptionState::kGPUError, "effect: the device rejected a {} byte uniform buffer.",
                    buffer_size);
    return;
  }
  std::vector<std::uint8_t> staging(static_cast<std::size_t>(buffer_size), 0);
  if (size)
    std::memcpy(staging.data(), data, size);
  GPUDevice::Get().queue().WriteBuffer(buffer, 0, staging.data(), buffer_size);

  SlotResource& resource = resources_[slot];
  resource.kind = SlotKind::kBuffer;
  resource.buffer = buffer;
  resource.size = buffer_size;
  binding_dirty_ = true;
}

void Effect::RebuildBindGroup(URGE_EXCEPT) {
  const wgpu::BindGroupLayout layout = holder_->pipeline.GetBindGroupLayout(2);
  if (layout == nullptr) {
    exception_state.Throw(ExceptionState::kGPUError,
                          "effect: the pipeline carries no custom bind group.");
    return;
  }
  std::vector<wgpu::BindGroupEntry> entries;
  entries.reserve(resources_.size());

  for (const auto& [binding, resource] : resources_) {
    wgpu::BindGroupEntry entry;
    entry.binding = binding;

    switch (resource.kind) {
      case SlotKind::kTexture: {
        const bool usable = resource.bitmap && !resource.bitmap->IsDisposed();
        entry.textureView = usable ? resource.bitmap->texture_view()
                                   : EffectDefaults::Get().white_view();
        break;
      }
      case SlotKind::kSampler: {
        const bool usable = resource.bitmap && !resource.bitmap->IsDisposed();
        entry.sampler = usable ? resource.bitmap->sampler()
                               : EffectDefaults::Get().sampler();
        break;
      }
      case SlotKind::kBuffer:
        entry.buffer = resource.buffer;
        entry.offset = 0;
        entry.size = WGPU_WHOLE_SIZE;
        break;
    }

    entries.push_back(entry);
  }

  wgpu::BindGroupDescriptor group_desc;
  group_desc.layout = layout;
  group_desc.entryCount = entries.size();
  group_desc.entries = entries.empty() ? nullptr : entries.data();

  binding_ = GPUDevice::Get().device().CreateBindGroup(&group_desc);
  if (binding_ == nullptr) {
    exception_state.Throw(ExceptionState::kGPUError,
                          "effect: the device rejected the custom bind group.");
    return;
  }
  binding_dirty_ = false;
}

}

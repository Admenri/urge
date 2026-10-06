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

#include "core/effect.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/exception.h"
#include "core/device.h"
#include "core/logger.h"
#include "core/pipeline.h"

namespace urge {

namespace {

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

  wgpu::Buffer zero_buffer(uint64_t size) {
    const uint64_t aligned = AlignUniformSize(size);
    const auto found = buffers_.find(aligned);
    if (found != buffers_.end())
      return found->second;

    wgpu::BufferDescriptor desc;
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    desc.size = aligned;
    wgpu::Buffer buffer = GPUDevice::Get().device().CreateBuffer(&desc);

    const std::vector<std::uint8_t> zeros(static_cast<std::size_t>(aligned), 0);
    GPUDevice::Get().queue().WriteBuffer(buffer, 0, zeros.data(), aligned);

    buffers_.emplace(aligned, buffer);
    return buffer;
  }

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
  std::map<uint64_t, wgpu::Buffer> buffers_;
};

bool IsUniformBuffer(const ShaderBinding& binding) {
  return !binding.texture && !binding.sampler &&
         binding.buffer == WGPUBufferBindingType_Uniform;
}

}  // namespace

Effect::Effect(std::string vs_glsl,
               std::string fs_glsl,
               std::string blend_states) {
  CreateInternal(std::move(vs_glsl), std::move(fs_glsl),
                 std::move(blend_states));
}

Effect::Effect(RefPtr<Effect> other) {
  if (!other)
    throw Exception(Exception::kRGSSError, "effect: cannot copy a null effect.");

  holder_ = other->holder_;
  resources_ = other->resources_;
  binding_dirty_ = true;
}

Effect::~Effect() = default;

void Effect::SetFloat(uint32_t slot, std::vector<float> data) {
  SetBufferBytes(slot, data.data(),
                 static_cast<uint32_t>(data.size() * sizeof(float)));
}

void Effect::SetInt(uint32_t slot, std::vector<int32_t> data) {
  SetBufferBytes(slot, data.data(),
                 static_cast<uint32_t>(data.size() * sizeof(int32_t)));
}

void Effect::SetTexture(uint32_t slot, RefPtr<Bitmap> texture) {
  const auto found = holder_->custom_bindings.find(slot);
  if (found == holder_->custom_bindings.end())
    throw Exception(Exception::kRGSSError,
                    "effect: the stages declare no binding at set 2 slot {}.",
                    slot);
  if (!found->second.texture)
    throw Exception(Exception::kRGSSError,
                    "effect: set 2 slot {} is not a texture binding.", slot);

  resources_[slot].bitmap = texture;
  binding_dirty_ = true;
}

void Effect::SetSampler(uint32_t slot, RefPtr<Bitmap> texture) {
  const auto found = holder_->custom_bindings.find(slot);
  if (found == holder_->custom_bindings.end())
    throw Exception(Exception::kRGSSError,
                    "effect: the stages declare no binding at set 2 slot {}.",
                    slot);
  if (!found->second.sampler)
    throw Exception(Exception::kRGSSError,
                    "effect: set 2 slot {} is not a sampler binding.", slot);

  resources_[slot].bitmap = texture;
  binding_dirty_ = true;
}

void Effect::SetBlock(uint32_t slot, std::string uniform_block) {
  SetBufferBytes(slot, uniform_block.data(),
                 static_cast<uint32_t>(uniform_block.size()));
}

wgpu::RenderPipeline Effect::AcquirePipeline() { return holder_->pipeline; }

wgpu::BindGroup Effect::AcquireBindGroup() {
  if (binding_dirty_ || binding_ == nullptr)
    RebuildBindGroup();
  return binding_;
}

void Effect::SetFilterSource(RefPtr<Bitmap> texture) {
  const auto stage = [&](uint32_t slot, bool wants_texture) {
    const auto found = holder_->custom_bindings.find(slot);
    if (found == holder_->custom_bindings.end())
      return;

    const ShaderBinding& info = found->second;
    if (wants_texture ? !info.texture : !info.sampler)
      return;

    SlotResource& resource = resources_[slot];
    if (resource.bitmap.get() == texture.get())
      return;

    resource.bitmap = texture;
    binding_dirty_ = true;
  };

  stage(0, true);
  stage(1, false);
}

void Effect::CreateInternal(std::string vs_glsl,
                            std::string fs_glsl,
                            std::string blend_states) {
  if (vs_glsl.empty() || fs_glsl.empty())
    throw Exception(Exception::kRGSSError,
                    "effect: the vertex and the fragment stage both need a "
                    "source.");

  const std::optional<wgpu::BlendState> blend = ParseBlendState(blend_states);

  Pipeline shader(vs_glsl, fs_glsl, {{0, 1, 2}}, {1}, 3);

  const std::vector<ShaderBinding> scene = shader.group_bindings(0);
  const std::vector<ShaderBinding> object = shader.group_bindings(1);
  if (scene.size() != 1 || !IsUniformBuffer(scene[0]) ||
      scene[0].min_binding_size < sizeof(SceneData) || object.size() != 1 ||
      !IsUniformBuffer(object[0]) ||
      object[0].min_binding_size < sizeof(ObjectData))
    throw Exception(
        Exception::kRGSSError,
        "effect: the stages have to declare the scene uniform at set 0 "
        "binding 0 and the object uniform at set 1 binding 0, like the built "
        "in shaders.");

  const wgpu::RenderPipeline pipeline =
      shader.MakeDefaultState(blend ? &*blend : nullptr);

  auto holder = MakeRefCounted<PipelineHolder>();
  holder->pipeline = pipeline;
  for (const ShaderBinding& binding : shader.group_bindings(2))
    holder->custom_bindings.emplace(binding.binding, binding);

  holder_ = std::move(holder);
  binding_dirty_ = true;

  LOGGER_DEBUG("effect: pipeline built with {} custom binding(s)",
               holder_->custom_bindings.size());
}

void Effect::SetBufferBytes(uint32_t slot, const void* data, uint32_t size) {
  const auto found = holder_->custom_bindings.find(slot);
  if (found == holder_->custom_bindings.end())
    throw Exception(Exception::kRGSSError,
                    "effect: the stages declare no binding at set 2 slot {}.",
                    slot);
  if (!IsUniformBuffer(found->second))
    throw Exception(Exception::kRGSSError,
                    "effect: set 2 slot {} is not a uniform buffer binding.",
                    slot);

  const uint64_t buffer_size =
      AlignUniformSize(std::max<uint64_t>(size, found->second.min_binding_size));

  wgpu::BufferDescriptor desc;
  desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  desc.size = buffer_size;
  wgpu::Buffer buffer = GPUDevice::Get().device().CreateBuffer(&desc);
  if (buffer == nullptr)
    throw Exception(Exception::kGPUError,
                    "effect: the device rejected a {} byte uniform buffer.",
                    buffer_size);

  std::vector<std::uint8_t> staging(static_cast<std::size_t>(buffer_size), 0);
  if (size)
    std::memcpy(staging.data(), data, size);
  GPUDevice::Get().queue().WriteBuffer(buffer, 0, staging.data(),
                                       buffer_size);

  SlotResource& resource = resources_[slot];
  resource.buffer = buffer;
  binding_dirty_ = true;
}

void Effect::RebuildBindGroup() {
  const wgpu::BindGroupLayout layout = holder_->pipeline.GetBindGroupLayout(2);
  if (layout == nullptr)
    throw Exception(Exception::kGPUError,
                    "effect: the pipeline carries no custom bind group.");

  std::vector<wgpu::BindGroupEntry> entries;
  entries.reserve(holder_->custom_bindings.size());

  for (const auto& [binding, info] : holder_->custom_bindings) {
    const auto found = resources_.find(binding);
    const SlotResource* resource =
        found == resources_.end() ? nullptr : &found->second;

    wgpu::BindGroupEntry entry;
    entry.binding = binding;

    if (info.texture) {
      const bool usable =
          resource && resource->bitmap && !resource->bitmap->IsDisposed();
      entry.textureView = usable ? resource->bitmap->texture_view()
                                 : EffectDefaults::Get().white_view();
    } else if (info.sampler) {
      const bool usable =
          resource && resource->bitmap && !resource->bitmap->IsDisposed();
      entry.sampler = usable ? resource->bitmap->sampler()
                             : EffectDefaults::Get().sampler();
    } else if (resource && resource->buffer) {
      entry.buffer = resource->buffer;
      entry.offset = 0;
      entry.size = WGPU_WHOLE_SIZE;
    } else {
      entry.buffer = EffectDefaults::Get().zero_buffer(info.min_binding_size);
      entry.offset = 0;
      entry.size = WGPU_WHOLE_SIZE;
    }

    entries.push_back(entry);
  }

  wgpu::BindGroupDescriptor group_desc;
  group_desc.layout = layout;
  group_desc.entryCount = entries.size();
  group_desc.entries = entries.empty() ? nullptr : entries.data();

  binding_ = GPUDevice::Get().device().CreateBindGroup(&group_desc);
  if (binding_ == nullptr)
    throw Exception(Exception::kGPUError,
                    "effect: the device rejected the custom bind group.");
  binding_dirty_ = false;
}

}  // namespace urge

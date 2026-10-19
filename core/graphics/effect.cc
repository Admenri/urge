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

#include "core/graphics/device.h"
#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/graphics/pipeline.h"

namespace urge {

namespace {

// A custom shader enters at the same names the built-in ones do.  The engine
// cannot look the entry point up in the source any more -- the layout comes
// from the device, which is only told a name -- so this is the convention.
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

}  // namespace

Effect::Effect(std::string vs_wgsl,
               std::string fs_wgsl,
               std::string blend_states) {
  CreateInternal(std::move(vs_wgsl), std::move(fs_wgsl),
                 std::move(blend_states));
}

Effect::Effect(RefPtr<Effect> other) {
  if (!other)
    throw Exception(Exception::kRGSSError,
                    "effect: cannot copy a null effect.");

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

void Effect::SetBlock(uint32_t slot, std::string uniform_block) {
  SetBufferBytes(slot, uniform_block.data(),
                 static_cast<uint32_t>(uniform_block.size()));
}

wgpu::RenderPipeline Effect::AcquirePipeline() {
  return holder_->pipeline;
}

wgpu::BindGroup Effect::AcquireBindGroup() {
  if (binding_dirty_ || binding_ == nullptr)
    RebuildBindGroup();
  return binding_;
}

void Effect::SetFilterSource(RefPtr<Bitmap> texture) {
  // The filter convention: the canvas goes in as a texture at slot 0 and its
  // sampler at slot 1.  Both are filled here because a filter shader samples
  // the canvas and has nothing else to bind.
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

void Effect::CreateInternal(std::string vs_wgsl,
                            std::string fs_wgsl,
                            std::string blend_states) {
  if (vs_wgsl.empty() || fs_wgsl.empty())
    throw Exception(Exception::kRGSSError,
                    "effect: the vertex and the fragment stage both need a "
                    "source.");

  const std::optional<wgpu::BlendState> blend = ParseBlendState(blend_states);

  // A null pipeline layout: the device reads the two stages and builds one.
  // That is the only way a shader written at run time can be laid out, and
  // `GetBindGroupLayout` below is how the result comes back.
  const ShaderSource vertex{vs_wgsl, kVertexEntry, {}};
  const ShaderSource fragment{fs_wgsl, kFragmentEntry, {}};
  Pipeline shader = Pipeline::AutoLayout(vertex, fragment, QuadVertexLayout());

  const wgpu::RenderPipeline pipeline =
      shader.MakeDefaultState(blend ? &*blend : nullptr);

  // The engine binds its own scene and object groups at 0 and 1, so a shader
  // that never declares them would only fail once a frame was already being
  // drawn.  The derived layout is the earliest place it shows up: a group the
  // shader did not declare has no layout.
  if (!pipeline.GetBindGroupLayout(0) || !pipeline.GetBindGroupLayout(1))
    throw Exception(
        Exception::kRGSSError,
        "effect: the stages have to declare the scene uniform at "
        "@group(0) @binding(0) and the object uniform at @group(1) "
        "@binding(0), like the built in shaders do.");

  auto holder = MakeRefCounted<PipelineHolder>();
  holder->pipeline = pipeline;

  holder_ = std::move(holder);
  binding_dirty_ = true;

  LOGGER_DEBUG("effect: pipeline built with a layout derived from the stages");
}

void Effect::SetBufferBytes(uint32_t slot, const void* data, uint32_t size) {
  // There is no minimum to size against any more -- the device owns the layout
  // -- so the buffer is exactly as big as what was handed in, rounded up to a
  // whole uniform.  A shader whose struct is larger than the bytes the script
  // sends is a device error on the next draw.
  const uint64_t buffer_size = AlignUniformSize(std::max<uint64_t>(size, 16));

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
  GPUDevice::Get().queue().WriteBuffer(buffer, 0, staging.data(), buffer_size);

  SlotResource& resource = resources_[slot];
  resource.kind = SlotKind::kBuffer;
  resource.buffer = buffer;
  resource.size = buffer_size;
  binding_dirty_ = true;
}

void Effect::RebuildBindGroup() {
  const wgpu::BindGroupLayout layout = holder_->pipeline.GetBindGroupLayout(2);
  if (layout == nullptr)
    throw Exception(Exception::kGPUError,
                    "effect: the pipeline carries no custom bind group.");

  std::vector<wgpu::BindGroupEntry> entries;
  entries.reserve(resources_.size());

  // One entry per slot the script has filled.  The layout the device derived
  // is the authority on what it wants; since the engine cannot read it, a slot
  // that was never set is simply absent, and a shader that declares one the
  // script does not fill is a device error at the next draw.
  for (const auto& [binding, resource] : resources_) {
    wgpu::BindGroupEntry entry;
    entry.binding = binding;

    switch (resource.kind) {
      case SlotKind::kTexture: {
        const bool usable =
            resource.bitmap && !resource.bitmap->IsDisposed();
        entry.textureView = usable ? resource.bitmap->texture_view()
                                   : EffectDefaults::Get().white_view();
        break;
      }
      case SlotKind::kSampler: {
        const bool usable =
            resource.bitmap && !resource.bitmap->IsDisposed();
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
  if (binding_ == nullptr)
    throw Exception(Exception::kGPUError,
                    "effect: the device rejected the custom bind group.");
  binding_dirty_ = false;
}

}  // namespace urge

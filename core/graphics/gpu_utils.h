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

#include <tuple>
#include <variant>

#include "core/device.h"

namespace urge::util {

struct BufferSet {
  wgpu::Buffer buffer;
  uint64_t offset = 0;
  uint64_t size = WGPU_WHOLE_SIZE;
  BufferSet(wgpu::Buffer b) : buffer(b) {}
  BufferSet(wgpu::Buffer b, uint64_t o, uint64_t s)
      : buffer(b), offset(o), size(s) {}
};

struct SamplerSet {
  wgpu::Sampler sampler;
  SamplerSet(wgpu::Sampler s) : sampler(s) {}
};

struct TextureViewSet {
  wgpu::TextureView view;
  TextureViewSet(wgpu::TextureView v) : view(v) {}
};

using BindingSetType = std::variant<BufferSet, SamplerSet, TextureViewSet>;
inline wgpu::BindGroup CreateBindGroup(
    wgpu::BindGroupLayout layout,
    const std::vector<std::pair<uint32_t, BindingSetType>>& sets) {
  std::vector<wgpu::BindGroupEntry> entries = {};
  for (auto& set : sets) {
    wgpu::BindGroupEntry entry = {};
    entry.binding = set.first;
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, BufferSet>) {
            entry.buffer = arg.buffer;
            entry.offset = arg.offset;
            entry.size = arg.size;
          } else if constexpr (std::is_same_v<T, SamplerSet>) {
            entry.sampler = arg.sampler;
          } else if constexpr (std::is_same_v<T, TextureViewSet>) {
            entry.textureView = arg.view;
          }
        },
        set.second);
    entries.push_back(std::move(entry));
  }

  wgpu::BindGroupDescriptor group_desc = {};
  group_desc.layout = layout;
  group_desc.entryCount = entries.size();
  group_desc.entries = entries.data();
  return g_device.CreateBindGroup(&group_desc);
}

struct BufferLayout {
  wgpu::BufferBindingType type = wgpu::BufferBindingType::BindingNotUsed;
  bool has_dynamic_offset = false;
  uint64_t min_binding_size = 0;
  BufferLayout(wgpu::BufferBindingType t, bool off, uint64_t s)
      : type(t), has_dynamic_offset(off), min_binding_size(s) {}

  static BufferLayout Uniform(bool d = false, uint64_t s = 0) {
    return BufferLayout(wgpu::BufferBindingType::Uniform, d, s);
  }
  static BufferLayout Storage(bool d = false, uint64_t s = 0) {
    return BufferLayout(wgpu::BufferBindingType::Storage, d, s);
  }
  static BufferLayout ReadOnlyStorage(bool d = false, uint64_t s = 0) {
    return BufferLayout(wgpu::BufferBindingType::ReadOnlyStorage, d, s);
  }
};

struct SamplerLayout {
  wgpu::SamplerBindingType type = wgpu::SamplerBindingType::BindingNotUsed;
  SamplerLayout(wgpu::SamplerBindingType t) : type(t) {}

  static SamplerLayout Filtering() {
    return SamplerLayout(wgpu::SamplerBindingType::Filtering);
  }

  static SamplerLayout NonFiltering() {
    return SamplerLayout(wgpu::SamplerBindingType::NonFiltering);
  }

  static SamplerLayout Comparison() {
    return SamplerLayout(wgpu::SamplerBindingType::Comparison);
  }
};

struct TextureLayout {
  wgpu::TextureSampleType type = wgpu::TextureSampleType::BindingNotUsed;
  wgpu::TextureViewDimension dimension = wgpu::TextureViewDimension::Undefined;
  bool multisampled = false;
  TextureLayout(wgpu::TextureSampleType t, wgpu::TextureViewDimension d, bool m)
      : type(t), dimension(d), multisampled(m) {}

  static TextureLayout Float(
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined,
      bool m = false) {
    return TextureLayout(wgpu::TextureSampleType::Float, d, m);
  }

  static TextureLayout UnfilterableFloat(
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined,
      bool m = false) {
    return TextureLayout(wgpu::TextureSampleType::UnfilterableFloat, d, m);
  }

  static TextureLayout Depth(
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined,
      bool m = false) {
    return TextureLayout(wgpu::TextureSampleType::Depth, d, m);
  }

  static TextureLayout Sint(
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined,
      bool m = false) {
    return TextureLayout(wgpu::TextureSampleType::Sint, d, m);
  }

  static TextureLayout Uint(
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined,
      bool m = false) {
    return TextureLayout(wgpu::TextureSampleType::Uint, d, m);
  }
};

struct StorageTextureLayout {
  wgpu::StorageTextureAccess type = wgpu::StorageTextureAccess::BindingNotUsed;
  wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
  wgpu::TextureViewDimension dimension = wgpu::TextureViewDimension::Undefined;
  StorageTextureLayout(wgpu::StorageTextureAccess t,
                       wgpu::TextureFormat f,
                       wgpu::TextureViewDimension d)
      : type(t), format(f), dimension(d) {}

  static StorageTextureLayout WriteOnly(
      wgpu::TextureFormat f = wgpu::TextureFormat::Undefined,
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined) {
    return StorageTextureLayout(wgpu::StorageTextureAccess::WriteOnly, f, d);
  }

  static StorageTextureLayout ReadOnly(
      wgpu::TextureFormat f = wgpu::TextureFormat::Undefined,
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined) {
    return StorageTextureLayout(wgpu::StorageTextureAccess::ReadOnly, f, d);
  }

  static StorageTextureLayout ReadWrite(
      wgpu::TextureFormat f = wgpu::TextureFormat::Undefined,
      wgpu::TextureViewDimension d = wgpu::TextureViewDimension::Undefined) {
    return StorageTextureLayout(wgpu::StorageTextureAccess::ReadWrite, f, d);
  }
};

using LayoutVariant = std::
    variant<BufferLayout, SamplerLayout, TextureLayout, StorageTextureLayout>;
using LayoutEntry = std::tuple<uint32_t, wgpu::ShaderStage, LayoutVariant>;
inline wgpu::BindGroupLayout CreateBindLayout(
    const std::vector<LayoutEntry>& sets) {
  std::vector<wgpu::BindGroupLayoutEntry> entries = {};
  for (auto& set : sets) {
    wgpu::BindGroupLayoutEntry entry = {};
    entry.binding = std::get<0>(set);
    entry.visibility = std::get<1>(set);
    entry.buffer.type = wgpu::BufferBindingType::BindingNotUsed;
    entry.sampler.type = wgpu::SamplerBindingType::BindingNotUsed;
    entry.texture.sampleType = wgpu::TextureSampleType::BindingNotUsed;
    entry.storageTexture.access = wgpu::StorageTextureAccess::BindingNotUsed;

    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, BufferLayout>) {
            entry.buffer.type = arg.type;
            entry.buffer.hasDynamicOffset = arg.has_dynamic_offset;
            entry.buffer.minBindingSize = arg.min_binding_size;
          } else if constexpr (std::is_same_v<T, SamplerLayout>) {
            entry.sampler.type = arg.type;
          } else if constexpr (std::is_same_v<T, TextureLayout>) {
            entry.texture.sampleType = arg.type;
            entry.texture.viewDimension = arg.dimension;
            entry.texture.multisampled = arg.multisampled;
          } else if constexpr (std::is_same_v<T, StorageTextureLayout>) {
            entry.storageTexture.access = arg.type;
            entry.storageTexture.format = arg.format;
            entry.storageTexture.viewDimension = arg.dimension;
          }
        },
        std::get<2>(set));
    entries.push_back(std::move(entry));
  }

  wgpu::BindGroupLayoutDescriptor layout_desc = {};
  layout_desc.entryCount = entries.size();
  layout_desc.entries = entries.data();
  return g_device.CreateBindGroupLayout(&layout_desc);
}

}  // namespace urge::util

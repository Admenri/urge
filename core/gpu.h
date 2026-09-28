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
#include <variant>
#include <vector>

#include "SDL3/SDL_video.h"
#include "webgpu/webgpu_cpp.hpp"

#include "core/object.h"

namespace urge {

class GPUDevice : public Singleton<GPUDevice> {
 public:
  //! Creates a device for a window, the surface is created from it.
  GPUDevice(SDL_Window* window);
  //! Creates a device without a surface, which cannot present.
  GPUDevice();

  wgpu::Device device() const { return device_; }
  wgpu::Adapter adapter() const { return adapter_; }
  wgpu::Surface swapchain() const { return surface_; }
  wgpu::Queue queue() const { return queue_; }

  void WaitAny(wgpu::Future future);

  /*! Blocks until the device has run out of work when \p wait is set, or
     returns as soon as it can when it is not.
      \remarks wgpu-native does not implement the future API of WebGPU yet,
      wgpuBufferMapAsync() returns an empty future and GPUDevice::WaitAny has
      therefore nothing to wait for. Polling the device is what drives the
      callback of an asynchronous operation to its completion. */
  void Poll(bool wait);

 private:
  //! Requests the adapter and the device, with the surface when there is one.
  void CreateDevice();

  wgpu::Instance instance_;
  wgpu::Surface surface_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::Queue queue_;
};

struct WBufferSet {
  wgpu::Buffer buffer;
  uint64_t offset = 0;
  uint64_t size = WGPU_WHOLE_SIZE;
  WBufferSet(wgpu::Buffer b) : buffer(b) {}
};

struct WSamplerSet {
  wgpu::Sampler sampler;
  WSamplerSet(wgpu::Sampler s) : sampler(s) {}
};

struct WTextureViewSet {
  wgpu::TextureView view;
  WTextureViewSet(wgpu::TextureView v) : view(v) {}
};

using WBinding = std::variant<WBufferSet, WSamplerSet, WTextureViewSet>;
inline wgpu::BindGroup CreateWGroup(
    wgpu::BindGroupLayout layout,
    const std::vector<std::pair<uint32_t, WBinding>>& sets) {
  std::vector<wgpu::BindGroupEntry> entries;
  for (auto& set : sets) {
    wgpu::BindGroupEntry entry;
    entry.binding = set.first;
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, WBufferSet>) {
            entry.buffer = arg.buffer;
            entry.offset = arg.offset;
            entry.size = arg.size;
          } else if constexpr (std::is_same_v<T, WSamplerSet>) {
            entry.sampler = arg.sampler;
          } else if constexpr (std::is_same_v<T, WTextureViewSet>) {
            entry.textureView = arg.view;
          }
        },
        set.second);
    entries.push_back(std::move(entry));
  }

  wgpu::BindGroupDescriptor group_desc;
  group_desc.layout = layout;
  group_desc.entryCount = entries.size();
  group_desc.entries = entries.data();
  return GPUDevice::Get().device().CreateBindGroup(&group_desc);
}

}  // namespace urge

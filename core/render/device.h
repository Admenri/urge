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
#include <string>
#include <variant>
#include <vector>

#include "SDL3/SDL_video.h"
#include "core/common/definition.h"
#include "webgpu/webgpu_cpp.hpp"

#include "core/common/object.h"

namespace urge {

struct DeviceInfo {
  std::string adapter_device;
  std::string adapter_vendor;
  std::string adapter_architecture;
  std::string adapter_description;
  std::string backend;
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;

  int cpu_cores = 0;
  uint64_t system_ram_mb = 0;
};

DeviceInfo QueryDeviceInfo(const wgpu::Adapter& adapter);

#define g_adapter GPUDevice::Get().adapter()
#define g_device GPUDevice::Get().device()
#define g_queue GPUDevice::Get().queue()
class GPUDevice : public Singleton<GPUDevice> {
 public:
  GPUDevice(URGE_EXCEPT, SDL_Window* window, std::string backend);

  wgpu::Device device() const { return device_; }
  wgpu::Adapter adapter() const { return adapter_; }
  wgpu::Surface swapchain() const { return surface_; }
  wgpu::Queue queue() const { return queue_; }

  const DeviceInfo& info() const { return info_; }

  bool RefreshSurface();

  uint64_t surface_generation() const { return surface_generation_; }

  void WaitAny(wgpu::Future future);

 private:
  bool CreateSurfaceFromWindow();

  void CreateDevice(URGE_EXCEPT, std::string backend);

  SDL_Window* window_ = nullptr;

  void* native_window_ = nullptr;

  uint64_t surface_generation_ = 0;

  wgpu::Instance instance_;
  wgpu::Surface surface_;
  wgpu::Adapter adapter_;
  wgpu::Device device_;
  wgpu::Queue queue_;

  DeviceInfo info_;
};

}

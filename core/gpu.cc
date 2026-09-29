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

#include "core/gpu.h"

#include <string_view>

#include "SDL3/SDL_platform_defines.h"

// The logging entry points of wgpu-native are native extensions, they live next
// to the standard WebGPU header rather than inside it.
#include "wgpu.h"

#include "core/logger.h"

namespace urge {

namespace {

LogLevel FromWGPULogLevel(WGPULogLevel level) {
  switch (level) {
    case WGPULogLevel_Error:
      return LogLevel::kError;
    case WGPULogLevel_Warn:
      return LogLevel::kWarn;
    case WGPULogLevel_Info:
      return LogLevel::kInfo;
    case WGPULogLevel_Debug:
      return LogLevel::kDebug;
    case WGPULogLevel_Trace:
      return LogLevel::kTrace;
    default:
      return LogLevel::kOff;
  }
}

void OnWGPULog(WGPULogLevel level, WGPUStringView message, void* /*userdata*/) {
  const LogLevel engine_level = FromWGPULogLevel(level);
  if (engine_level == LogLevel::kOff)
    return;

  const std::string_view text = wgpu::StringView(message);

  LogMessage(engine_level, "[wgpu] ", "{}", text);
}

void InstallWGPULogger() {
  wgpuSetLogCallback(&OnWGPULog, nullptr);

#if defined(NDEBUG)
  wgpuSetLogLevel(WGPULogLevel_Warn);
#else
  wgpuSetLogLevel(WGPULogLevel_Info);
#endif
}

}  // namespace

GPUDevice::GPUDevice(SDL_Window* window) {
  InstallWGPULogger();

  // Instance
  const std::vector<wgpu::InstanceFeatureName> instance_exts = {
      wgpu::InstanceFeatureName::ShaderSourceSPIRV,
  };

  wgpu::InstanceDescriptor instance_desc;
  instance_desc.requiredFeatureCount = instance_exts.size();
  instance_desc.requiredFeatures = instance_exts.data();
#if 1
  WGPUInstanceExtras instance_extras = {};
  instance_extras.chain.sType =
      static_cast<WGPUSType>(WGPUSType_InstanceExtras);
  instance_extras.flags = WGPUInstanceFlag_Empty;
  instance_desc.nextInChain =
      reinterpret_cast<wgpu::ChainedStruct*>(&instance_extras.chain);
#endif
  instance_ = wgpu::CreateInstance(&instance_desc);

  // Platform Surface, a device without a window cannot present
  if (window != nullptr) {
    wgpu::SurfaceDescriptor surface_desc;
    SDL_PropertiesID window_prop = SDL_GetWindowProperties(window);
#if defined(SDL_PLATFORM_WIN32)
    wgpu::SurfaceSourceWindowsHWND win_hwnd;
    win_hwnd.hinstance = SDL_GetPointerProperty(
        window_prop, SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
    win_hwnd.hwnd = SDL_GetPointerProperty(
        window_prop, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    surface_desc.nextInChain = &win_hwnd;
#else
#error not support
#endif
    surface_ = instance_.CreateSurface(&surface_desc);
  }

  CreateDevice();
}

GPUDevice::GPUDevice() : GPUDevice(nullptr) {}

void GPUDevice::CreateDevice() {
  // Adapter, the callback of a request runs before the call returns
  WGPURequestAdapterCallbackInfo adapter_callback = {};
  adapter_callback.mode = WGPUCallbackMode_AllowProcessEvents;
  adapter_callback.callback = [](WGPURequestAdapterStatus status,
                                 WGPUAdapter adapter, WGPUStringView message,
                                 void* userdata1, void* userdata2) {
    auto* host = static_cast<GPUDevice*>(userdata1);
    host->adapter_ = wgpu::Adapter::Acquire(adapter);
  };
  adapter_callback.userdata1 = this;

  wgpu::RequestAdapterOptions adapter_request;
  if (surface_ != nullptr)
    adapter_request.compatibleSurface = surface_;
  instance_.RequestAdapter(&adapter_request, adapter_callback);

  // Device
  WGPURequestDeviceCallbackInfo device_callback = {};
  device_callback.mode = WGPUCallbackMode_AllowProcessEvents;
  device_callback.callback = [](WGPURequestDeviceStatus status,
                                WGPUDevice device, WGPUStringView message,
                                void* userdata1, void* userdata2) {
    auto* host = static_cast<GPUDevice*>(userdata1);
    host->device_ = wgpu::Device::Acquire(device);
  };
  device_callback.userdata1 = this;

  wgpu::DeviceDescriptor device_desc;
#if 1
  WGPUDeviceExtras device_extras = {};
  device_extras.chain.sType = static_cast<WGPUSType>(WGPUSType_DeviceExtras);
  device_extras.memoryHints = WGPUMemoryHints_Performance;
  device_desc.nextInChain =
      reinterpret_cast<wgpu::ChainedStruct*>(&device_extras.chain);
#endif
  adapter_.RequestDevice(&device_desc, device_callback);

  // Queue
  queue_ = device_.GetQueue();
}

void GPUDevice::WaitAny(wgpu::Future future) {
  if (!future.id)
    return;

  wgpu::FutureWaitInfo wait_info;
  wait_info.future = future;
  instance_.WaitAny(1, &wait_info, UINT64_MAX);
}

void GPUDevice::Poll(bool wait) {
  wgpuDevicePoll(device_.Get(), wait ? 1u : 0u, nullptr);
}

}  // namespace urge

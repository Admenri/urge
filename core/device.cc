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

#include "core/device.h"

#include <string_view>

#include "SDL3/SDL_platform_defines.h"

#if defined(WEBGPU_BACKEND_WGPU)
#include "wgpu.h"
#endif  // WEBGPU_BACKEND_WGPU

#include "core/logger.h"

namespace urge {

namespace {

wgpu::BackendType FromGFXBackend(std::string backend) {
  if (backend == "d3d11")
    return wgpu::BackendType::D3D11;
  if (backend == "d3d12")
    return wgpu::BackendType::D3D12;
  if (backend == "metal")
    return wgpu::BackendType::Metal;
  if (backend == "vulkan")
    return wgpu::BackendType::Vulkan;
  if (backend == "opengl")
    return wgpu::BackendType::OpenGL;
  if (backend == "opengles")
    return wgpu::BackendType::OpenGLES;
  return wgpu::BackendType::Undefined;
}

}  // namespace

GPUDevice::GPUDevice(SDL_Window* window, std::string backend) {
  // Instance
  const std::vector<wgpu::InstanceFeatureName> instance_exts = {
      wgpu::InstanceFeatureName::ShaderSourceSPIRV,
  };

  wgpu::InstanceDescriptor instance_desc;
  instance_desc.requiredFeatureCount = instance_exts.size();
  instance_desc.requiredFeatures = instance_exts.data();
#if defined(WEBGPU_BACKEND_WGPU)
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

  CreateDevice(backend);
}

void GPUDevice::CreateDevice(std::string backend) {
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
  adapter_request.backendType = FromGFXBackend(backend);
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
  adapter_.RequestDevice(&device_desc, device_callback);

  // Dev info
  wgpu::AdapterInfo adapter_info;
  adapter_.GetInfo(&adapter_info);
  LOGGER_INFO("[GPU] Device: {} ({:#X})", std::string_view(adapter_info.device),
              adapter_info.deviceID);
  LOGGER_INFO("[GPU] Vendor: {} ({:#X})", std::string_view(adapter_info.vendor),
              adapter_info.vendorID);

  // Backend
  switch (adapter_info.backendType) {
    case wgpu::BackendType::Null:
      LOGGER_INFO("[GPU] Backend: Null");
      break;
    case wgpu::BackendType::WebGPU:
      LOGGER_INFO("[GPU] Backend: WebGPU");
      break;
    case wgpu::BackendType::D3D11:
      LOGGER_INFO("[GPU] Backend: D3D11");
      break;
    case wgpu::BackendType::D3D12:
      LOGGER_INFO("[GPU] Backend: D3D12");
      break;
    case wgpu::BackendType::Metal:
      LOGGER_INFO("[GPU] Backend: Metal");
      break;
    case wgpu::BackendType::Vulkan:
      LOGGER_INFO("[GPU] Backend: Vulkan");
      break;
    case wgpu::BackendType::OpenGL:
      LOGGER_INFO("[GPU] Backend: OpenGL");
      break;
    case wgpu::BackendType::OpenGLES:
      LOGGER_INFO("[GPU] Backend: OpenGLES");
      break;
    default:
      LOGGER_WARN("[GPU] Backend: Unknown ({:#X})",
                  static_cast<uint32_t>(adapter_info.backendType));
  }

  // Queue
  queue_ = device_.GetQueue();
}

void GPUDevice::WaitAny(wgpu::Future future) {
#if defined(WEBGPU_BACKEND_WGPU)
  wgpuDevicePoll(device_.Get(), true, nullptr);
#else
  if (future.id) {
    wgpu::FutureWaitInfo wait_info = {};
    wait_info.future = future;
    instance_.WaitAny(1, &wait_info, UINT64_MAX);
  }
#endif
}

}  // namespace urge

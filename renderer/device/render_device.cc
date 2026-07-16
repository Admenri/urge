// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/device/render_device.h"

#include "base/buildflags/build.h"

// WGPU-Native specific
#include "wgpu-cmake/wgpu-native/ffi/wgpu.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // OS_WIN

namespace renderer {

RenderDevice::RenderDevice(base::WeakPtr<ui::Widget> window,
                           wgpu::Instance instance,
                           wgpu::Adapter adapter,
                           wgpu::Surface surface,
                           wgpu::Device device,
                           wgpu::Queue queue)
    : window_(window),
      instance_(instance),
      adapter_(adapter),
      surface_(surface),
      device_(device),
      queue_(queue) {}

RenderDevice::~RenderDevice() = default;

// static
std::unique_ptr<RenderDevice> RenderDevice::Create(
    base::WeakPtr<ui::Widget> window) {
  // Utility
  wgpuSetLogCallback(
      [](WGPULogLevel level, WGPUStringView message, void* userdata) {
        LOG(INFO) << "[WGPU] " << std::string(message.data, message.length);
      },
      nullptr);

  // Instance
  auto instance = wgpu::CreateInstance(nullptr);

  // Swapchain
  wgpu::SurfaceDescriptor surface_desc;
  {
    auto sdl_window_properties = SDL_GetWindowProperties(window->AsSDLWindow());
#if defined(OS_WIN)
    HWND window_handle = reinterpret_cast<HWND>(SDL_GetPointerProperty(
        sdl_window_properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));

    wgpu::SurfaceSourceWindowsHWND win_surface_desc;
    win_surface_desc.hinstance = ::GetModuleHandle(nullptr);
    win_surface_desc.hwnd = window_handle;

    // WGPU-native requires this, i dont know why.
    ::UpdateWindow(window_handle);

    surface_desc.nextInChain = &win_surface_desc;
#else
#error unsupport platform
#endif  // OS_XXX
  }
  auto surface = instance.CreateSurface(&surface_desc);

  // Adapter
  wgpu::Adapter adapter;
  {
    wgpu::RequestAdapterOptions adapter_desc;
    adapter_desc.compatibleSurface = surface;

    WGPURequestAdapterCallbackInfo adapter_callback;
    adapter_callback.userdata1 = &adapter;
    adapter_callback.callback = [](WGPURequestAdapterStatus status,
                                   WGPUAdapter adapter, WGPUStringView message,
                                   void* userdata1, void* userdata2) {
      auto* adapter_out = static_cast<wgpu::Adapter*>(userdata1);
      *adapter_out = wgpu::Adapter::Acquire(adapter);
    };

    instance.RequestAdapter(&adapter_desc, adapter_callback);
  }

  // Device
  wgpu::Device device;
  {
    WGPURequestDeviceCallbackInfo device_callback;
    device_callback.userdata1 = &device;
    device_callback.callback = [](WGPURequestDeviceStatus status,
                                  WGPUDevice device, WGPUStringView message,
                                  void* userdata1, void* userdata2) {
      auto* device_out = static_cast<wgpu::Device*>(userdata1);
      *device_out = wgpu::Device::Acquire(device);
    };

    adapter.RequestDevice(nullptr, device_callback);
  }

  // Queue
  auto queue = device.GetQueue();

  return std::unique_ptr<RenderDevice>(
      new RenderDevice(window, instance, adapter, surface, device, queue));
}

}  // namespace renderer

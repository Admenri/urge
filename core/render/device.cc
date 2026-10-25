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

#include "core/render/device.h"

#include <string>
#include <string_view>

#include "SDL3/SDL_cpuinfo.h"
#include "SDL3/SDL_platform_defines.h"

#if defined(WEBGPU_BACKEND_WGPU)
#include "wgpu.h"
#endif

#if defined(OS_EMSCRIPTEN)
#include <emscripten.h>
#endif

#include "core/common/exception.h"
#include "core/common/logger.h"

namespace urge {

namespace {

#if defined(OS_EMSCRIPTEN)
/* The browser answers RequestAdapter and RequestDevice on its own event loop,
   and no loop is running while this constructor blocks: a single
   ProcessEvents() would arrive before the promise settled almost every time,
   and the engine would go on to use a null adapter.  Yielding with Asyncify is
   what lets the promise settle -- the frame loop does exactly this every frame.
   A browser without WebGPU never answers at all, so the wait is bounded and
   reports the failure instead of hanging. */
constexpr int kGpuWaitFrames = 600;

template <typename Ready>
bool WaitForGpuObject(wgpu::Instance instance, Ready ready) {
  for (int frame = 0; frame < kGpuWaitFrames; ++frame) {
    instance.ProcessEvents();
    if (ready())
      return true;
    emscripten_sleep(0);
  }
  return ready();
}
#endif

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

std::string BackendName(wgpu::BackendType backend) {
  switch (backend) {
    case wgpu::BackendType::Null:
      return "Null";
    case wgpu::BackendType::WebGPU:
      return "WebGPU";
    case wgpu::BackendType::D3D11:
      return "D3D11";
    case wgpu::BackendType::D3D12:
      return "D3D12";
    case wgpu::BackendType::Metal:
      return "Metal";
    case wgpu::BackendType::Vulkan:
      return "Vulkan";
    case wgpu::BackendType::OpenGL:
      return "OpenGL";
    case wgpu::BackendType::OpenGLES:
      return "OpenGL ES";
    default:
      return "Unknown";
  }
}

std::string FromStringView(const wgpu::StringView& view) {
  if (view.data == nullptr)
    return {};
  if (view.length == WGPU_STRLEN)
    return std::string(view.data);
  return std::string(view.data, view.length);
}

}

DeviceInfo QueryDeviceInfo(const wgpu::Adapter& adapter) {
  DeviceInfo info;

  wgpu::AdapterInfo adapter_info;
  adapter.GetInfo(&adapter_info);
  info.adapter_device = FromStringView(adapter_info.device);
  info.adapter_vendor = FromStringView(adapter_info.vendor);
  info.adapter_architecture = FromStringView(adapter_info.architecture);
  info.adapter_description = FromStringView(adapter_info.description);
  info.backend = BackendName(adapter_info.backendType);
  info.vendor_id = adapter_info.vendorID;
  info.device_id = adapter_info.deviceID;

  info.cpu_cores = SDL_GetNumLogicalCPUCores();
  info.system_ram_mb = static_cast<uint64_t>(SDL_GetSystemRAM());

  return info;
}

GPUDevice::GPUDevice(URGE_EXCEPT, SDL_Window* window, std::string backend) {
  wgpu::InstanceDescriptor instance_desc;
#if defined(WEBGPU_BACKEND_WGPU)
  WGPUInstanceExtras instance_extras = {};
  instance_extras.chain.sType =
      static_cast<WGPUSType>(WGPUSType_InstanceExtras);
  instance_extras.flags = WGPUInstanceFlag_Empty;
  instance_desc.nextInChain =
      reinterpret_cast<wgpu::ChainedStruct*>(&instance_extras.chain);
#endif
#if defined(SDL_PLATFORM_EMSCRIPTEN)
  /* `wgpuInstanceWaitAny` returns immediately in the browser unless the
     instance was created with the timed-wait feature, and a read-back that
     returns immediately never runs its mapping callback -- `Bitmap#get_pixel`
     then fails with "the mapping callback was never invoked".  The limit has to
     be non-zero for the feature to be granted. */
  static constexpr wgpu::InstanceFeatureName kTimedWaitAny =
      wgpu::InstanceFeatureName::TimedWaitAny;
  static const wgpu::InstanceLimits kInstanceLimits = [] {
    wgpu::InstanceLimits limits = {};
    limits.timedWaitAnyMaxCount = 8;
    return limits;
  }();
  instance_desc.requiredFeatureCount = 1;
  instance_desc.requiredFeatures = &kTimedWaitAny;
  instance_desc.requiredLimits = &kInstanceLimits;
#endif
  instance_ = wgpu::CreateInstance(&instance_desc);

  if (window != nullptr) {
    wgpu::SurfaceDescriptor surface_desc;
#if defined(SDL_PLATFORM_EMSCRIPTEN)
    /* There is no window handle to hand over in a browser: the surface is the
       HTML canvas, and Emscripten's EGL -- which wgpu's GLES backend renders
       through -- always binds to the module canvas.  The selector has to name
       that same element, which is the `#canvas` the shell hands to the module
       as `Module.canvas`. */
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas_source = {};
    canvas_source.chain.sType = static_cast<WGPUSType>(
        WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector);
    canvas_source.selector = WGPUStringView{"#canvas", WGPU_STRLEN};
    surface_desc.nextInChain =
        reinterpret_cast<wgpu::ChainedStruct*>(&canvas_source.chain);
#elif defined(SDL_PLATFORM_WIN32)
    SDL_PropertiesID window_prop = SDL_GetWindowProperties(window);
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

  CreateDevice(exception_state, backend);
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

void GPUDevice::CreateDevice(URGE_EXCEPT, std::string backend) {
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

                                                                            
                                                                               
                                                                         
                                                                           
                                                                             
                                                             
#if defined(OS_EMSCRIPTEN)
  if (!WaitForGpuObject(instance_, [this] { return adapter_ != nullptr; }))
    exception_state.Throw(ExceptionState::kGPUError, "the browser provided no WebGPU adapter");
    return;#else
  if (!adapter_)
    instance_.ProcessEvents();
#endif

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

#if defined(OS_EMSCRIPTEN)
  if (!WaitForGpuObject(instance_, [this] { return device_ != nullptr; }))
    exception_state.Throw(ExceptionState::kGPUError, "the browser provided no WebGPU device");
    return;#else
  if (!device_)
    instance_.ProcessEvents();
#endif

  wgpu::AdapterInfo adapter_info;
  adapter_.GetInfo(&adapter_info);
  LOGGER_INFO("[GPU] Device: {} ({:#X})", std::string_view(adapter_info.device),
              adapter_info.deviceID);
  LOGGER_INFO("[GPU] Vendor: {} ({:#X})", std::string_view(adapter_info.vendor),
              adapter_info.vendorID);

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

  info_ = QueryDeviceInfo(adapter_);

  queue_ = device_.GetQueue();
}

}

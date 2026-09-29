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

//! Translates a message level of wgpu-native into the level of the engine.
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

//! Whether a block of the validation layer is the present-after-write hazard it
//! raises about every present of the engine, and which nothing the engine does
//! can prevent.
//! \details The swapchain image of a frame is written by a render pass, and the
//! driver transitions it to the layout a present requires at the end of that
//! pass. The layer calls the present a hazard because its model of a present
//! orders the transition an explicit barrier performs and not the one
//! vkCmdEndRenderPass performs, which is the origin the block names. No
//! synchronization is missing: wgpu waits on the semaphore a surface image is
//! acquired with, signals one in the submission that writes the image, and the
//! present waits on that one, so the presentation engine reads the image after
//! the transition. The behaviour is reported to the layer as
//! https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/13117 and its
//! model is stated to omit the ordering in
//! https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/6177; wgpu-hal
//! drops the reports it knows to be wrong in the callback that hands the layer
//! over to the host, so this is the same measure one level up.
//! \remarks Both fragments have to be found. A rewording of either one lets the
//! reports through again, which is what keeps a hazard of another origin from
//! being swallowed.
bool IsRenderPassPresentAfterWrite(std::string_view text) {
  return text.find("SYNC-HAZARD-PRESENT-AFTER-WRITE") !=
             std::string_view::npos &&
         text.find("initiated by vkCmdEndRenderPass") != std::string_view::npos;
}

//! Whether a message is one of the records wgpu-hal appends to a message of the
//! validation layer. The objects of a message are a log record of their own, and
//! they arrive after the block that carries the message.
bool IsValidationContinuation(std::string_view text) {
  static constexpr std::string_view kPrefixes[] = {"\tobjects: ", "\tqueues: ",
                                                   "\tcommand buffers: "};
  for (const std::string_view prefix : kPrefixes) {
    if (text.starts_with(prefix))
      return true;
  }
  return false;
}

//! Whether the records that follow a dropped block are still being dropped.
//! A callback runs on the thread that raised the message, and the block of a
//! message and its objects are raised by one call into the layer, so the state
//! belongs to a thread and not to the process.
thread_local bool g_dropping_validation_continuation = false;

//! Receives the messages of wgpu-native. Without a callback installed the
//! backend drops a validation error, which is what happens to a draw call that
//! is rejected for a reason the engine never gets to know.
//! \remarks The callback may run on the thread that raised the message, and the
//! messages are multi-line blocks of text. The block of a present the engine
//! cannot synchronize is dropped, see IsRenderPassPresentAfterWrite().
void OnWGPULog(WGPULogLevel level, WGPUStringView message, void* /*userdata*/) {
  const LogLevel engine_level = FromWGPULogLevel(level);
  if (engine_level == LogLevel::kOff)
    return;

  // wgpu::StringView resolves the WGPU_STRLEN sentinel, the message of a
  // callback is not necessarily null terminated.
  const std::string_view text = wgpu::StringView(message);

  // The block is dropped before it is formatted, so the measure does not depend
  // on the level the logger of the engine filters at.
  if (IsRenderPassPresentAfterWrite(text)) {
    g_dropping_validation_continuation = true;
    return;
  }

  if (g_dropping_validation_continuation) {
    if (IsValidationContinuation(text))
      return;
    // A message that is not an appendage of a block ends the drop, which bounds
    // it to the records of the block that was dropped.
    g_dropping_validation_continuation = false;
  }

  // The location of the message is inside wgpu-native, a file and a line of the
  // engine would be misleading, so the source is marked instead.
  LogMessage(engine_level, "[wgpu] ", "{}", text);
}

//! Installs the callback and asks for the messages an engine is interested in.
//! The level of wgpu-native is a ceiling of its own, a level that is not
//! requested here can never be raised by SetLogLevel() afterwards.
//! \remarks Debug is deliberately not requested, it is the level the Vulkan
//! loader reports every internal step of the instance creation with, which
//! buries the validation errors this callback exists for.
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
  // The callback is installed before the instance is created so that a message
  // of the instance, the adapter and the device is not lost.
  InstallWGPULogger();

  // Instance
  wgpu::InstanceDescriptor instance_desc;
  instance_desc.requiredFeatureCount = 1;
  wgpu::InstanceFeatureName spirv =
      wgpu::InstanceFeatureName::ShaderSourceSPIRV;
  instance_desc.requiredFeatures = &spirv;
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

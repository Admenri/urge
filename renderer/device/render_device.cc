// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/device/render_device.h"

#include "base/debug/logging.h"
#include "ui/widget.h"

// Include platform-specific or system headers needed for Windows, SDL, WGPU, etc.
// #include ...

namespace renderer {

//--------------------------------------------------------------------------------------
// Internal Helper Functions
//--------------------------------------------------------------------------------------

namespace {

/**
 * Callback that gets triggered when the WGPU device is lost.
 *
 * @param device  The WGPU device instance that triggered this callback.
 * @param reason  The enumerated reason why the device was lost.
 * @param message Additional diagnostic message from the GPU stack or driver.
 */
void OnDeviceLost(const wgpu::Device& /*device*/,
                  wgpu::DeviceLostReason reason,
                  wgpu::StringView message) {
  std::string reason_str;
  switch (reason) {
    case wgpu::DeviceLostReason::Unknown:
      reason_str = "Unknown";
      break;
    case wgpu::DeviceLostReason::Destroyed:
      reason_str = "Destroyed";
      break;
    case wgpu::DeviceLostReason::InstanceDropped:
      reason_str = "InstanceDropped";
      break;
    case wgpu::DeviceLostReason::FailedCreation:
      reason_str = "FailedCreation";
      break;
    // You may want to handle additional enumerations if WGPU adds more
    default:
      reason_str = "Unrecognized Reason";
      break;
  }

  LOG(INFO) << "[Renderer] Device lost - " << reason_str << ": "
            << std::string_view(message);
}

/**
 * Callback for uncaptured WGPU errors such as validation, OOM, or unknown errors.
 *
 * @param device  The WGPU device instance that triggered this callback.
 * @param error_type  The enumerated type of error.
 * @param message     The error's diagnostic message.
 */
void OnUncapturedError(const wgpu::Device& /*device*/,
                       wgpu::ErrorType error_type,
                       wgpu::StringView message) {
  const char* type_str = nullptr;
  switch (error_type) {
    case wgpu::ErrorType::Validation:
      type_str = "Validation";
      break;
    case wgpu::ErrorType::OutOfMemory:
      type_str = "OutOfMemory";
      break;
    case wgpu::ErrorType::Unknown:
      type_str = "Unknown";
      break;
    case wgpu::ErrorType::DeviceLost:
      type_str = "DeviceLost";
      break;
    default:
      type_str = "UnrecognizedErrorType";
      break;
  }

  LOG(INFO) << "[Renderer] " << type_str
            << " error: " << std::string_view(message);
}

}  // namespace

//--------------------------------------------------------------------------------------
// Public Static Methods
//--------------------------------------------------------------------------------------

/**
 * Factory function to create a RenderDevice instance.
 *
 * @param window_target     A weak pointer to the UI widget that represents our main window.
 * @param required_backend  An optional WGPU backend requirement (e.g., Vulkan, Metal, etc.).
 *
 * @return A unique_ptr to the RenderDevice or nullptr on failure.
 */
std::unique_ptr<RenderDevice> RenderDevice::Create(
    base::WeakPtr<ui::Widget> window_target,
    wgpu::BackendType required_backend /* = wgpu::BackendType::Undefined */) {
  // 1) Create a WGPU instance
  wgpu::InstanceDescriptor instance_desc;
  instance_desc.features.timedWaitAnyEnable = true;  // Example of additional features if needed
  wgpu::Instance instance = wgpu::CreateInstance(&instance_desc);
  if (!instance) {
    LOG(ERROR) << "[Renderer] Failed to create WGPU instance.";
    return nullptr;
  }

  // 2) Request an adapter that is "compatibilityMode" = true (for broad compatibility)
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.compatibilityMode = true;
  // If needed: adapter_options.backendType = required_backend; // or other fields

  wgpu::Adapter adapter;
  // We wait synchronously with WaitAny(...) for an adapter to be provided
  instance.WaitAny(
      instance.RequestAdapter(
          &adapter_options, wgpu::CallbackMode::WaitAnyOnly,
          [&](wgpu::RequestAdapterStatus status,
              wgpu::Adapter result_adapter,
              wgpu::StringView message) {
            if (status != wgpu::RequestAdapterStatus::Success) {
              LOG(ERROR) << "[Renderer] Failed to get an adapter: "
                         << std::string_view(message);
              return;
            }
            adapter = std::move(result_adapter);
          }),
      /*timeout=*/UINT64_MAX);

  if (!adapter) {
    LOG(ERROR) << "[Renderer] No valid WGPU adapter available.";
    return nullptr;
  }

  // 3) Log some adapter info
  wgpu::AdapterInfo info;
  adapter.GetInfo(&info);
  LOG(INFO) << "[Renderer] Using adapter device: " << std::string_view(info.device);
  LOG(INFO) << "[Renderer] Description: " << std::string_view(info.description);

  // 4) Request the actual WGPU device
  wgpu::DeviceDescriptor device_desc = {};
  device_desc.SetUncapturedErrorCallback(OnUncapturedError);
  device_desc.SetDeviceLostCallback(wgpu::CallbackMode::AllowSpontaneous, OnDeviceLost);

  wgpu::Device device;
  instance.WaitAny(
      adapter.RequestDevice(
          &device_desc, wgpu::CallbackMode::WaitAnyOnly,
          [&](wgpu::RequestDeviceStatus status,
              wgpu::Device result_device,
              wgpu::StringView message) {
            if (status != wgpu::RequestDeviceStatus::Success) {
              LOG(ERROR) << "[Renderer] Failed to create a WGPU device: "
                         << std::string_view(message);
              return;
            }
            device = std::move(result_device);
          }),
      UINT64_MAX);

  if (!device) {
    LOG(ERROR) << "[Renderer] No valid WGPU device created.";
    return nullptr;
  }

  // 5) Create a WGPU surface from the native window
  SDL_PropertiesID window_properties =
      SDL_GetWindowProperties(window_target->AsSDLWindow());

  wgpu::SurfaceDescriptor surface_desc;
  surface_desc.label = nullptr;

#if defined(OS_WIN)
  wgpu::SurfaceSourceWindowsHWND hwnd_desc;
  hwnd_desc.hinstance = ::GetModuleHandle(nullptr);
  hwnd_desc.hwnd = SDL_GetPointerProperty(window_properties,
                                          SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                          nullptr);
  surface_desc.nextInChain = &hwnd_desc;
#elif defined(OS_LINUX)
  // Not implemented: e.g. wgpu::SurfaceSourceXcb ...
  LOG(ERROR) << "Linux platform not implemented for surface creation. "
             << "Please fill in the wgpu::SurfaceSource* details for your environment.";
  return nullptr;
#elif defined(OS_MACOSX)
  // Not implemented: e.g. wgpu::SurfaceSourceMetalLayer ...
  LOG(ERROR) << "macOS platform not implemented for surface creation. "
             << "Please fill in the wgpu::SurfaceSourceMetalLayer details for your environment.";
  return nullptr;
#else
  #error "Unsupported platform for surface creation."
#endif

  wgpu::Surface surface = instance.CreateSurface(&surface_desc);
  if (!surface) {
    LOG(ERROR) << "[Renderer] Surface creation failed.";
    return nullptr;
  }

  // 6) Retrieve surface capabilities and configure
  wgpu::SurfaceCapabilities caps;
  surface.GetCapabilities(adapter, &caps);
  if (caps.formats.empty()) {
    LOG(ERROR) << "[Renderer] Surface has no supported formats.";
    return nullptr;
  }

  wgpu::SurfaceConfiguration config;
  config.device = device;
  config.format = caps.formats[0];  // Typically choose the first supported format or BGRA8Unorm
  config.width = window_target->GetSize().x;
  config.height = window_target->GetSize().y;

  // Additional config fields can be set here (e.g., usage, presentMode, etc.)
  surface.Configure(&config);

  // 7) Success: create and return our RenderDevice instance
  return std::unique_ptr<RenderDevice>(
      new RenderDevice(window_target, instance, adapter, device, surface));
}

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------

RenderDevice::RenderDevice(base::WeakPtr<ui::Widget> window,
                           const wgpu::Instance& instance,
                           const wgpu::Adapter& adapter,
                           const wgpu::Device& device,
                           const wgpu::Surface& surface)
    : window_(std::move(window)),
      instance_(instance),
      adapter_(adapter),
      device_(device),
      surface_(surface) {
  // Additional initialization if needed
}

//--------------------------------------------------------------------------------------
// (Possible) Additional Methods
//--------------------------------------------------------------------------------------

// e.g. surface resizing, device cleanup, etc. 
// ...

}  // namespace renderer

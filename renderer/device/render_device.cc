// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/device/render_device.h"

#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_loadso.h"
#include "SDL3/SDL_video.h"
#include "magic_enum/magic_enum.hpp"

#include "Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Primitives/interface/DebugOutput.h"

#include "base/debug/logging.h"
#include "ui/widget/widget.h"

#if defined(OS_ANDROID)
#include "Graphics/GraphicsEngineOpenGL/interface/RenderDeviceGLES.h"
#endif

#if defined(OS_LINUX)
using PFN_XGetXCBConnection = void* (*)(void*);

#ifdef SDL_PLATFORM_OPENBSD
#define DEFAULT_VULKAN "libvulkan.so"
#define DEFAULT_X11_XCB "libX11-xcb.so"
#else
#define DEFAULT_VULKAN "libvulkan.so.1"
#define DEFAULT_X11_XCB "libX11-xcb.so.1"
#endif

#endif  // OS_LINUX

namespace renderer {

//--------------------------------------------------------------------------------------
// Internal Helper Functions
//--------------------------------------------------------------------------------------

namespace {

struct HookMemoryAllocator : public Diligent::IMemoryAllocator {
  void* Allocate(size_t Size,
                 const Diligent::Char* dbgDescription,
                 const char* dbgFileName,
                 const Diligent::Int32 dbgLineNumber) override {
    return mi_malloc(Size);
  }

  void Free(void* Ptr) override { mi_free(Ptr); }

  void* AllocateAligned(size_t Size,
                        size_t Alignment,
                        const Diligent::Char* dbgDescription,
                        const char* dbgFileName,
                        const Diligent::Int32 dbgLineNumber) override {
    return mi_malloc_aligned(Size, Alignment);
  }

  void FreeAligned(void* Ptr) override { mi_free(Ptr); }
};

void DILIGENT_CALL_TYPE
DebugMessageOutputFunc(Diligent::DEBUG_MESSAGE_SEVERITY Severity,
                       const Diligent::Char* Message,
                       const Diligent::Char* Function,
                       const Diligent::Char* File,
                       int Line) {
  if (Function)
    LOG(INFO) << "[Renderer] Function " << Function << ":";

  switch (Severity) {
    default:
    case Diligent::DEBUG_MESSAGE_SEVERITY_INFO:
      LOG(INFO) << "[Renderer] " << Message;
      break;
    case Diligent::DEBUG_MESSAGE_SEVERITY_WARNING:
      LOG(WARNING) << "[Renderer] " << Message;
      break;
    case Diligent::DEBUG_MESSAGE_SEVERITY_ERROR:
      LOG(ERROR) << "[Renderer] " << Message;
      break;
    case Diligent::DEBUG_MESSAGE_SEVERITY_FATAL_ERROR:
      LOG(FATAL) << "[Renderer] " << Message;
      break;
  }
}

HookMemoryAllocator g_raw_memory_allocator;

}  // namespace

#if !ENGINE_DLL
using Diligent::GetEngineFactoryD3D11;
using Diligent::GetEngineFactoryD3D12;
using Diligent::GetEngineFactoryOpenGL;
using Diligent::GetEngineFactoryVk;
#endif

RenderDevice::CreateDeviceResult RenderDevice::Create(
    base::WeakPtr<ui::Widget> window_target,
    DriverType driver_type) {
  // Setup debugging output
  Diligent::SetDebugMessageCallback(DebugMessageOutputFunc);

  // Setup native window
  Diligent::NativeWindow native_window;
  SDL_PropertiesID window_properties =
      SDL_GetWindowProperties(window_target->AsSDLWindow());

  // Setup specific platform window handle
  SDL_GLContext glcontext = nullptr;
#if defined(OS_WIN)
  if (driver_type == DRIVER_UNDEFINED)
    driver_type = DRIVER_D3D11;

  native_window.hWnd = SDL_GetPointerProperty(
      window_properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(OS_LINUX)
  if (driver_type == DriverType::UNDEFINED)
    driver_type = DriverType::OPENGL;

  {
    // Xlib Display
    void* xdisplay = SDL_GetPointerProperty(
        window_properties, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
    int64_t xwindow = SDL_GetNumberProperty(
        window_properties, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

    glcontext = SDL_GL_CreateContext(window_target->AsSDLWindow());
    SDL_GL_MakeCurrent(window_target->AsSDLWindow(), glcontext);

    // Get XCBConnect from Xlib
    const char* xcb_library_name = SDL_GetHint(SDL_HINT_X11_XCB_LIBRARY);
    if (!xcb_library_name || !*xcb_library_name)
      xcb_library_name = DEFAULT_X11_XCB;

    SDL_SharedObject* xlib_xcb_library = SDL_LoadObject(xcb_library_name);
    PFN_XGetXCBConnection xgetxcb_func = nullptr;
    if (xlib_xcb_library)
      xgetxcb_func = (PFN_XGetXCBConnection)SDL_LoadFunction(
          xlib_xcb_library, "XGetXCBConnection");

    // Setup native window
    native_window.WindowId = static_cast<uint32_t>(xwindow);
    native_window.pDisplay = xdisplay;
    native_window.pXCBConnection =
        xgetxcb_func ? xgetxcb_func(xdisplay) : nullptr;
  }
#elif defined(OS_ANDROID)
  if (driver_type == DriverType::UNDEFINED)
    driver_type = DriverType::OPENGL;

  native_window.pAWindow = SDL_GetPointerProperty(
      window_properties, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);
#else
#error "unsupport platform selected."
#endif

  Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
  Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
  Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain;

  // Initialize driver descriptor
  Diligent::EngineCreateInfo engine_create_info;
  Diligent::SwapChainDesc swap_chain_desc;
#if D3D11_SUPPORTED || D3D12_SUPPORTED
  Diligent::FullScreenModeDesc fullscreen_mode_desc;
#endif

  // Hook raw memory allocator
  engine_create_info.pRawMemAllocator = &g_raw_memory_allocator;

  // Setup primary swapchain
  swap_chain_desc.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM;
  swap_chain_desc.PreTransform = Diligent::SURFACE_TRANSFORM_OPTIMAL;
  swap_chain_desc.IsPrimary = Diligent::True;

// Initialize specific graphics api
#if GL_SUPPORTED || GLES_SUPPORTED
  if (driver_type == DRIVER_OPENGL) {
#if ENGINE_DLL
    auto GetEngineFactoryOpenGL = Diligent::LoadGraphicsEngineOpenGL();
#endif
    auto* factory = GetEngineFactoryOpenGL();

    Diligent::EngineGLCreateInfo gl_create_info(engine_create_info);
    gl_create_info.Window = native_window;
    gl_create_info.ZeroToOneNDZ = Diligent::True;
    factory->CreateDeviceAndSwapChainGL(gl_create_info, &device, &context,
                                        swap_chain_desc, &swapchain);
  }
#endif  // OPENGL_SUPPORT
#if VULKAN_SUPPORTED
  if (driver_type == DRIVER_VULKAN) {
#if ENGINE_DLL
    auto GetEngineFactoryVk = Diligent::LoadGraphicsEngineVk();
#endif
    auto* factory = GetEngineFactoryVk();

    Diligent::EngineVkCreateInfo vk_create_info(engine_create_info);
    factory->CreateDeviceAndContextsVk(vk_create_info, &device, &context);
    factory->CreateSwapChainVk(device, context, swap_chain_desc, native_window,
                               &swapchain);
  }
#endif  // VULKAN_SUPPORT
#if D3D11_SUPPORTED
  if (driver_type == DRIVER_D3D11) {
#if ENGINE_DLL
    auto GetEngineFactoryD3D11 = Diligent::LoadGraphicsEngineD3D11();
#endif
    auto* pFactory = GetEngineFactoryD3D11();

    Diligent::EngineD3D11CreateInfo d3d11_create_info(engine_create_info);
    pFactory->CreateDeviceAndContextsD3D11(d3d11_create_info, &device,
                                           &context);
    pFactory->CreateSwapChainD3D11(device, context, swap_chain_desc,
                                   fullscreen_mode_desc, native_window,
                                   &swapchain);
  }
#endif  // D3D11_SUPPORT
#if D3D12_SUPPORTED
  if (driver_type == DRIVER_D3D12) {
#if ENGINE_DLL
    auto GetEngineFactoryD3D12 = Diligent::LoadGraphicsEngineD3D12();
#endif
    auto* pFactoryD3D12 = GetEngineFactoryD3D12();

    Diligent::EngineD3D12CreateInfo d3d12_create_info(engine_create_info);
    pFactoryD3D12->CreateDeviceAndContextsD3D12(d3d12_create_info, &device,
                                                &context);
    pFactoryD3D12->CreateSwapChainD3D12(device, context, swap_chain_desc,
                                        fullscreen_mode_desc, native_window,
                                        &swapchain);
  }
#endif  // D3D12_SUPPORT

  // etc
  const auto& device_info = device->GetDeviceInfo();
  const auto& adapter_info = device->GetAdapterInfo();
  LOG(INFO) << "[Renderer] DeviceType: " +
                   base::String(GetRenderDeviceTypeString(device_info.Type)) +
                   " (version "
            << device_info.APIVersion.Major << "."
            << device_info.APIVersion.Minor << ")";
  LOG(INFO) << "[Renderer] Adapter: " << adapter_info.Description;
  LOG(INFO) << "[Renderer] MaxTexture Size: "
            << adapter_info.Texture.MaxTexture2DDimension;

  // Global render device
  base::OwnedPtr<RenderDevice> render_device = base::MakeOwnedPtr<RenderDevice>(
      window_target, swap_chain_desc, device, swapchain, glcontext);

  // Immediate render context
  base::OwnedPtr<RenderContext> render_context =
      base::MakeOwnedPtr<RenderContext>(context);

  return std::make_tuple(std::move(render_device), std::move(render_context));
}

RenderDevice::RenderDevice(
    base::WeakPtr<ui::Widget> window,
    const Diligent::SwapChainDesc& swapchain_desc,
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device,
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain,
    SDL_GLContext gl_context)
    : window_(std::move(window)),
      swapchain_desc_(swapchain_desc),
      device_(device),
      swapchain_(swapchain),
      pipelines_(device_, Diligent::TEX_FORMAT_RGBA8_UNORM),
      quad_index_(QuadIndexCache::Make(device_)),
      device_type_(device_->GetDeviceInfo().Type),
      gl_context_(gl_context) {
  quad_index_.Allocate(1 << 10);
}

RenderDevice::~RenderDevice() {
  if (gl_context_)
    SDL_GL_DestroyContext(gl_context_);
}

void RenderDevice::SuspendContext() {
#if defined(OS_ANDROID)
  switch (device_type_) {
    case Diligent::RENDER_DEVICE_TYPE_GLES: {
      Diligent::RefCntAutoPtr<Diligent::IRenderDeviceGLES> es_device(
          device_, Diligent::IID_RenderDeviceGLES);
      es_device->Suspend();
    } break;
#if VULKAN_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_VULKAN:
      swapchain_.Release();
      break;
#endif  // VULKAN_SUPPORTED
    default:
      break;
  }
#endif  // OS_ANDROID
}

int32_t RenderDevice::ResumeContext(
    Diligent::IDeviceContext* immediate_context) {
#if defined(OS_ANDROID)
  SDL_PropertiesID window_properties =
      SDL_GetWindowProperties(window_->AsSDLWindow());
  void* android_native_window = SDL_GetPointerProperty(
      window_properties, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr);

  switch (device_type_) {
    case Diligent::RENDER_DEVICE_TYPE_GLES: {
      Diligent::RefCntAutoPtr<Diligent::IRenderDeviceGLES> es_device(
          device_, Diligent::IID_RenderDeviceGLES);
      return es_device->Resume(
          static_cast<ANativeWindow*>(android_native_window));
    }
#if VULKAN_SUPPORTED
    case Diligent::RENDER_DEVICE_TYPE_VULKAN: {
      device_->IdleGPU();

      Diligent::NativeWindow native_window;
      native_window.pAWindow = android_native_window;

#if ENGINE_DLL
      auto GetEngineFactoryVk = Diligent::LoadGraphicsEngineVk();
#endif
      auto* factory = GetEngineFactoryVk();
      factory->CreateSwapChainVk(device_, immediate_context, swapchain_desc_,
                                 native_window, &swapchain_);

      return swapchain_ ? EGL_SUCCESS : EGL_NOT_INITIALIZED;
    }
#endif  // VULKAN_SUPPORTED
    default:
      break;
  }

  return EGL_NOT_INITIALIZED;
#else
  return 0;
#endif  // OS_ANDROID
}

}  // namespace renderer

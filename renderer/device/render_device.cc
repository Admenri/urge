// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/device/render_device.h"

#undef ENGINE_DLL

#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h"
#include "Graphics/GraphicsEngineOpenGL/interface/EngineFactoryOpenGL.h"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include "Primitives/interface/DebugOutput.h"

namespace renderer {

void DILIGENT_CALL_TYPE
DebugMessageOutputFunc(enum DEBUG_MESSAGE_SEVERITY Severity,
                       const Char* Message,
                       const Char* Function,
                       const Char* File,
                       int Line) {
  if (Severity >= DEBUG_MESSAGE_SEVERITY::DEBUG_MESSAGE_SEVERITY_ERROR)
    printf("Function: %s, Info: %s", Function, Message);
}

RenderDevice::~RenderDevice() {}

std::unique_ptr<RenderDevice> RenderDevice::Create(
    base::WeakPtr<ui::Widget> render_window,
    RendererBackend backend) {
  // Set debug output
  SetDebugMessageCallback(DebugMessageOutputFunc);

  // Set window target
  SDL_PropertiesID win_prop =
      SDL_GetWindowProperties(render_window->AsSDLWindow());
  void* win_handle = SDL_GetPointerProperty(
      win_prop, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);

  RefCntAutoPtr<IRenderDevice> device;
  RefCntAutoPtr<IDeviceContext> context;
  RefCntAutoPtr<ISwapChain> swapchain;

  SwapChainDesc SCDesc;
  if (backend == RendererBackend::kOpenGL) {
#if ENGINE_DLL
    auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#endif
    auto* factory = GetEngineFactoryOpenGL();
    EngineGLCreateInfo EngineCI;
    EngineCI.Window.hWnd = win_handle;
    factory->CreateDeviceAndSwapChainGL(EngineCI, &device, &context, SCDesc,
                                        &swapchain);
  } else if (backend == RendererBackend::kD3D12) {
#if ENGINE_DLL
    auto GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
#endif
    EngineD3D12CreateInfo EngineCI;
    auto* pFactoryD3D12 = GetEngineFactoryD3D12();
    pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &device, &context);
    Win32NativeWindow Window{win_handle};
    pFactoryD3D12->CreateSwapChainD3D12(
        device, context, SCDesc, FullScreenModeDesc{}, Window, &swapchain);
  } else if (backend == RendererBackend::kD3D11) {
#if ENGINE_DLL
    auto GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#endif
    EngineD3D11CreateInfo EngineCI;
    auto* pFactory = GetEngineFactoryD3D11();
    pFactory->CreateDeviceAndContextsD3D11(EngineCI, &device, &context);
    Win32NativeWindow Window{win_handle};
    pFactory->CreateSwapChainD3D11(device, context, SCDesc,
                                   FullScreenModeDesc{}, Window, &swapchain);
  } else if (backend == RendererBackend::kVulkan) {
#if ENGINE_DLL
    auto GetEngineFactoryVk = LoadGraphicsEngineVk();
#endif
    EngineVkCreateInfo EngineCI;
    auto* pFactory = GetEngineFactoryVk();
    pFactory->CreateDeviceAndContextsVk(EngineCI, &device, &context);
    Win32NativeWindow Window{win_handle};
    pFactory->CreateSwapChainVk(device, context, SCDesc, Window, &swapchain);
  }

  std::unique_ptr<RenderDevice> self(new RenderDevice);
  self->render_window_ = render_window;
  self->device_ = device;
  self->context_ = context;
  self->swapchain_ = swapchain;

  self->quad_index_buffer_ = new QuadArrayIndices(device);
  self->quad_index_buffer_->EnsureSize(context, 2 << 10);
  self->common_quad_.reset(new QuadDrawable(device, self->quad_index_buffer_));

  context->WaitForIdle();

  return self;
}

void RenderDevice::InitializePipelines(TEXTURE_FORMAT render_target_format) {
  pipelines_.reset(new PipelineStorage(device_, render_target_format));
  context_->WaitForIdle();
}

RefCntAutoPtr<ITexture> RenderDevice::MakeGenericFramebuffer(
    const base::Vec2i& size,
    TEXTURE_FORMAT texfmt) {
  if (!generic_framebuffer_ || generic_framebuffer_->GetDesc().Width < size.x ||
      generic_framebuffer_->GetDesc().Height < size.y ||
      generic_framebuffer_->GetDesc().Format != texfmt) {
    TextureDesc TexDesc;
    TexDesc.Name = "Global generic texture";
    TexDesc.Type = RESOURCE_DIM_TEX_2D;
    TexDesc.Format = texfmt;
    TexDesc.Usage = USAGE_DEFAULT;
    TexDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    TexDesc.Width = std::max<uint32_t>(
        size.x,
        generic_framebuffer_ ? generic_framebuffer_->GetDesc().Width : 0);
    TexDesc.Height = std::max<uint32_t>(
        size.y,
        generic_framebuffer_ ? generic_framebuffer_->GetDesc().Height : 0);

    generic_framebuffer_.Release();
    device()->CreateTexture(TexDesc, nullptr, &generic_framebuffer_);
  }

  return generic_framebuffer_;
}

void CopyTexture(IDeviceContext* context,
                 ITexture* src,
                 const base::Rect& src_region,
                 ITexture* dst,
                 const base::Vec2i& dst_pos) {
  base::Rect tmp_src = src_region;
  base::Rect tex_size(0, 0, src->GetDesc().Width, src->GetDesc().Height);

  auto result_src = base::MakeIntersect(tmp_src, tex_size);

  if (!result_src.width || !result_src.height)
    return;

  Diligent::Box SrcBox(result_src.x, result_src.x + result_src.width,
                       result_src.y, result_src.y + result_src.height);
  Diligent::CopyTextureAttribs CopyTexAttr(
      src, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION, dst,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  CopyTexAttr.pSrcBox = &SrcBox;
  CopyTexAttr.DstX = dst_pos.x;
  CopyTexAttr.DstY = dst_pos.y;
  context->CopyTexture(CopyTexAttr);
}

void MakeProjectionMatrix(float* out,
                          const base::Vec2& size,
                          bool origin_bottom) {
  const float aa = 2.0f / size.x;
  const float bb = (origin_bottom ? 2.0f : -2.0f) / size.y;
  const float cc = origin_bottom ? 2.0f : 1.0f;
  const float dd = -1.0f;
  const float ee = origin_bottom ? -1.0f : 1.0f;
  const float ff = origin_bottom ? -1.0f : 0.0f;

  memset(out, 0, sizeof(float) * 16);
  out[0] = aa;
  out[3] = dd;
  out[5] = bb;
  out[7] = ee;
  out[10] = cc;
  out[11] = ff;
  out[15] = 1.0f;
}

}  // namespace renderer

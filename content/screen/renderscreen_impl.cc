// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/screen/renderscreen_impl.h"

#include <unordered_map>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"
#include "magic_enum/magic_enum.hpp"

#include "Common/interface/BasicMath.hpp"
#include "Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp"
#if defined(OS_ANDROID)
#include "Graphics/GraphicsEngineOpenGL/interface/RenderDeviceGLES.h"
#endif

#include "content/canvas/canvas_scheduler.h"
#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"
#include "content/gpu/buffer_impl.h"
#include "content/gpu/device_context_impl.h"
#include "content/gpu/render_device_impl.h"
#include "content/profile/command_ids.h"
#include "renderer/utils/texture_utils.h"

namespace content {

///////////////////////////////////////////////////////////////////////////////
// RenderScreenImpl Implement

RenderScreenImpl::RenderScreenImpl(ExecutionContext* execution_context,
                                   uint32_t frame_rate)
    : EngineObject(execution_context),
      limiter_(frame_rate),
      frozen_(false),
      brightness_(255),
      frame_count_(0),
      frame_rate_(frame_rate) {
  // Setup render device on render thread if possible
  GPUCreateGraphicsHostInternal();

  // Initialize fps limiter
  limiter_.Reset();
}

RenderScreenImpl::~RenderScreenImpl() = default;

void RenderScreenImpl::PresentScreenBuffer(
    Diligent::ImGuiDiligentRenderer* gui_renderer) {
  // Present to screen surface
  GPUPresentScreenBufferInternal(context()->primary_render_context,
                                 gui_renderer);
}

void RenderScreenImpl::ResetFPSCounter() {
  limiter_.Reset();
}

void RenderScreenImpl::CreateButtonGUISettings() {
  const auto& adapter_info = (*context()->render_device)->GetAdapterInfo();
  const auto& device_info = (*context()->render_device)->GetDeviceInfo();
  auto& settings_profile = *context()->engine_profile;

  if (ImGui::CollapsingHeader(
          context()
              ->i18n_profile->GetI18NString(IDS_SETTINGS_GRAPHICS, "Graphics")
              .c_str())) {
    ImGui::TextWrapped(
        "%s: %s (%d.%d)",
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_BACKEND, "Backend")
            .c_str(),
        Diligent::GetRenderDeviceTypeString(device_info.Type),
        device_info.APIVersion.Major, device_info.APIVersion.Minor);
    ImGui::Separator();
    ImGui::TextWrapped(
        "%s: %s (PCI: %X)",
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_RENDERER, "Renderer")
            .c_str(),
        adapter_info.Description, adapter_info.DeviceId);
    ImGui::Separator();
    ImGui::TextWrapped(
        "%s: %s (PCI: %X)",
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_VENDOR, "Vendor")
            .c_str(),
        magic_enum::enum_name(adapter_info.Vendor).data() +
            sizeof("ADAPTER_VENDOR"),
        adapter_info.VendorId);
    ImGui::Separator();

    // Keep Ratio
    ImGui::Checkbox(
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_KEEP_RATIO, "Keep Ratio")
            .c_str(),
        &settings_profile.keep_ratio);

    // Skip Frame
    ImGui::Checkbox(
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_SKIP_FRAME, "Skip Frame")
            .c_str(),
        &settings_profile.allow_skip_frame);

    // Fullscreen
    bool is_fullscreen = context()->window->IsFullscreen(),
         last_fullscreen = is_fullscreen;
    ImGui::Checkbox(
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_FULLSCREEN, "Fullscreen")
            .c_str(),
        &is_fullscreen);
    if (last_fullscreen != is_fullscreen)
      context()->window->SetFullscreen(is_fullscreen);

    // Background running
    ImGui::Checkbox(context()
                        ->i18n_profile
                        ->GetI18NString(IDS_GRAPHICS_BACKGROUND_RUNNING,
                                        "Background Running")
                        .c_str(),
                    &settings_profile.background_running);
  }
}

void RenderScreenImpl::Update(ExceptionState& exception_state) {
  const bool frozen_render = frozen_;
  const bool need_skip_frame = limiter_.RequireFrameSkip() &&
                               context()->engine_profile->allow_skip_frame;

  if (!frozen_render && !need_skip_frame) {
    // Render a frame and push into render queue
    // This function only encodes the render commands
    RenderFrameInternal(gpu_.screen_buffer, gpu_.screen_depth_stencil);
  }

  if (need_skip_frame)
    limiter_.Reset();

  // Process frame delay
  // This calling will yield to event coroutine and present
  FrameProcessInternal(gpu_.screen_buffer);
}

void RenderScreenImpl::Wait(uint32_t duration,
                            ExceptionState& exception_state) {
  for (uint32_t i = 0; i < duration; ++i)
    Update(exception_state);
}

void RenderScreenImpl::FadeOut(uint32_t duration,
                               ExceptionState& exception_state) {
  duration = std::max(duration, 1u);

  float current_brightness = static_cast<float>(brightness_);
  for (uint32_t i = 0; i < duration; ++i) {
    brightness_ = current_brightness -
                  current_brightness * (i / static_cast<float>(duration));
    if (frozen_) {
      FrameProcessInternal(gpu_.frozen_buffer);
    } else {
      Update(exception_state);
    }
  }

  // Set final brightness
  brightness_ = 0;
  Update(exception_state);
}

void RenderScreenImpl::FadeIn(uint32_t duration,
                              ExceptionState& exception_state) {
  duration = std::max(duration, 1u);

  float current_brightness = static_cast<float>(brightness_);
  float diff = 255.0f - current_brightness;
  for (uint32_t i = 0; i < duration; ++i) {
    brightness_ =
        current_brightness + diff * (i / static_cast<float>(duration));

    if (frozen_) {
      FrameProcessInternal(gpu_.frozen_buffer);
    } else {
      Update(exception_state);
    }
  }

  // Set final brightness
  brightness_ = 255;
  Update(exception_state);
}

void RenderScreenImpl::Freeze(ExceptionState& exception_state) {
  if (!frozen_) {
    // Get frozen scene snapshot for transition
    RenderFrameInternal(gpu_.frozen_buffer, gpu_.frozen_depth_stencil);

    // Set forzen flag for blocking frame update
    frozen_ = true;
  }
}

void RenderScreenImpl::Transition(ExceptionState& exception_state) {
  Transition(10, std::string(), 40, exception_state);
}

void RenderScreenImpl::Transition(uint32_t duration,
                                  ExceptionState& exception_state) {
  Transition(duration, std::string(), 40, exception_state);
}

void RenderScreenImpl::Transition(uint32_t duration,
                                  const std::string& filename,
                                  ExceptionState& exception_state) {
  Transition(duration, filename, 40, exception_state);
}

void RenderScreenImpl::Transition(uint32_t duration,
                                  const std::string& filename,
                                  uint32_t vague,
                                  ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> transition_mapping = nullptr;
  if (!filename.empty())
    transition_mapping =
        CanvasImpl::Create(context(), filename, exception_state);
  if (exception_state.HadException())
    return;

  TransitionWithBitmap(duration, transition_mapping, vague, exception_state);
}

void RenderScreenImpl::TransitionWithBitmap(uint32_t duration,
                                            scoped_refptr<Bitmap> bitmap,
                                            uint32_t vague,
                                            ExceptionState& exception_state) {
  if (!frozen_)
    return;

  if (duration > 0) {
    // Reset brightness
    brightness_ = 255;
    // Transition attribute
    const float vague_norm = std::clamp<uint32_t>(vague, 1, 256) / 256.0f;

    // Derive transition mapping if available
    scoped_refptr<CanvasImpl> mapping_bitmap = CanvasImpl::FromBitmap(bitmap);
    BitmapTexture* texture_agent =
        Disposable::IsValid(mapping_bitmap.get()) ? **mapping_bitmap : nullptr;
    Diligent::ITextureView* transition_mapping =
        texture_agent ? texture_agent->shader_resource_view.RawPtr() : nullptr;

    // Get current scene snapshot for transition
    auto* render_context = context()->primary_render_context;
    RenderFrameInternal(gpu_.transition_buffer, gpu_.transition_depth_stencil);

    // Transition render loop
    for (uint32_t i = 0; i < duration; ++i) {
      // Norm transition progress
      float progress = i * (1.0f / duration);

      // Render per transition frame
      if (transition_mapping)
        GPURenderVagueTransitionFrameInternal(
            render_context, transition_mapping, progress, vague_norm);
      else
        GPURenderAlphaTransitionFrameInternal(render_context, progress);

      // Present to screen
      FrameProcessInternal(gpu_.screen_buffer);
    }
  } else {
    // Update frame as common
    Update(exception_state);
  }

  // Transition process complete
  frozen_ = false;
}

scoped_refptr<Bitmap> RenderScreenImpl::SnapToBitmap(
    ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> target =
      CanvasImpl::Create(context(), context()->resolution, exception_state);
  BitmapTexture* texture_agent =
      Disposable::IsValid(target.get()) ? **target : nullptr;

  if (texture_agent) {
    // Invalidate render target bitmap cache
    target->InvalidateSurfaceCache();

    // Execute rendering
    RenderFrameInternal(texture_agent->data, texture_agent->depth);
  }

  return target;
}

void RenderScreenImpl::FrameReset(ExceptionState& exception_state) {
  limiter_.Reset();
}

uint32_t RenderScreenImpl::Width(ExceptionState& exception_state) {
  return context()->resolution.x;
}

uint32_t RenderScreenImpl::Height(ExceptionState& exception_state) {
  return context()->resolution.y;
}

void RenderScreenImpl::ResizeScreen(uint32_t width,
                                    uint32_t height,
                                    ExceptionState& exception_state) {
  context()->resolution = base::Vec2i(width, height);
  GPUResetScreenBufferInternal();
}

void RenderScreenImpl::Reset(ExceptionState& exception_state) {
  /* Reset freeze */
  frozen_ = false;

  /* Reset attribute */
  frame_rate_ = context()->engine_profile->api_version ==
                        ContentProfile::APIVersion::RGSS1
                    ? 40
                    : 60;
  brightness_ = 255;
  FrameReset(exception_state);
}

void RenderScreenImpl::MoveWindow(int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height,
                                  ExceptionState& exception_state) {
  auto* window = context()->window->AsSDLWindow();
  SDL_SetWindowSize(window, width, height);
  SDL_SetWindowPosition(window, x, y);
}

scoped_refptr<Rect> RenderScreenImpl::GetWindowRect(
    ExceptionState& exception_state) {
  auto* window = context()->window->AsSDLWindow();
  base::Rect window_rect;
  SDL_GetWindowPosition(window, &window_rect.x, &window_rect.y);
  SDL_GetWindowSize(window, &window_rect.width, &window_rect.height);
  return base::MakeRefCounted<RectImpl>(window_rect);
}

uint32_t RenderScreenImpl::GetDisplayID(ExceptionState& exception_state) {
  return SDL_GetDisplayForWindow(context()->window->AsSDLWindow());
}

void RenderScreenImpl::SetWindowIcon(scoped_refptr<Bitmap> icon,
                                     ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> data = CanvasImpl::FromBitmap(icon);
  SDL_Surface* icon_surface = data->RequireMemorySurface();
  SDL_SetWindowIcon(context()->window->AsSDLWindow(), icon_surface);
}

int32_t RenderScreenImpl::GetMaxTextureSize(ExceptionState& exception_state) {
  return context()->render_device->MaxTextureSize();
}

scoped_refptr<GPURenderDevice> RenderScreenImpl::GetRenderDevice(
    ExceptionState& exception_state) {
  return base::MakeRefCounted<RenderDeviceImpl>(context(),
                                                **context()->render_device);
}

scoped_refptr<GPUDeviceContext> RenderScreenImpl::GetImmediateContext(
    ExceptionState& exception_state) {
  return base::MakeRefCounted<DeviceContextImpl>(
      context(), context()->primary_render_context);
}

scoped_refptr<GPUBuffer> RenderScreenImpl::GetGenericQuadIndexBuffer(
    uint32_t draw_quad_count,
    ExceptionState& exception_state) {
  auto& quad_index = context()->render.quad_index;
  quad_index->Allocate(draw_quad_count);
  return base::MakeRefCounted<BufferImpl>(context(), **quad_index);
}

GPU::ValueType RenderScreenImpl::GetInternalIndexType(
    ExceptionState& exception_state) {
  auto& quad_index = context()->render.quad_index;
  return static_cast<GPU::ValueType>(quad_index->GetIndexType());
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    FrameRate,
    uint32_t,
    RenderScreenImpl,
    { return frame_rate_; },
    {
      frame_rate_ = value;
      limiter_.SetFrameRate(frame_rate_);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    FrameCount,
    uint32_t,
    RenderScreenImpl,
    { return frame_count_; },
    { frame_count_ = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Brightness,
    uint32_t,
    RenderScreenImpl,
    { return brightness_; },
    { brightness_ = std::clamp<uint32_t>(value, 0, 255); });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    VSync,
    uint32_t,
    RenderScreenImpl,
    { return context()->engine_profile->vsync; },
    {
      context()->engine_profile->vsync = std::clamp<uint32_t>(value, 0, 255);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Fullscreen,
    bool,
    RenderScreenImpl,
    { return context()->window->IsFullscreen(); },
    { context()->window->SetFullscreen(value); });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Skipframe,
    bool,
    RenderScreenImpl,
    { return context()->engine_profile->allow_skip_frame; },
    { context()->engine_profile->allow_skip_frame = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    KeepRatio,
    bool,
    RenderScreenImpl,
    { return context()->engine_profile->keep_ratio; },
    { context()->engine_profile->keep_ratio = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    BackgroundRunning,
    bool,
    RenderScreenImpl,
    { return context()->engine_profile->background_running; },
    { context()->engine_profile->background_running = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Ox,
    int32_t,
    RenderScreenImpl,
    { return origin_.x; },
    {
      origin_.x = value;
      GPUUpdateScreenWorldInternal();
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Oy,
    int32_t,
    RenderScreenImpl,
    { return origin_.y; },
    {
      origin_.y = value;
      GPUUpdateScreenWorldInternal();
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    WindowTitle,
    std::string,
    RenderScreenImpl,
    { return SDL_GetWindowTitle(context()->window->AsSDLWindow()); },
    { SDL_SetWindowTitle(context()->window->AsSDLWindow(), value.c_str()); });

void RenderScreenImpl::FrameProcessInternal(
    Diligent::ITexture* present_target) {
  // Increase frame render count
  ++frame_count_;

  // Determine wait delay time
  limiter_.Delay();

  // Tick callback
  frame_tick_handler_.Run(present_target);
}

void RenderScreenImpl::RenderFrameInternal(Diligent::ITexture* render_target,
                                           Diligent::ITexture* depth_stencil) {
  // Submit pending canvas commands
  context()->canvas_scheduler->SubmitPendingPaintCommands();

  // Setup viewport
  controller_.CurrentViewport().bound = context()->resolution;
  controller_.CurrentViewport().origin = origin_;

  // Prepare for rendering context
  DrawableNode::RenderControllerParams controller_params;
  controller_params.context = context()->primary_render_context;
  controller_params.screen_buffer = render_target;
  controller_params.screen_depth_stencil = depth_stencil;
  controller_params.screen_size = context()->resolution;

  // 1) Execute pre-composite handler
  controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                    &controller_params);

  // 1.5) Update sprite batch data
  context()->sprite_batcher->SubmitBatchDataAndResetCache(
      context()->render.quad_index.get(), controller_params.context);

  // 2) Setup renderpass
  GPUFrameBeginRenderPassInternal(controller_params.context, render_target,
                                  depth_stencil);

  // 3) Notify render a frame
  ScissorStack scissor_stack(controller_params.context, context()->resolution);
  controller_params.root_world = gpu_.root_transform;
  controller_params.world_binding = gpu_.world_transform;
  controller_params.scissors = &scissor_stack;
  controller_.BroadCastNotification(DrawableNode::ON_RENDERING,
                                    &controller_params);

  // 4) End render pass and process after-render effect
  GPUFrameEndRenderPassInternal(controller_params.context);
}

void RenderScreenImpl::GPUCreateGraphicsHostInternal() {
  // Create generic quads batch
  gpu_.transition_quads = renderer::QuadBatch::Make(**context()->render_device);
  gpu_.effect_quads = renderer::QuadBatch::Make(**context()->render_device);

  // Create generic shader binding
  gpu_.transition_binding_alpha =
      context()->render.pipeline_loader->alphatrans.CreateBinding();
  gpu_.transition_binding_vague =
      context()->render.pipeline_loader->mappedtrans.CreateBinding();
  gpu_.effect_binding =
      context()->render.pipeline_loader->color.CreateBinding();

  // Create screen buffer
  GPUResetScreenBufferInternal();
}

void RenderScreenImpl::GPUUpdateScreenWorldInternal() {
  renderer::WorldTransform world_transform;
  renderer::MakeProjectionMatrix(world_transform.projection,
                                 context()->resolution.Recast<float>());
  renderer::MakeTransformMatrix(world_transform.transform,
                                base::Vec2(-origin_.x, -origin_.y));

  gpu_.world_transform.Release();
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(world_transform),
      "graphics.world.transform", &gpu_.world_transform,
      Diligent::USAGE_IMMUTABLE, Diligent::BIND_UNIFORM_BUFFER,
      Diligent::CPU_ACCESS_NONE, &world_transform);
}

void RenderScreenImpl::GPUResetScreenBufferInternal() {
  // Reset original buffer
  gpu_.screen_buffer.Release();
  gpu_.screen_depth_stencil.Release();
  gpu_.frozen_buffer.Release();
  gpu_.frozen_depth_stencil.Release();
  gpu_.transition_buffer.Release();
  gpu_.transition_depth_stencil.Release();

  // Color attachment
  renderer::CreateTexture2D(
      **context()->render_device, &gpu_.screen_buffer, "screen.main.buffer",
      context()->resolution, Diligent::USAGE_DEFAULT,
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE);
  renderer::CreateTexture2D(
      **context()->render_device, &gpu_.frozen_buffer, "screen.frozen.buffer",
      context()->resolution, Diligent::USAGE_DEFAULT,
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE);
  renderer::CreateTexture2D(
      **context()->render_device, &gpu_.transition_buffer,
      "screen.transition.buffer", context()->resolution,
      Diligent::USAGE_DEFAULT,
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE);

  // Depth stencil
  renderer::CreateTexture2D(
      **context()->render_device, &gpu_.screen_depth_stencil,
      "screen.main.depth_stencil", context()->resolution,
      Diligent::USAGE_DEFAULT, Diligent::BIND_DEPTH_STENCIL,
      Diligent::CPU_ACCESS_NONE, Diligent::TEX_FORMAT_D24_UNORM_S8_UINT);
  renderer::CreateTexture2D(
      **context()->render_device, &gpu_.frozen_depth_stencil,
      "screen.frozen.depth_stencil", context()->resolution,
      Diligent::USAGE_DEFAULT, Diligent::BIND_DEPTH_STENCIL,
      Diligent::CPU_ACCESS_NONE, Diligent::TEX_FORMAT_D24_UNORM_S8_UINT);
  renderer::CreateTexture2D(
      **context()->render_device, &gpu_.transition_depth_stencil,
      "screen.transition.depth_stencil", context()->resolution,
      Diligent::USAGE_DEFAULT, Diligent::BIND_DEPTH_STENCIL,
      Diligent::CPU_ACCESS_NONE, Diligent::TEX_FORMAT_D24_UNORM_S8_UINT);

  // Root transform
  renderer::WorldTransform world_transform;
  renderer::MakeProjectionMatrix(world_transform.projection,
                                 context()->resolution.Recast<float>());
  renderer::MakeIdentityMatrix(world_transform.transform);

  gpu_.root_transform.Release();
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(world_transform),
      "graphics.root.transform", &gpu_.root_transform,
      Diligent::USAGE_IMMUTABLE, Diligent::BIND_UNIFORM_BUFFER,
      Diligent::CPU_ACCESS_NONE, &world_transform);

  // Offset transform
  GPUUpdateScreenWorldInternal();
}

void RenderScreenImpl::GPUPresentScreenBufferInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::ImGuiDiligentRenderer* gui_renderer) {
  // Initial swapchain attribute
  Diligent::ISwapChain* swapchain = context()->render_device->GetSwapChain();
  auto* render_target_view = swapchain->GetCurrentBackBufferRTV();
  auto* depth_stencil_view = swapchain->GetDepthBufferDSV();

  // Prepare for rendering
  float clear_color[] = {0, 0, 0, 1};
  render_context->SetRenderTargets(
      1, &render_target_view, depth_stencil_view,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply present scissor
  Diligent::Rect present_scissor(0, 0, swapchain->GetDesc().Width,
                                 swapchain->GetDesc().Height);
  render_context->SetScissorRects(1, &present_scissor, UINT32_MAX, UINT32_MAX);

  // Render GUI and present
  gui_renderer->RenderDrawData(render_context, ImGui::GetDrawData());

  // Flush command buffer and present GPU surface
  swapchain->Present(context()->engine_profile->vsync);
}

void RenderScreenImpl::GPUFrameBeginRenderPassInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::ITexture* render_target,
    Diligent::ITexture* depth_stencil) {
  // Setup screen effect params
  if (brightness_ < 255) {
    renderer::Quad effect_quad;
    renderer::Quad::SetPositionRect(&effect_quad,
                                    base::Rect(context()->resolution));
    renderer::Quad::SetColor(&effect_quad,
                             base::Vec4(0, 0, 0, (255 - brightness_) / 255.0f));
    gpu_.effect_quads.QueueWrite(render_context, &effect_quad);
  }

  // Setup render pass
  auto* render_target_view =
      render_target->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  auto* depth_stencil_view =
      depth_stencil->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
  render_context->SetRenderTargets(
      1, &render_target_view, depth_stencil_view,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->ClearDepthStencil(
      depth_stencil_view, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Push scissor
  Diligent::Rect render_scissor(0, 0, render_target->GetDesc().Width,
                                render_target->GetDesc().Height);
  render_context->SetScissorRects(1, &render_scissor, UINT32_MAX, UINT32_MAX);
}

void RenderScreenImpl::GPUFrameEndRenderPassInternal(
    Diligent::IDeviceContext* render_context) {
  // Render screen effect if need
  if (brightness_ < 255) {
    // Apply brightness effect
    auto* pipeline = context()->render.pipeline_states->brightness.RawPtr();

    // Set world transform
    gpu_.effect_binding.u_transform->Set(gpu_.world_transform);

    // Apply pipeline state
    render_context->SetPipelineState(pipeline);
    render_context->CommitShaderResources(
        *gpu_.effect_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *gpu_.effect_quads;
    render_context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    render_context->SetIndexBuffer(
        **context()->render.quad_index, 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType =
        context()->render.quad_index->GetIndexType();
    render_context->DrawIndexed(draw_indexed_attribs);
  }
}

void RenderScreenImpl::GPURenderAlphaTransitionFrameInternal(
    Diligent::IDeviceContext* render_context,
    float progress) {
  // Update transition uniform
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(
      &transient_quad, base::RectF(base::Vec2(0), base::Vec2(1)));
  renderer::Quad::SetColor(&transient_quad, base::Vec4(progress));
  gpu_.transition_quads.QueueWrite(render_context, &transient_quad);

  // Composite transition frame
  auto render_target_view =
      gpu_.screen_buffer->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  render_context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Derive pipeline sets
  auto* pipeline = context()->render.pipeline_states->alpha_transition.RawPtr();

  // Set uniform texture
  gpu_.transition_binding_alpha.u_current_texture->Set(
      gpu_.transition_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  gpu_.transition_binding_alpha.u_frozen_texture->Set(
      gpu_.frozen_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *gpu_.transition_binding_alpha,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *gpu_.transition_quads;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render.quad_index, 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = context()->render.quad_index->GetIndexType();
  render_context->DrawIndexed(draw_indexed_attribs);
}

void RenderScreenImpl::GPURenderVagueTransitionFrameInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::ITextureView* trans_mapping,
    float progress,
    float vague) {
  // Update transition uniform
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(
      &transient_quad, base::RectF(base::Vec2(0), base::Vec2(1)));
  renderer::Quad::SetColor(&transient_quad, base::Vec4(vague, 0, 0, progress));
  gpu_.transition_quads.QueueWrite(render_context, &transient_quad);

  // Composite transition frame
  auto render_target_view =
      gpu_.screen_buffer->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  render_context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float clear_color[] = {0, 0, 0, 1};
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Derive pipeline sets
  auto* pipeline = context()->render.pipeline_states->vague_transition.RawPtr();

  // Set uniform texture
  gpu_.transition_binding_vague.u_current_texture->Set(
      gpu_.transition_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  gpu_.transition_binding_vague.u_frozen_texture->Set(
      gpu_.frozen_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  gpu_.transition_binding_vague.u_trans_texture->Set(trans_mapping);

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *gpu_.transition_binding_vague,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *gpu_.transition_quads;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render.quad_index, 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = context()->render.quad_index->GetIndexType();
  render_context->DrawIndexed(draw_indexed_attribs);
}

}  // namespace content

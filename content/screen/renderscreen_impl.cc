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
#include "content/gpu/buffer_impl.h"
#include "content/gpu/device_context_impl.h"
#include "content/gpu/render_device_impl.h"
#include "content/profile/command_ids.h"
#include "renderer/utils/texture_utils.h"

namespace content {

namespace {

Diligent::float4x4 MakePresentationTransform(
    Diligent::SURFACE_TRANSFORM pre_transform) {
  switch (pre_transform) {
    case Diligent::SURFACE_TRANSFORM_IDENTITY:
      // Nothing to do
      return Diligent::float4x4::Identity();
    case Diligent::SURFACE_TRANSFORM_ROTATE_90:
      // The image content is rotated 90 degrees clockwise.
      return Diligent::float4x4::RotationZ(-Diligent::PI_F * 0.5f);
    case Diligent::SURFACE_TRANSFORM_ROTATE_180:
      // The image content is rotated 180 degrees clockwise.
      return Diligent::float4x4::RotationZ(-Diligent::PI_F * 1.0f);
    case Diligent::SURFACE_TRANSFORM_ROTATE_270:
      // The image content is rotated 270 degrees clockwise.
      return Diligent::float4x4::RotationZ(-Diligent::PI_F * 1.5f);
    case Diligent::SURFACE_TRANSFORM_OPTIMAL:
      UNEXPECTED(
          "SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during "
          "swap chain initialization.");
      break;
    case Diligent::SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
    case Diligent::SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
    case Diligent::SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
    case Diligent::SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
      UNEXPECTED("Mirror transforms are not supported");
      break;
    default:
      UNEXPECTED("Unknown transform");
  }

  return Diligent::float4x4::Identity();
}

}  // namespace

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

  // Initialize viewport
  UpdateWindowViewportInternal();

  // Initialize fps limiter
  limiter_.Reset();
}

RenderScreenImpl::~RenderScreenImpl() = default;

void RenderScreenImpl::PresentScreenBuffer(
    Diligent::ImGuiDiligentRenderer* gui_renderer) {
  // Determine wait delay time
  limiter_.Delay();

  // Update drawing viewport
  UpdateWindowViewportInternal();

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
        "%s: %s",
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_RENDERER, "Renderer")
            .c_str(),
        Diligent::GetRenderDeviceTypeString(device_info.Type));
    ImGui::Separator();
    ImGui::TextWrapped(
        "%s: %s",
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_VENDOR, "Vendor")
            .c_str(),
        magic_enum::enum_name(adapter_info.Vendor).data());
    ImGui::Separator();
    ImGui::TextWrapped(
        "%s: %s",
        context()
            ->i18n_profile
            ->GetI18NString(IDS_GRAPHICS_DESCRIPTION, "Description")
            .c_str(),
        adapter_info.Description);
    ImGui::Separator();

    // Keep Ratio
    ImGui::Checkbox(
        context()
            ->i18n_profile->GetI18NString(IDS_GRAPHICS_KEEP_RATIO, "Keep Ratio")
            .c_str(),
        &settings_profile.keep_ratio);

    // Smooth Scale
    ImGui::Checkbox(
        context()
            ->i18n_profile
            ->GetI18NString(IDS_GRAPHICS_SMOOTH_SCALE, "Smooth Scale")
            .c_str(),
        &settings_profile.smooth_scale);

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

void RenderScreenImpl::AddTickObserver(const base::RepeatingClosure& handler) {
  tick_observers_.AddUnsafe(handler);
}

void RenderScreenImpl::AddDisposable(Disposable* disp) {
  disposable_elements_.Append(disp);
}

void RenderScreenImpl::Update(ExceptionState& exception_state) {
  const bool frozen_render = frozen_;
  const bool need_skip_frame = limiter_.RequireFrameSkip() &&
                               context()->engine_profile->allow_skip_frame;

  if (!frozen_render && !need_skip_frame) {
    // Render a frame and push into render queue
    // This function only encodes the render commands
    RenderFrameInternal(agent_.screen_buffer, agent_.screen_depth_stencil);
  }

  if (need_skip_frame)
    limiter_.Reset();

  // Process frame delay
  // This calling will yield to event coroutine and present
  FrameProcessInternal(agent_.screen_buffer);
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
      FrameProcessInternal(agent_.frozen_buffer);
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
      FrameProcessInternal(agent_.frozen_buffer);
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
    RenderFrameInternal(agent_.frozen_buffer, agent_.frozen_depth_stencil);

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
    BitmapAgent* texture_agent =
        mapping_bitmap ? mapping_bitmap->GetAgent() : nullptr;
    Diligent::ITextureView* transition_mapping =
        texture_agent ? texture_agent->resource : nullptr;

    // Get current scene snapshot for transition
    auto* render_context = context()->primary_render_context;
    RenderFrameInternal(agent_.transition_buffer,
                        agent_.transition_depth_stencil);

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
      FrameProcessInternal(agent_.screen_buffer);
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
  BitmapAgent* texture_agent = target ? target->GetAgent() : nullptr;

  if (texture_agent) {
    // Invalidate render target bitmap cache
    target->InvalidateSurfaceCache();

    // Execute rendering
    RenderFrameInternal(texture_agent->data, texture_agent->depth_stencil);
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

  /* Disposed all elements */
  for (auto it = disposable_elements_.tail(); it != disposable_elements_.end();
       it = it->previous()) {
    it->value()->Dispose();
  }

  /* Reset attribute */
  frame_rate_ = context()->engine_profile->api_version ==
                        ContentProfile::APIVersion::RGSS1
                    ? 40
                    : 60;
  brightness_ = 255;
  FrameReset(exception_state);
}

void RenderScreenImpl::PlayMovie(const std::string& filename,
                                 ExceptionState& exception_state) {
  exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                             "Unimplement: Graphics.play_movie");
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
  auto* quad_index = context()->render_device->GetQuadIndex();
  quad_index->Allocate(draw_quad_count);
  return base::MakeRefCounted<BufferImpl>(context(), **quad_index);
}

uint32_t RenderScreenImpl::Get_FrameRate(ExceptionState& exception_state) {
  return frame_rate_;
}

void RenderScreenImpl::Put_FrameRate(const uint32_t& rate,
                                     ExceptionState& exception_state) {
  frame_rate_ = rate;
  limiter_.SetFrameRate(frame_rate_);
}

uint32_t RenderScreenImpl::Get_FrameCount(ExceptionState& exception_state) {
  return frame_count_;
}

void RenderScreenImpl::Put_FrameCount(const uint32_t& count,
                                      ExceptionState& exception_state) {
  frame_count_ = count;
}

uint32_t RenderScreenImpl::Get_Brightness(ExceptionState& exception_state) {
  return brightness_;
}

void RenderScreenImpl::Put_Brightness(const uint32_t& value,
                                      ExceptionState& exception_state) {
  brightness_ = std::clamp<uint32_t>(value, 0, 255);
}

uint32_t RenderScreenImpl::Get_VSync(ExceptionState& exception_state) {
  return context()->engine_profile->vsync;
}

void RenderScreenImpl::Put_VSync(const uint32_t& value,
                                 ExceptionState& exception_state) {
  context()->engine_profile->vsync = std::clamp<uint32_t>(value, 0, 255);
}

bool RenderScreenImpl::Get_Fullscreen(ExceptionState& exception_state) {
  return context()->window->IsFullscreen();
}

void RenderScreenImpl::Put_Fullscreen(const bool& value,
                                      ExceptionState& exception_state) {
  context()->window->SetFullscreen(value);
}

bool RenderScreenImpl::Get_Skipframe(ExceptionState& exception_state) {
  return context()->engine_profile->allow_skip_frame;
}

void RenderScreenImpl::Put_Skipframe(const bool& value,
                                     ExceptionState& exception_state) {
  context()->engine_profile->allow_skip_frame = value;
}

bool RenderScreenImpl::Get_KeepRatio(ExceptionState& exception_state) {
  return context()->engine_profile->keep_ratio;
}

void RenderScreenImpl::Put_KeepRatio(const bool& value,
                                     ExceptionState& exception_state) {
  context()->engine_profile->keep_ratio = value;
}

bool RenderScreenImpl::Get_SmoothScale(ExceptionState& exception_state) {
  return context()->engine_profile->smooth_scale;
}

void RenderScreenImpl::Put_SmoothScale(const bool& value,
                                       ExceptionState& exception_state) {
  context()->engine_profile->smooth_scale = value;
}

bool RenderScreenImpl::Get_BackgroundRunning(ExceptionState& exception_state) {
  return context()->engine_profile->background_running;
}

void RenderScreenImpl::Put_BackgroundRunning(const bool& value,
                                             ExceptionState& exception_state) {
  context()->engine_profile->background_running = value;
}

int32_t RenderScreenImpl::Get_Ox(ExceptionState& exception_state) {
  return origin_.x;
}

void RenderScreenImpl::Put_Ox(const int32_t& value,
                              ExceptionState& exception_state) {
  origin_.x = value;
  GPUUpdateScreenWorldInternal();
}

int32_t RenderScreenImpl::Get_Oy(ExceptionState& exception_state) {
  return origin_.y;
}

void RenderScreenImpl::Put_Oy(const int32_t& value,
                              ExceptionState& exception_state) {
  origin_.y = value;
  GPUUpdateScreenWorldInternal();
}

std::string RenderScreenImpl::Get_WindowTitle(
    ExceptionState& exception_state) {
  return SDL_GetWindowTitle(context()->window->AsSDLWindow());
}

void RenderScreenImpl::Put_WindowTitle(const std::string& value,
                                       ExceptionState& exception_state) {
  SDL_SetWindowTitle(context()->window->AsSDLWindow(), value.c_str());
}

void RenderScreenImpl::FrameProcessInternal(
    Diligent::ITexture* present_target) {
  // Setup target
  agent_.present_target = present_target;

  // Increase frame render count
  ++frame_count_;

  // Tick callback
  tick_observers_.Notify();
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
      controller_params.context);

  // 2) Setup renderpass
  GPUFrameBeginRenderPassInternal(controller_params.context, render_target,
                                  depth_stencil);

  // 3) Notify render a frame
  ScissorStack scissor_stack(controller_params.context, context()->resolution);
  controller_params.root_world = agent_.root_transform;
  controller_params.world_binding = agent_.world_transform;
  controller_params.scissors = &scissor_stack;
  controller_.BroadCastNotification(DrawableNode::ON_RENDERING,
                                    &controller_params);

  // 4) End render pass and process after-render effect
  GPUFrameEndRenderPassInternal(controller_params.context);
}

void RenderScreenImpl::UpdateWindowViewportInternal() {
  auto window_size = context()->window->GetSize();

  if (!(window_size == window_size_)) {
    window_size_ = window_size;

    // Resize screen surface
    auto* swapchain = context()->render_device->GetSwapChain();
    swapchain->Resize(window_size_.x, window_size_.y);
  }

  // Update real display viewport
  float window_ratio = static_cast<float>(window_size.x) / window_size.y;
  float screen_ratio =
      static_cast<float>(context()->resolution.x) / context()->resolution.y;

  display_viewport_.width = window_size.x;
  display_viewport_.height = window_size.y;

  if (screen_ratio > window_ratio)
    display_viewport_.height = display_viewport_.width / screen_ratio;
  else if (screen_ratio < window_ratio)
    display_viewport_.width = display_viewport_.height * screen_ratio;

  display_viewport_.x = (window_size.x - display_viewport_.width) / 2.0f;
  display_viewport_.y = (window_size.y - display_viewport_.height) / 2.0f;

  // Process mouse coordinate and viewport rect
  base::WeakPtr<ui::Widget> window = context()->render_device->GetWindow();
  window->GetDisplayState().scale = display_viewport_.Size().Recast<float>() /
                                    context()->resolution.Recast<float>();
  window->GetDisplayState().viewport = display_viewport_;
}

void RenderScreenImpl::GPUCreateGraphicsHostInternal() {
  // Get pipeline manager
  auto* pipelines = context()->render_device->GetPipelines();

  // Create generic quads batch
  agent_.transition_quads =
      renderer::QuadBatch::Make(**context()->render_device);
  agent_.effect_quads = renderer::QuadBatch::Make(**context()->render_device);

  // Create generic shader binding
  agent_.transition_binding_alpha = pipelines->alphatrans.CreateBinding();
  agent_.transition_binding_vague = pipelines->mappedtrans.CreateBinding();
  agent_.effect_binding = pipelines->color.CreateBinding();

  // If the swap chain color buffer format is a non-sRGB UNORM format,
  // we need to manually convert pixel shader output to gamma space.
  auto* swapchain = context()->render_device->GetSwapChain();
  const auto& swapchain_desc = swapchain->GetDesc();
  const auto srgb_framebuffer =
      Diligent::GetTextureFormatAttribs(swapchain_desc.ColorBufferFormat)
          .ComponentType == Diligent::COMPONENT_TYPE_UNORM_SRGB;

  // Create screen present pipeline
  renderer::PipelineInitParams pipeline_init_params;
  pipeline_init_params.device = **context()->render_device;
  pipeline_init_params.target_format = swapchain_desc.ColorBufferFormat;
  pipeline_init_params.depth_stencil_format = swapchain_desc.DepthBufferFormat;

  agent_.present_pipeline = std::make_unique<renderer::Pipeline_Present>(
      pipeline_init_params, srgb_framebuffer);
  agent_.present_quad =
      renderer::DynamicQuadBatch::Make(**context()->render_device);
  agent_.present_binding = agent_.present_pipeline->CreateBinding();

  // Create window screen transform
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(renderer::WorldTransform),
      "present.world.uniform", &agent_.present_world, Diligent::USAGE_DYNAMIC,
      Diligent::BIND_UNIFORM_BUFFER, Diligent::CPU_ACCESS_WRITE);

  // Create screen buffer
  GPUResetScreenBufferInternal();
}

void RenderScreenImpl::GPUUpdateScreenWorldInternal() {
  renderer::WorldTransform world_transform;
  renderer::MakeProjectionMatrix(world_transform.projection,
                                 context()->resolution.Recast<float>());
  renderer::MakeTransformMatrix(world_transform.transform,
                                base::Vec2(-origin_.x, -origin_.y));

  agent_.world_transform.Release();
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(world_transform),
      "graphics.world.transform", &agent_.world_transform,
      Diligent::USAGE_IMMUTABLE, Diligent::BIND_UNIFORM_BUFFER,
      Diligent::CPU_ACCESS_NONE, &world_transform);
}

void RenderScreenImpl::GPUResetScreenBufferInternal() {
  constexpr Diligent::BIND_FLAGS bind_flags =
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;

  agent_.screen_buffer.Release();
  agent_.screen_depth_stencil.Release();
  agent_.frozen_buffer.Release();
  agent_.frozen_depth_stencil.Release();
  agent_.transition_buffer.Release();
  agent_.transition_depth_stencil.Release();

  // Color attachment
  renderer::CreateTexture2D(**context()->render_device, &agent_.screen_buffer,
                            "screen.main.buffer", context()->resolution,
                            Diligent::USAGE_DEFAULT, bind_flags);
  renderer::CreateTexture2D(**context()->render_device, &agent_.frozen_buffer,
                            "screen.frozen.buffer", context()->resolution,
                            Diligent::USAGE_DEFAULT, bind_flags);
  renderer::CreateTexture2D(**context()->render_device,
                            &agent_.transition_buffer,
                            "screen.transition.buffer", context()->resolution,
                            Diligent::USAGE_DEFAULT, bind_flags);

  // Depth stencil
  renderer::CreateTexture2D(
      **context()->render_device, &agent_.screen_depth_stencil,
      "screen.main.depth_stencil", context()->resolution,
      Diligent::USAGE_DEFAULT, Diligent::BIND_DEPTH_STENCIL,
      Diligent::CPU_ACCESS_NONE, Diligent::TEX_FORMAT_D24_UNORM_S8_UINT);
  renderer::CreateTexture2D(
      **context()->render_device, &agent_.frozen_depth_stencil,
      "screen.frozen.depth_stencil", context()->resolution,
      Diligent::USAGE_DEFAULT, Diligent::BIND_DEPTH_STENCIL,
      Diligent::CPU_ACCESS_NONE, Diligent::TEX_FORMAT_D24_UNORM_S8_UINT);
  renderer::CreateTexture2D(
      **context()->render_device, &agent_.transition_depth_stencil,
      "screen.transition.depth_stencil", context()->resolution,
      Diligent::USAGE_DEFAULT, Diligent::BIND_DEPTH_STENCIL,
      Diligent::CPU_ACCESS_NONE, Diligent::TEX_FORMAT_D24_UNORM_S8_UINT);

  renderer::WorldTransform world_transform;
  renderer::MakeProjectionMatrix(world_transform.projection,
                                 context()->resolution.Recast<float>());
  renderer::MakeIdentityMatrix(world_transform.transform);

  agent_.root_transform.Release();
  Diligent::CreateUniformBuffer(
      **context()->render_device, sizeof(world_transform),
      "graphics.root.transform", &agent_.root_transform,
      Diligent::USAGE_IMMUTABLE, Diligent::BIND_UNIFORM_BUFFER,
      Diligent::CPU_ACCESS_NONE, &world_transform);

  GPUUpdateScreenWorldInternal();
}

void RenderScreenImpl::GPUPresentScreenBufferInternal(
    Diligent::IDeviceContext* render_context,
    Diligent::ImGuiDiligentRenderer* gui_renderer) {
  // Initial device attribute
  Diligent::ISwapChain* swapchain = context()->render_device->GetSwapChain();

  // Setup render params
  auto* render_target_view = swapchain->GetCurrentBackBufferRTV();
  auto* depth_stencil_view = swapchain->GetDepthBufferDSV();
  base::Vec2i screen_size(swapchain->GetDesc().Width,
                          swapchain->GetDesc().Height);

  auto& pipeline_set = *agent_.present_pipeline;
  auto* pipeline =
      pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND, true);
  RRefPtr<Diligent::ISampler> present_sampler;

  if (agent_.present_target) {
    // Update vertex
    renderer::Quad transient_quad;
    renderer::Quad::SetPositionRect(&transient_quad, display_viewport_);
    renderer::Quad::SetTexCoordRectNorm(
        &transient_quad, base::RectF(base::Vec2(0), base::Vec2(1)));
    agent_.present_quad.QueueWrite(render_context, &transient_quad);

    // Update uniform
    {
      void* buffer_mapping;
      render_context->MapBuffer(agent_.present_world, Diligent::MAP_WRITE,
                                Diligent::MAP_FLAG_DISCARD, buffer_mapping);

      auto projection_size = screen_size;
      auto pre_transform = swapchain->GetDesc().PreTransform;
      if (pre_transform == Diligent::SURFACE_TRANSFORM_ROTATE_90 ||
          pre_transform == Diligent::SURFACE_TRANSFORM_ROTATE_270)
        std::swap(projection_size.x, projection_size.y);

      auto* world_matrix =
          static_cast<renderer::WorldTransform*>(buffer_mapping);
      renderer::MakeProjectionMatrix(world_matrix->projection,
                                     projection_size.Recast<float>());

      auto surface_transform = MakePresentationTransform(pre_transform);
      std::memcpy(&world_matrix->transform, &surface_transform,
                  sizeof(surface_transform));

      render_context->UnmapBuffer(agent_.present_world, Diligent::MAP_WRITE);
    }

    // Create sampler
    Diligent::SamplerDesc sampler_desc;
    sampler_desc.Name = "present.sampler";
    sampler_desc.MinFilter = context()->engine_profile->smooth_scale
                                 ? Diligent::FILTER_TYPE_LINEAR
                                 : Diligent::FILTER_TYPE_POINT;
    sampler_desc.MagFilter = sampler_desc.MinFilter;
    sampler_desc.MipFilter = sampler_desc.MinFilter;
    (*context()->render_device)->CreateSampler(sampler_desc, &present_sampler);
  }

  // Update gui device objects if need
  if (gui_renderer) {
    gui_renderer->CheckDeviceObjects();
  }

  // Prepare for rendering
  float clear_color[] = {0, 0, 0, 1};
  render_context->SetRenderTargets(
      1, &render_target_view, depth_stencil_view,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->ClearDepthStencil(
      depth_stencil_view, Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply present scissor
  Diligent::Rect present_scissor(0, 0, screen_size.x, screen_size.y);
  render_context->SetScissorRects(1, &present_scissor, UINT32_MAX, UINT32_MAX);

  // Start screen render
  if (agent_.present_target) {
    auto* render_source = agent_.present_target->GetDefaultView(
        Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    render_source->SetSampler(present_sampler);

    // Set present uniform
    agent_.present_binding.u_transform->Set(agent_.present_world);
    agent_.present_binding.u_texture->Set(render_source);

    // Apply pipeline state
    render_context->SetPipelineState(pipeline);
    render_context->CommitShaderResources(
        *agent_.present_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.present_quad;
    render_context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    render_context->SetIndexBuffer(
        **context()->render_device->GetQuadIndex(), 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    render_context->DrawIndexed(draw_indexed_attribs);
  }

  // Render GUI if need
  if (gui_renderer)
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
    agent_.effect_quads.QueueWrite(render_context, &effect_quad);
  }

  // Setup render pass
  auto* render_target_view =
      render_target->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  auto* depth_stencil_view =
      depth_stencil->GetDefaultView(Diligent::TEXTURE_VIEW_DEPTH_STENCIL);
  render_context->SetRenderTargets(
      1, &render_target_view, depth_stencil_view,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  const float clear_color[] = {0, 0, 0, 1.0f};
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
    auto& pipeline_set = context()->render_device->GetPipelines()->color;
    auto* pipeline =
        pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL, true);

    // Set world transform
    agent_.effect_binding.u_transform->Set(agent_.world_transform);

    // Apply pipeline state
    render_context->SetPipelineState(pipeline);
    render_context->CommitShaderResources(
        *agent_.effect_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = *agent_.effect_quads;
    render_context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    render_context->SetIndexBuffer(
        **context()->render_device->GetQuadIndex(), 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
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
  agent_.transition_quads.QueueWrite(render_context, &transient_quad);

  // Composite transition frame
  auto render_target_view = agent_.screen_buffer->GetDefaultView(
      Diligent::TEXTURE_VIEW_RENDER_TARGET);
  render_context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float clear_color[] = {0, 0, 0, 1};
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  base::Vec2i resolution(agent_.screen_buffer->GetDesc().Width,
                         agent_.screen_buffer->GetDesc().Height);

  Diligent::Rect render_scissor(0, 0, resolution.x, resolution.y);
  render_context->SetScissorRects(1, &render_scissor, UINT32_MAX, UINT32_MAX);

  // Derive pipeline sets
  auto& pipeline_set = context()->render_device->GetPipelines()->alphatrans;
  auto* pipeline =
      pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND, false);

  // Set uniform texture
  agent_.transition_binding_alpha.u_current_texture->Set(
      agent_.transition_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.transition_binding_alpha.u_frozen_texture->Set(
      agent_.frozen_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *agent_.transition_binding_alpha,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.transition_quads;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render_device->GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
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
  agent_.transition_quads.QueueWrite(render_context, &transient_quad);

  // Composite transition frame
  auto render_target_view = agent_.screen_buffer->GetDefaultView(
      Diligent::TEXTURE_VIEW_RENDER_TARGET);
  render_context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  const float clear_color[] = {0, 0, 0, 1};
  render_context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  base::Vec2i resolution(agent_.screen_buffer->GetDesc().Width,
                         agent_.screen_buffer->GetDesc().Height);

  Diligent::Rect render_scissor(0, 0, resolution.x, resolution.y);
  render_context->SetScissorRects(1, &render_scissor, UINT32_MAX, UINT32_MAX);

  // Derive pipeline sets
  auto& pipeline_set = context()->render_device->GetPipelines()->mappedtrans;
  auto* pipeline =
      pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND, false);

  // Set uniform texture
  agent_.transition_binding_vague.u_current_texture->Set(
      agent_.transition_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.transition_binding_vague.u_frozen_texture->Set(
      agent_.frozen_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  agent_.transition_binding_vague.u_trans_texture->Set(trans_mapping);

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *agent_.transition_binding_vague,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *agent_.transition_quads;
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **context()->render_device->GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  render_context->DrawIndexed(draw_indexed_attribs);
}

}  // namespace content

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/screen/renderscreen_impl.h"

#include <unordered_map>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"
#include "magic_enum/magic_enum.hpp"

#include "Graphics/GraphicsAccessories/interface/GraphicsAccessories.hpp"

#if defined(OS_ANDROID)
#include "Graphics/GraphicsEngineOpenGL/interface/RenderDeviceGLES.h"
#endif

#include "content/canvas/canvas_scheduler.h"
#include "content/common/rect_impl.h"
#include "content/profile/command_ids.h"
#include "renderer/utils/texture_utils.h"

namespace content {

namespace {

void GPUUpdateScreenWorldInternal(RenderGraphicsAgent* agent,
                                  const base::Vec2i& resolution,
                                  const base::Vec2i& offset) {
  renderer::WorldTransform world_transform;
  renderer::MakeProjectionMatrix(world_transform.projection, resolution);
  renderer::MakeTransformMatrix(world_transform.transform, resolution, offset,
                                agent->device->IsUVFlip());

  agent->world_transform.Release();
  Diligent::CreateUniformBuffer(
      **agent->device, sizeof(world_transform), "graphics.world.transform",
      &agent->world_transform, Diligent::USAGE_IMMUTABLE,
      Diligent::BIND_UNIFORM_BUFFER, Diligent::CPU_ACCESS_NONE,
      &world_transform);
}

void GPUResetScreenBufferInternal(RenderGraphicsAgent* agent,
                                  const base::Vec2i& resolution,
                                  const base::Vec2i& offset) {
  constexpr Diligent::BIND_FLAGS bind_flags =
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;

  agent->screen_buffer.Release();
  agent->frozen_buffer.Release();
  agent->transition_buffer.Release();

  renderer::CreateTexture2D(**agent->device, &agent->screen_buffer,
                            "screen.main.buffer", resolution,
                            Diligent::USAGE_DEFAULT, bind_flags);
  renderer::CreateTexture2D(**agent->device, &agent->frozen_buffer,
                            "screen.frozen.buffer", resolution,
                            Diligent::USAGE_DEFAULT, bind_flags);
  renderer::CreateTexture2D(**agent->device, &agent->transition_buffer,
                            "screen.transition.buffer", resolution,
                            Diligent::USAGE_DEFAULT, bind_flags);

  renderer::WorldTransform world_transform;
  renderer::MakeProjectionMatrix(world_transform.projection, resolution);
  renderer::MakeIdentityMatrix(world_transform.transform,
                               agent->device->IsUVFlip());

  agent->root_transform.Release();
  Diligent::CreateUniformBuffer(
      **agent->device, sizeof(world_transform), "graphics.root.transform",
      &agent->root_transform, Diligent::USAGE_IMMUTABLE,
      Diligent::BIND_UNIFORM_BUFFER, Diligent::CPU_ACCESS_NONE,
      &world_transform);

  GPUUpdateScreenWorldInternal(agent, resolution, offset);
}

void GPUCreateGraphicsHostInternal(RenderGraphicsAgent* agent,
                                   base::WeakPtr<ui::Widget> window,
                                   ContentProfile* profile,
                                   base::ThreadWorker* render_worker,
                                   filesystem::IOService* io_service,
                                   const base::Vec2i& resolution) {
  // Create primary device on window widget
  auto [render_device, render_context] = renderer::RenderDevice::Create(
      window,
      magic_enum::enum_cast<renderer::DriverType>(profile->driver_backend)
          .value_or(renderer::DriverType::UNDEFINED));
  agent->device = std::move(render_device);
  agent->context = std::move(render_context);

  // Create global canvas scheduler
  agent->canvas_scheduler = base::MakeOwnedPtr<CanvasScheduler>(
      render_worker, agent->device.get(), agent->context.get(), io_service);

  // Create global sprite batch scheduler
  if (agent->device->GetPipelines()->sprite.storage_buffer_support)
    agent->sprite_batch = base::MakeOwnedPtr<SpriteBatch>(agent->device.get());

  // Get pipeline manager
  auto* pipelines = agent->device->GetPipelines();

  // Create generic quads batch
  agent->transition_quads = renderer::QuadBatch::Make(**agent->device);
  agent->effect_quads = renderer::QuadBatch::Make(**agent->device);

  // Create generic shader binding
  agent->transition_binding_alpha = pipelines->alphatrans.CreateBinding();
  agent->transition_binding_vague = pipelines->mappedtrans.CreateBinding();
  agent->effect_binding = pipelines->color.CreateBinding();

  // If the swap chain color buffer format is a non-sRGB UNORM format,
  // we need to manually convert pixel shader output to gamma space.
  auto* swapchain = agent->device->GetSwapChain();
  const auto& swapchain_desc = swapchain->GetDesc();
  const auto srgb_framebuffer =
      Diligent::GetTextureFormatAttribs(swapchain_desc.ColorBufferFormat)
          .ComponentType == Diligent::COMPONENT_TYPE_UNORM_SRGB;

  // Create screen present pipeline
  agent->present_pipeline = base::MakeOwnedPtr<renderer::Pipeline_Present>(
      **agent->device, swapchain_desc.ColorBufferFormat, srgb_framebuffer);

  // Get renderer info
  Diligent::GraphicsAdapterInfo adapter_info =
      (*agent->device)->GetAdapterInfo();
  agent->renderer_info.device = Diligent::GetRenderDeviceTypeString(
      (*agent->device)->GetDeviceInfo().Type);
  agent->renderer_info.vendor = magic_enum::enum_name(
      Diligent::VendorIdToAdapterVendor(adapter_info.VendorId));
  agent->renderer_info.description = adapter_info.Description;

  struct {
    base::String device;
    base::String vendor;
    base::String description;
  } renderer_info_;

  // Create screen buffer
  GPUResetScreenBufferInternal(agent, resolution, base::Vec2i());
}

void GPUDestroyGraphicsHostInternal(RenderGraphicsAgent* agent) {
  delete agent;
}

void GPUPresentScreenBufferInternal(
    RenderGraphicsAgent* agent,
    const base::Rect& display_viewport,
    const base::Vec2i& resolution,
    Diligent::ImGuiDiligentRenderer* gui_renderer,
    bool smooth,
    uint32_t vsync) {
  // Initial device attribute
  auto& context = *agent->context;
  Diligent::ISwapChain* swapchain = agent->device->GetSwapChain();

  // Setup render params
  auto* render_target_view = swapchain->GetCurrentBackBufferRTV();
  base::Vec2i screen_size(swapchain->GetDesc().Width,
                          swapchain->GetDesc().Height);

  auto& pipeline_set = *agent->present_pipeline;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);
  base::OwnedPtr<renderer::QuadBatch> present_quads =
      renderer::QuadBatch::Make(**agent->device);
  base::OwnedPtr<renderer::Binding_Base> present_binding =
      pipeline_set.CreateBinding();
  RRefPtr<Diligent::IBuffer> present_uniform;
  RRefPtr<Diligent::ISampler> present_sampler;

  if (agent->present_target) {
    // Update vertex
    renderer::Quad transient_quad;
    renderer::Quad::SetPositionRect(&transient_quad, display_viewport);
    renderer::Quad::SetTexCoordRectNorm(
        &transient_quad, agent->device->IsUVFlip() ? base::Rect(0, 1, 1, -1)
                                                   : base::Rect(0, 0, 1, 1));
    present_quads->QueueWrite(*context, &transient_quad);

    // Update window screen transform
    renderer::WorldTransform world_matrix;
    renderer::MakeProjectionMatrix(world_matrix.projection, screen_size);
    renderer::MakeIdentityMatrix(world_matrix.transform,
                                 agent->device->IsUVFlip());

    Diligent::CreateUniformBuffer(**agent->device, sizeof(world_matrix),
                                  "present.world.uniform", &present_uniform,
                                  Diligent::USAGE_IMMUTABLE,
                                  Diligent::BIND_UNIFORM_BUFFER,
                                  Diligent::CPU_ACCESS_NONE, &world_matrix);

    // Create sampler
    Diligent::SamplerDesc sampler_desc;
    sampler_desc.Name = "present.sampler";
    sampler_desc.MinFilter =
        smooth ? Diligent::FILTER_TYPE_LINEAR : Diligent::FILTER_TYPE_POINT;
    sampler_desc.MagFilter = sampler_desc.MinFilter;
    sampler_desc.MipFilter = sampler_desc.MinFilter;
    (*agent->device)->CreateSampler(sampler_desc, &present_sampler);
  }

  // Update gui device objects if need
  if (gui_renderer) {
    gui_renderer->CheckDeviceObjects();
  }

  // Prepare for rendering
  float clear_color[] = {0, 0, 0, 1};
  context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply present scissor
  agent->context->ScissorState()->Apply(screen_size);

  // Start screen render
  if (agent->present_target) {
    auto* render_source =
        (*agent->present_target)
            ->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    render_source->SetSampler(present_sampler);

    // Set present uniform
    present_binding->u_transform->Set(present_uniform);
    present_binding->u_texture->Set(render_source);

    // Apply pipeline state
    context->SetPipelineState(pipeline);
    context->CommitShaderResources(
        **present_binding, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = **present_quads;
    context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->SetIndexBuffer(
        **agent->device->GetQuadIndex(), 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    context->DrawIndexed(draw_indexed_attribs);
  }

  // Render GUI if need
  if (gui_renderer)
    gui_renderer->RenderDrawData(*context, ImGui::GetDrawData());

  // Flush command buffer and present GPU surface
  swapchain->Present(vsync);
}

void GPUFrameBeginRenderPassInternal(RenderGraphicsAgent* agent,
                                     Diligent::ITexture** render_target,
                                     const base::Vec2i& resolution,
                                     uint32_t brightness) {
  auto& context = *agent->context;

  // Setup screen effect params
  if (brightness < 255) {
    renderer::Quad effect_quad;
    renderer::Quad::SetPositionRect(&effect_quad, base::Rect(resolution));
    renderer::Quad::SetColor(&effect_quad,
                             base::Vec4(0, 0, 0, (255 - brightness) / 255.0f));
    agent->effect_quads->QueueWrite(*context, &effect_quad);
  }

  // Setup render pass
  auto render_target_view =
      (*render_target)->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  const float clear_color[] = {0, 0, 0, 1};
  context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Push scissor
  agent->context->ScissorState()->Push(resolution);
}

void GPUFrameEndRenderPassInternal(RenderGraphicsAgent* agent,
                                   uint32_t brightness) {
  auto& context = *agent->context;

  // Render screen effect if need
  if (brightness < 255) {
    // Apply brightness effect
    auto& pipeline_set = agent->device->GetPipelines()->color;
    auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NORMAL);

    // Set world transform
    agent->effect_binding->u_transform->Set(agent->world_transform);

    // Apply pipeline state
    context->SetPipelineState(pipeline);
    context->CommitShaderResources(
        **agent->effect_binding,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Apply vertex index
    Diligent::IBuffer* const vertex_buffer = **agent->effect_quads;
    context->SetVertexBuffers(
        0, 1, &vertex_buffer, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->SetIndexBuffer(
        **agent->device->GetQuadIndex(), 0,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Execute render command
    Diligent::DrawIndexedAttribs draw_indexed_attribs;
    draw_indexed_attribs.NumIndices = 6;
    draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
    context->DrawIndexed(draw_indexed_attribs);
  }

  // Pop scissor
  agent->context->ScissorState()->Pop();
}

void GPURenderAlphaTransitionFrameInternal(RenderGraphicsAgent* agent,
                                           float progress) {
  // Initial context
  auto& context = *agent->context;

  // Target UV
  const bool flip_uv = agent->device->IsUVFlip();
  const base::RectF uv_rect = flip_uv ? base::RectF(0.0f, 1.0f, 1.0f, -1.0f)
                                      : base::RectF(0.0f, 0.0f, 1.0f, 1.0f);

  // Update transition uniform
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(&transient_quad, uv_rect);
  renderer::Quad::SetColor(&transient_quad, base::Vec4(progress));
  agent->transition_quads->QueueWrite(*context, &transient_quad);

  // Composite transition frame
  auto render_target_view = agent->screen_buffer->GetDefaultView(
      Diligent::TEXTURE_VIEW_RENDER_TARGET);
  const float clear_color[] = {0, 0, 0, 1};
  context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  base::Vec2i resolution(agent->screen_buffer->GetDesc().Width,
                         agent->screen_buffer->GetDesc().Height);
  agent->context->ScissorState()->Push(resolution);

  // Apply brightness effect
  auto& pipeline_set = agent->device->GetPipelines()->alphatrans;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);

  // Set uniform texture
  static_cast<renderer::Binding_AlphaTrans*>(
      agent->transition_binding_alpha.get())
      ->u_current_texture->Set(agent->transition_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  static_cast<renderer::Binding_AlphaTrans*>(
      agent->transition_binding_alpha.get())
      ->u_frozen_texture->Set(agent->frozen_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  context->SetPipelineState(pipeline);
  context->CommitShaderResources(
      **agent->transition_binding_alpha,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = **agent->transition_quads;
  context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->SetIndexBuffer(**agent->device->GetQuadIndex(), 0,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  context->DrawIndexed(draw_indexed_attribs);

  agent->context->ScissorState()->Pop();
}

void GPURenderVagueTransitionFrameInternal(
    RenderGraphicsAgent* agent,
    float progress,
    float vague,
    Diligent::ITextureView** trans_mapping) {
  // Initial context
  auto& context = *agent->context;

  // Target UV
  const bool flip_uv = agent->device->IsUVFlip();
  const base::RectF uv_rect = flip_uv ? base::RectF(0.0f, 1.0f, 1.0f, -1.0f)
                                      : base::RectF(0.0f, 0.0f, 1.0f, 1.0f);

  // Update transition uniform
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(&transient_quad, uv_rect);
  renderer::Quad::SetColor(&transient_quad, base::Vec4(vague, 0, 0, progress));
  agent->transition_quads->QueueWrite(*context, &transient_quad);

  // Composite transition frame
  auto render_target_view = agent->screen_buffer->GetDefaultView(
      Diligent::TEXTURE_VIEW_RENDER_TARGET);
  const float clear_color[] = {0, 0, 0, 1};
  context->SetRenderTargets(
      1, &render_target_view, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->ClearRenderTarget(
      render_target_view, clear_color,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  base::Vec2i resolution(agent->screen_buffer->GetDesc().Width,
                         agent->screen_buffer->GetDesc().Height);
  agent->context->ScissorState()->Push(resolution);

  // Apply brightness effect
  auto& pipeline_set = agent->device->GetPipelines()->mappedtrans;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);

  // Set uniform texture
  static_cast<renderer::Binding_VagueTrans*>(
      agent->transition_binding_vague.get())
      ->u_current_texture->Set(agent->transition_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  static_cast<renderer::Binding_VagueTrans*>(
      agent->transition_binding_vague.get())
      ->u_frozen_texture->Set(agent->frozen_buffer->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));
  static_cast<renderer::Binding_VagueTrans*>(
      agent->transition_binding_vague.get())
      ->u_trans_texture->Set(*trans_mapping);

  // Apply pipeline state
  context->SetPipelineState(pipeline);
  context->CommitShaderResources(
      **agent->transition_binding_vague,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = **agent->transition_quads;
  context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  context->SetIndexBuffer(**agent->device->GetQuadIndex(), 0,
                          Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  context->DrawIndexed(draw_indexed_attribs);

  agent->context->ScissorState()->Pop();
}

void GPUResizeSwapchainInternal(RenderGraphicsAgent* agent,
                                const base::Vec2i& size) {
  auto* swapchain = agent->device->GetSwapChain();
  swapchain->Resize(size.x, size.y);
}

}  // namespace

RenderScreenImpl::RenderScreenImpl(base::WeakPtr<ui::Widget> window,
                                   base::ThreadWorker* render_worker,
                                   ContentProfile* profile,
                                   I18NProfile* i18n_profile,
                                   filesystem::IOService* io_service,
                                   ScopedFontData* scoped_font,
                                   const base::Vec2i& resolution,
                                   uint32_t frame_rate)
    : window_(window),
      profile_(profile),
      i18n_profile_(i18n_profile),
      render_worker_(render_worker),
      scoped_font_(scoped_font),
      limiter_(frame_rate),
      agent_(nullptr),
      frozen_(false),
      resolution_(resolution),
      brightness_(255),
      vsync_(1),
      frame_count_(0),
      frame_rate_(frame_rate),
      keep_ratio_(true),
      smooth_scale_(profile->smooth_scale),
      allow_skip_frame_(profile->allow_skip_frame),
      background_running_(profile->background_running) {
  // Setup render device on render thread if possible
  agent_ = new RenderGraphicsAgent;

  base::ThreadWorker::PostTask(
      render_worker,
      base::BindOnce(&GPUCreateGraphicsHostInternal, agent_, window, profile,
                     render_worker, io_service, resolution));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker);

  // Initialize viewport
  UpdateWindowViewportInternal();

  // Initialize fps limiter
  limiter_.Reset();
}

RenderScreenImpl::~RenderScreenImpl() {
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(&GPUDestroyGraphicsHostInternal, agent_));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void RenderScreenImpl::PresentScreenBuffer(
    Diligent::ImGuiDiligentRenderer* gui_renderer) {
  // Determine wait delay time
  limiter_.Delay();

  // Update drawing viewport
  UpdateWindowViewportInternal();

  // Present to screen surface
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&GPUPresentScreenBufferInternal, agent_, display_viewport_,
                     resolution_, gui_renderer, smooth_scale_, vsync_));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void RenderScreenImpl::SuspendRenderingContext() {
#if defined(OS_ANDROID)
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(
          [](renderer::RenderDevice* device) { device->SuspendContext(); },
          GetDevice()));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
#endif  // OS_ANDROID
}

void RenderScreenImpl::ResumeRenderingContext() {
#if defined(OS_ANDROID)
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(
                          [](renderer::RenderDevice* device,
                             renderer::RenderContext* immediate_context) {
                            int32_t resume_result =
                                device->ResumeContext(**immediate_context);
                            if (resume_result != EGL_SUCCESS)
                              LOG(INFO) << "Failed to resume EGL context.";
                          },
                          GetDevice(), GetContext()));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
#endif  // OS_ANDROID
  limiter_.Reset();
}

void RenderScreenImpl::CreateButtonGUISettings() {
  if (ImGui::CollapsingHeader(
          i18n_profile_->GetI18NString(IDS_SETTINGS_GRAPHICS, "Graphics")
              .c_str())) {
    ImGui::TextWrapped(
        "%s: %s",
        i18n_profile_->GetI18NString(IDS_GRAPHICS_RENDERER, "Renderer").c_str(),
        agent_->renderer_info.device.c_str());
    ImGui::Separator();
    ImGui::TextWrapped(
        "%s: %s",
        i18n_profile_->GetI18NString(IDS_GRAPHICS_VENDOR, "Vendor").c_str(),
        agent_->renderer_info.vendor.c_str());
    ImGui::Separator();
    ImGui::TextWrapped(
        "%s: %s",
        i18n_profile_->GetI18NString(IDS_GRAPHICS_DESCRIPTION, "Description")
            .c_str(),
        agent_->renderer_info.description.c_str());
    ImGui::Separator();

    // Keep Ratio
    ImGui::Checkbox(
        i18n_profile_->GetI18NString(IDS_GRAPHICS_KEEP_RATIO, "Keep Ratio")
            .c_str(),
        &keep_ratio_);

    // Smooth Scale
    ImGui::Checkbox(
        i18n_profile_->GetI18NString(IDS_GRAPHICS_SMOOTH_SCALE, "Smooth Scale")
            .c_str(),
        &smooth_scale_);

    // Skip Frame
    ImGui::Checkbox(
        i18n_profile_->GetI18NString(IDS_GRAPHICS_SKIP_FRAME, "Skip Frame")
            .c_str(),
        &allow_skip_frame_);

    // Fullscreen
    bool is_fullscreen = GetDevice()->GetWindow()->IsFullscreen(),
         last_fullscreen = is_fullscreen;
    ImGui::Checkbox(
        i18n_profile_->GetI18NString(IDS_GRAPHICS_FULLSCREEN, "Fullscreen")
            .c_str(),
        &is_fullscreen);
    if (last_fullscreen != is_fullscreen)
      GetDevice()->GetWindow()->SetFullscreen(is_fullscreen);

    // Background running
    ImGui::Checkbox(i18n_profile_
                        ->GetI18NString(IDS_GRAPHICS_BACKGROUND_RUNNING,
                                        "Background Running")
                        .c_str(),
                    &background_running_);
  }
}

base::CallbackListSubscription RenderScreenImpl::AddTickObserver(
    const base::RepeatingClosure& handler) {
  return tick_observers_.Add(handler);
}

void RenderScreenImpl::AddDisposable(Disposable* disp) {
  disposable_elements_.Append(disp);
}

void RenderScreenImpl::PostTask(base::OnceClosure task) {
  base::ThreadWorker::PostTask(render_worker_, std::move(task));
}

void RenderScreenImpl::WaitWorkerSynchronize() {
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void RenderScreenImpl::Update(ExceptionState& exception_state) {
  const bool frozen_render = frozen_;
  const bool need_skip_frame = limiter_.RequireFrameSkip() && allow_skip_frame_;

  if (!frozen_render && !need_skip_frame) {
    // Render a frame and push into render queue
    // This function only encodes the render commands
    RenderFrameInternal(agent_->screen_buffer.RawDblPtr());
  }

  if (need_skip_frame)
    limiter_.Reset();

  // Process frame delay
  // This calling will yield to event coroutine and present
  FrameProcessInternal(agent_->screen_buffer.RawDblPtr());
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
      FrameProcessInternal(agent_->frozen_buffer.RawDblPtr());
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
      FrameProcessInternal(agent_->frozen_buffer.RawDblPtr());
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
    RenderFrameInternal(agent_->frozen_buffer.RawDblPtr());

    // Set forzen flag for blocking frame update
    frozen_ = true;
  }
}

void RenderScreenImpl::Transition(ExceptionState& exception_state) {
  Transition(10, base::String(), 40, exception_state);
}

void RenderScreenImpl::Transition(uint32_t duration,
                                  ExceptionState& exception_state) {
  Transition(duration, base::String(), 40, exception_state);
}

void RenderScreenImpl::Transition(uint32_t duration,
                                  const base::String& filename,
                                  ExceptionState& exception_state) {
  Transition(duration, filename, 40, exception_state);
}

void RenderScreenImpl::Transition(uint32_t duration,
                                  const base::String& filename,
                                  uint32_t vague,
                                  ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> transition_mapping = nullptr;
  if (!filename.empty())
    transition_mapping = CanvasImpl::Create(
        GetCanvasScheduler(), this, scoped_font_, filename, exception_state);
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

  // Fetch screen attribute
  Put_Brightness(255, exception_state);
  vague = std::clamp<uint32_t>(vague, 1, 256);
  float vague_norm = vague / 255.0f;

  // Fetch transmapping if available
  scoped_refptr<CanvasImpl> mapping_bitmap = CanvasImpl::FromBitmap(bitmap);
  TextureAgent* texture_agent =
      mapping_bitmap ? mapping_bitmap->GetAgent() : nullptr;
  Diligent::ITextureView** transition_mapping =
      texture_agent ? texture_agent->view.RawDblPtr() : nullptr;

  // Get current scene snapshot for transition
  RenderFrameInternal(agent_->transition_buffer.RawDblPtr());

  // Transition render loop
  for (uint32_t i = 0; i < duration; ++i) {
    // Norm transition progress
    float progress = i * (1.0f / duration);

    // Render per transition frame
    if (transition_mapping)
      base::ThreadWorker::PostTask(
          render_worker_,
          base::BindOnce(&GPURenderVagueTransitionFrameInternal, agent_,
                         progress, vague_norm, transition_mapping));
    else
      base::ThreadWorker::PostTask(
          render_worker_, base::BindOnce(&GPURenderAlphaTransitionFrameInternal,
                                         agent_, progress));

    // Present to screen
    FrameProcessInternal(agent_->screen_buffer.RawDblPtr());
  }

  // Transition process complete
  frozen_ = false;
}

scoped_refptr<Bitmap> RenderScreenImpl::SnapToBitmap(
    ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> target = CanvasImpl::Create(
      GetCanvasScheduler(), this, scoped_font_, resolution_, exception_state);

  if (target) {
    RenderFrameInternal(target->GetAgent()->data.RawDblPtr());
    base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
  }

  return target;
}

void RenderScreenImpl::FrameReset(ExceptionState& exception_state) {
  limiter_.Reset();
}

uint32_t RenderScreenImpl::Width(ExceptionState& exception_state) {
  return resolution_.x;
}

uint32_t RenderScreenImpl::Height(ExceptionState& exception_state) {
  return resolution_.y;
}

void RenderScreenImpl::ResizeScreen(uint32_t width,
                                    uint32_t height,
                                    ExceptionState& exception_state) {
  resolution_ = base::Vec2i(width, height);
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(&GPUResetScreenBufferInternal, agent_,
                                     resolution_, -origin_));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void RenderScreenImpl::Reset(ExceptionState& exception_state) {
  /* Reset freeze */
  frozen_ = false;

  /* Disposed all elements */
  for (auto it = disposable_elements_.tail(); it != disposable_elements_.end();
       it = it->previous()) {
    it->value()->Dispose(exception_state);
  }

  /* Reset attribute */
  frame_rate_ =
      profile_->api_version == ContentProfile::APIVersion::RGSS1 ? 40 : 60;
  brightness_ = 255;
  FrameReset(exception_state);
}

void RenderScreenImpl::PlayMovie(const base::String& filename,
                                 ExceptionState& exception_state) {
  exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                             "Unimplement: Graphics.play_movie");
}

void RenderScreenImpl::MoveWindow(int32_t x,
                                  int32_t y,
                                  int32_t width,
                                  int32_t height,
                                  ExceptionState& exception_state) {
  auto* window = window_->AsSDLWindow();
  SDL_SetWindowPosition(window, x, y);
  SDL_SetWindowSize(window, width, height);
}

scoped_refptr<Rect> RenderScreenImpl::GetWindowRect(
    ExceptionState& exception_state) {
  auto* window = window_->AsSDLWindow();
  base::Rect window_rect;
  SDL_GetWindowPosition(window, &window_rect.x, &window_rect.y);
  SDL_GetWindowSize(window, &window_rect.width, &window_rect.height);
  return base::MakeRefCounted<RectImpl>(window_rect);
}

uint32_t RenderScreenImpl::GetDisplayID(ExceptionState& exception_state) {
  return SDL_GetDisplayForWindow(window_->AsSDLWindow());
}

void RenderScreenImpl::SetWindowIcon(scoped_refptr<Bitmap> icon,
                                     ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> data = CanvasImpl::FromBitmap(icon);
  SDL_Surface* icon_surface = data->RequireMemorySurface();
  SDL_SetWindowIcon(window_->AsSDLWindow(), icon_surface);
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
  return vsync_;
}

void RenderScreenImpl::Put_VSync(const uint32_t& value,
                                 ExceptionState& exception_state) {
  vsync_ = std::clamp<uint32_t>(value, 0, 255);
}

bool RenderScreenImpl::Get_Fullscreen(ExceptionState& exception_state) {
  return window_->IsFullscreen();
}

void RenderScreenImpl::Put_Fullscreen(const bool& value,
                                      ExceptionState& exception_state) {
  window_->SetFullscreen(value);
}

bool RenderScreenImpl::Get_Skipframe(ExceptionState& exception_state) {
  return allow_skip_frame_;
}

void RenderScreenImpl::Put_Skipframe(const bool& value,
                                     ExceptionState& exception_state) {
  allow_skip_frame_ = value;
}

bool RenderScreenImpl::Get_KeepRatio(ExceptionState& exception_state) {
  return keep_ratio_;
}

void RenderScreenImpl::Put_KeepRatio(const bool& value,
                                     ExceptionState& exception_state) {
  keep_ratio_ = value;
}

bool RenderScreenImpl::Get_SmoothScale(ExceptionState& exception_state) {
  return smooth_scale_;
}

void RenderScreenImpl::Put_SmoothScale(const bool& value,
                                       ExceptionState& exception_state) {
  smooth_scale_ = value;
}

bool RenderScreenImpl::Get_BackgroundRunning(ExceptionState& exception_state) {
  return background_running_;
}

void RenderScreenImpl::Put_BackgroundRunning(const bool& value,
                                             ExceptionState& exception_state) {
  background_running_ = value;
}

int32_t RenderScreenImpl::Get_Ox(ExceptionState& exception_state) {
  return origin_.x;
}

void RenderScreenImpl::Put_Ox(const int32_t& value,
                              ExceptionState& exception_state) {
  origin_.x = value;
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(&GPUUpdateScreenWorldInternal, agent_,
                                     resolution_, -origin_));
}

int32_t RenderScreenImpl::Get_Oy(ExceptionState& exception_state) {
  return origin_.y;
}

void RenderScreenImpl::Put_Oy(const int32_t& value,
                              ExceptionState& exception_state) {
  origin_.y = value;
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(&GPUUpdateScreenWorldInternal, agent_,
                                     resolution_, -origin_));
}

base::String RenderScreenImpl::Get_WindowTitle(ExceptionState& exception_state) {
  return SDL_GetWindowTitle(window_->AsSDLWindow());
}

void RenderScreenImpl::Put_WindowTitle(const base::String& value,
                                       ExceptionState& exception_state) {
  SDL_SetWindowTitle(window_->AsSDLWindow(), value.c_str());
}

void RenderScreenImpl::FrameProcessInternal(
    Diligent::ITexture** present_target) {
  // Setup target
  agent_->present_target = present_target;

  // Increase frame render count
  ++frame_count_;

  // Tick callback
  tick_observers_.Notify();
}

void RenderScreenImpl::RenderFrameInternal(Diligent::ITexture** render_target) {
  // Submit pending canvas commands
  GetCanvasScheduler()->SubmitPendingPaintCommands();

  // Prepare for rendering context
  DrawableNode::RenderControllerParams controller_params;
  controller_params.device = GetDevice();
  controller_params.context = GetContext();
  controller_params.screen_buffer = render_target;
  controller_params.screen_size = resolution_;
  controller_params.viewport = resolution_;
  controller_params.origin = origin_;

  // 1) Execute pre-composite handler
  controller_.BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                    &controller_params);

  // 1.5) Update sprite batch if need
  if (GetSpriteBatch())
    base::ThreadWorker::PostTask(
        render_worker_,
        base::BindOnce(&SpriteBatch::SubmitBatchDataAndResetCache,
                       base::Unretained(GetSpriteBatch()),
                       controller_params.device, controller_params.context));

  // 2) Setup renderpass
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(&GPUFrameBeginRenderPassInternal, agent_,
                                     render_target, resolution_, brightness_));

  // 3) Notify render a frame
  controller_params.root_world = agent_->root_transform.RawDblPtr();
  controller_params.world_binding = agent_->world_transform.RawDblPtr();
  controller_.BroadCastNotification(DrawableNode::ON_RENDERING,
                                    &controller_params);

  // 4) End render pass and process after-render effect
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&GPUFrameEndRenderPassInternal, agent_, brightness_));
}

void RenderScreenImpl::UpdateWindowViewportInternal() {
  auto window_size = window_->GetSize();

  if (!(window_size == window_size_)) {
    window_size_ = window_size;

    // Resize screen surface
    base::ThreadWorker::PostTask(
        render_worker_,
        base::BindOnce(&GPUResizeSwapchainInternal, agent_, window_size_));
  }

  // Update real display viewport
  float window_ratio = static_cast<float>(window_size.x) / window_size.y;
  float screen_ratio = static_cast<float>(resolution_.x) / resolution_.y;

  display_viewport_.width = window_size.x;
  display_viewport_.height = window_size.y;

  if (screen_ratio > window_ratio)
    display_viewport_.height = display_viewport_.width / screen_ratio;
  else if (screen_ratio < window_ratio)
    display_viewport_.width = display_viewport_.height * screen_ratio;

  display_viewport_.x = (window_size.x - display_viewport_.width) / 2.0f;
  display_viewport_.y = (window_size.y - display_viewport_.height) / 2.0f;

  // Process mouse coordinate and viewport rect
  base::WeakPtr<ui::Widget> window = agent_->device->GetWindow();
  window->GetDisplayState().scale =
      base::Vec2(display_viewport_.Size()) / base::Vec2(resolution_);
  window->GetDisplayState().viewport = display_viewport_;
}

}  // namespace content

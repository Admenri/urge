// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/screen/renderscreen_impl.h"

#include <unordered_map>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_sdl3.h"
#include "third_party/imgui/imgui_impl_wgpu.h"

#include "content/canvas/canvas_scheduler.h"
#include "renderer/utils/texture_utils.h"

namespace content {

namespace {

wgpu::BackendType GetWGPUBackend(const std::string& wgpu_backend) {
  static const std::unordered_map<std::string, wgpu::BackendType> backend_map =
      {{"Null", wgpu::BackendType::Null},
       {"WebGPU", wgpu::BackendType::WebGPU},
       {"D3D11", wgpu::BackendType::D3D11},
       {"D3D12", wgpu::BackendType::D3D12},
       {"Metal", wgpu::BackendType::Metal},
       {"Vulkan", wgpu::BackendType::Vulkan},
       {"OpenGL", wgpu::BackendType::OpenGL},
       {"OpenGLES", wgpu::BackendType::OpenGLES}};

  auto it = backend_map.find(wgpu_backend);
  if (it != backend_map.end())
    return it->second;

  return wgpu::BackendType::Undefined;
}

}  // namespace

RenderScreenImpl::RenderScreenImpl(CoroutineContext* cc,
                                   ContentProfile* profile,
                                   filesystem::IOService* io_service,
                                   ScopedFontData* scoped_font,
                                   const base::Vec2i& resolution,
                                   int frame_rate)
    : cc_(cc),
      profile_(profile),
      io_service_(io_service),
      render_worker_(nullptr),
      scoped_font_(scoped_font),
      agent_(nullptr),
      frozen_(false),
      resolution_(resolution),
      brightness_(255),
      frame_count_(0),
      frame_rate_(frame_rate),
      elapsed_time_(0.0),
      smooth_delta_time_(1.0),
      last_count_time_(SDL_GetPerformanceCounter()),
      desired_delta_time_(SDL_GetPerformanceFrequency() / frame_rate_),
      frame_skip_required_(false) {}

RenderScreenImpl::~RenderScreenImpl() {
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&RenderScreenImpl::DestroyGraphicsDeviceInternal,
                     base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void RenderScreenImpl::InitWithRenderWorker(base::ThreadWorker* render_worker,
                                            base::WeakPtr<ui::Widget> window,
                                            const std::string& wgpu_backend) {
  // Setup render thread worker (maybe null)
  render_worker_ = render_worker;

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

  // Apply DPI Settings
  int display_w, display_h;
  SDL_GetWindowSizeInPixels(window->AsSDLWindow(), &display_w, &display_h);
  io.DisplaySize =
      ImVec2(static_cast<float>(display_w), static_cast<float>(display_h));
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
  float window_scale = SDL_GetWindowDisplayScale(window->AsSDLWindow());
  ImGui::GetStyle().ScaleAllSizes(window_scale);

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup imgui platform backends
  ImGui_ImplSDL3_InitForOther(window->AsSDLWindow());

  // Setup render device on render thread if possible
  agent_ = new RenderGraphicsAgent;
  base::ThreadWorker::PostTask(
      render_worker,
      base::BindOnce(&RenderScreenImpl::InitGraphicsDeviceInternal,
                     base::Unretained(this), window, wgpu_backend));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker);
}

bool RenderScreenImpl::ExecuteEventMainLoop() {
  // Poll event queue
  SDL_Event queued_event;
  while (SDL_PollEvent(&queued_event)) {
    // Quit event
    if (queued_event.type == SDL_EVENT_QUIT)
      return false;

    // IMGUI Event
    ImGui_ImplSDL3_ProcessEvent(&queued_event);
  }

  // Start IMGUI frame
  ImGui_ImplSDL3_NewFrame();

  // Layout new frame
  ImGui::NewFrame();
  ImGui::ShowDemoWindow();
  ImGui::EndFrame();

  // Determine update repeat time
  const uint64_t now_time = SDL_GetPerformanceCounter();
  const uint64_t delta_time = now_time - last_count_time_;
  last_count_time_ = now_time;

  // Calculate smooth frame rate
  const double delta_rate =
      delta_time / static_cast<double>(desired_delta_time_);
  const int repeat_time = DetermineRepeatNumberInternal(delta_rate);

  for (int i = 0; i < repeat_time; ++i) {
    frame_skip_required_ = (i != 0);
    fiber_switch(cc_->main_loop_fiber);
  }

  // Present to screen surface
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&RenderScreenImpl::PresentScreenBufferInternal,
                     base::Unretained(this), agent_->present_target));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);

  return true;
}

renderer::RenderDevice* RenderScreenImpl::GetDevice() const {
  return agent_->device.get();
}

renderer::DeviceContext* RenderScreenImpl::GetContext() const {
  return agent_->context.get();
}

renderer::QuadrangleIndexCache* RenderScreenImpl::GetQuadIndexCache() const {
  return agent_->index_cache.get();
}

CanvasScheduler* RenderScreenImpl::GetCanvasScheduler() const {
  return agent_->canvas_scheduler.get();
}

void RenderScreenImpl::PostTask(base::OnceClosure task) {
  base::ThreadWorker::PostTask(render_worker_, std::move(task));
}

void RenderScreenImpl::WaitWorkerSynchronize() {
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

base::CallbackListSubscription RenderScreenImpl::AddTickObserver(
    const base::RepeatingClosure& handler) {
  return tick_observers_.Add(handler);
}

void RenderScreenImpl::RenderFrameInternal(DrawNodeController* controller,
                                           wgpu::Texture* render_target,
                                           const base::Vec2i& target_size) {
  // Submit pending canvas commands
  agent_->canvas_scheduler->SubmitPendingPaintCommands();

  // Prepare for rendering context
  DrawableNode::RenderControllerParams controller_params;
  controller_params.device = agent_->device.get();
  controller_params.index_cache = agent_->index_cache.get();
  controller_params.command_encoder = agent_->context->GetImmediateEncoder();
  controller_params.screen_buffer = render_target;
  controller_params.screen_size = target_size;
  controller_params.viewport = target_size;

  // 1) Execute pre-composite handler
  controller->BroadCastNotification(DrawableNode::BEFORE_RENDER,
                                    &controller_params);

  // 2) Setup renderpass
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&RenderScreenImpl::FrameBeginRenderPassInternal,
                     base::Unretained(this), render_target));

  // 3) Notify render a frame
  controller_params.root_world = &agent_->world_binding;
  controller_params.world_binding = &agent_->world_binding;
  controller_params.renderpass_encoder = &agent_->render_pass;
  controller->BroadCastNotification(DrawableNode::ON_RENDERING,
                                    &controller_params);

  // 4) End render pass and process after-render effect
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&RenderScreenImpl::FrameEndRenderPassInternal,
                     base::Unretained(this)));
}

void RenderScreenImpl::Update(ExceptionState& exception_state) {
  if (!frozen_ && !frame_skip_required_)
    RenderFrameInternal(&controller_, &agent_->screen_buffer, resolution_);

  // Process frame delay
  FrameProcessInternal(&agent_->screen_buffer);
}

void RenderScreenImpl::Wait(uint32_t duration,
                            ExceptionState& exception_state) {
  for (int32_t i = 0; i < duration; ++i)
    Update(exception_state);
}

void RenderScreenImpl::FadeOut(uint32_t duration,
                               ExceptionState& exception_state) {
  duration = std::max(duration, 1u);

  float current_brightness = static_cast<float>(brightness_);
  for (int i = 0; i < duration; ++i) {
    brightness_ = current_brightness -
                  current_brightness * (i / static_cast<float>(duration));
    if (frozen_) {
      FrameProcessInternal(&agent_->frozen_buffer);
    } else {
      Update(exception_state);
    }
  }

  // Set final brightness
  brightness_ = 0;
}

void RenderScreenImpl::FadeIn(uint32_t duration,
                              ExceptionState& exception_state) {
  duration = std::max(duration, 1u);

  float current_brightness = static_cast<float>(brightness_);
  float diff = 255.0f - current_brightness;
  for (int i = 0; i < duration; ++i) {
    brightness_ =
        current_brightness + diff * (i / static_cast<float>(duration));

    if (frozen_) {
      FrameProcessInternal(&agent_->frozen_buffer);
    } else {
      Update(exception_state);
    }
  }

  // Set final brightness
  brightness_ = 255;
}

void RenderScreenImpl::Freeze(ExceptionState& exception_state) {
  if (!frozen_) {
    // Get frozen scene snapshot for transition
    RenderFrameInternal(&controller_, &agent_->frozen_buffer, resolution_);

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
    transition_mapping = CanvasImpl::Create(
        GetCanvasScheduler(), this, scoped_font_, filename, exception_state);
  if (!transition_mapping || exception_state.HadException())
    return;

  TransitionWithBitmap(duration, transition_mapping, vague, exception_state);
}

void RenderScreenImpl::TransitionWithBitmap(uint32_t duration,
                                            scoped_refptr<Bitmap> bitmap,
                                            uint32_t vague,
                                            ExceptionState& exception_state) {
  if (!frozen_)
    return;

  // Fix screen attribute
  Put_Brightness(255, exception_state);
  vague = std::clamp<int>(vague, 1, 256);
  float vague_norm = vague / 255.0f;
  wgpu::Texture* transition_mapping = nullptr;

  if (bitmap)
    transition_mapping = &CanvasImpl::FromBitmap(bitmap)->GetAgent()->data;

  // Get current scene snapshot for transition
  RenderFrameInternal(&controller_, &agent_->transition_buffer, resolution_);

  // Create binding group
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&RenderScreenImpl::CreateTransitionUniformInternal,
                     base::Unretained(this), transition_mapping));

  for (int i = 0; i < duration; ++i) {
    // Norm transition progress
    float progress = i * (1.0f / duration);

    // Render per transition frame
    if (transition_mapping)
      base::ThreadWorker::PostTask(
          render_worker_,
          base::BindOnce(&RenderScreenImpl::RenderVagueTransitionFrameInternal,
                         base::Unretained(this), progress, vague_norm));
    else
      base::ThreadWorker::PostTask(
          render_worker_,
          base::BindOnce(&RenderScreenImpl::RenderAlphaTransitionFrameInternal,
                         base::Unretained(this), progress));

    // Present to screen
    FrameProcessInternal(&agent_->screen_buffer);
  }

  // Transition process complete
  frozen_ = false;
}

scoped_refptr<Bitmap> RenderScreenImpl::SnapToBitmap(
    ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> target = CanvasImpl::Create(
      GetCanvasScheduler(), this, scoped_font_, resolution_, exception_state);

  if (target) {
    RenderFrameInternal(&controller_, &target->GetAgent()->data, resolution_);
    base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
  }

  return target;
}

void RenderScreenImpl::FrameReset(ExceptionState& exception_state) {
  elapsed_time_ = 0.0;
  smooth_delta_time_ = 1.0;
  last_count_time_ = SDL_GetPerformanceCounter();
  desired_delta_time_ = SDL_GetPerformanceFrequency() / frame_rate_;
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
      render_worker_,
      base::BindOnce(&RenderScreenImpl::ResetScreenBufferInternal,
                     base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void RenderScreenImpl::PlayMovie(const std::string& filename,
                                 ExceptionState& exception_state) {
  exception_state.ThrowContentError(ExceptionCode::CONTENT_ERROR,
                                    "unimplement Graphics.play_movie");
}

void RenderScreenImpl::InitGraphicsDeviceInternal(
    base::WeakPtr<ui::Widget> window,
    const std::string& wgpu_backend) {
  // Create device on window
  agent_->device =
      renderer::RenderDevice::Create(window, GetWGPUBackend(wgpu_backend));

  // Create immediate command encoder
  agent_->context =
      renderer::DeviceContext::MakeContextFor(agent_->device.get());

  // Quad index generic buffer (reserved 1024)
  agent_->index_cache =
      renderer::QuadrangleIndexCache::Make(agent_->device.get());
  agent_->index_cache->Allocate(1 << 10);

  // Create global canvas scheduler
  agent_->canvas_scheduler =
      CanvasScheduler::MakeInstance(agent_->device.get(), agent_->context.get(),
                                    agent_->index_cache.get(), io_service_);
  agent_->canvas_scheduler->InitWithRenderWorker(render_worker_);

  // Create screen world matrix buffer
  agent_->world_buffer =
      renderer::CreateUniformBuffer<renderer::WorldMatrixUniform>(
          **agent_->device, "screen.world.matrix", wgpu::BufferUsage::CopyDst);

  // Create uniform binding
  agent_->world_binding = renderer::WorldMatrixUniform::CreateGroup(
      **agent_->device, agent_->world_buffer);

  // Create effect vertex buffer
  agent_->effect_vertex =
      renderer::CreateVertexBuffer<renderer::FullVertexLayout>(
          **agent_->device, "screen.brightness.vertex",
          wgpu::BufferUsage::CopyDst, 4);

  // Create screen present pipeline and buffer
  agent_->present_pipeline.reset(new renderer::Pipeline_Base(
      **agent_->device, agent_->device->SurfaceFormat()));
  agent_->present_vertex =
      renderer::CreateVertexBuffer<renderer::FullVertexLayout>(
          **agent_->device, "screen.vertex", wgpu::BufferUsage::CopyDst, 4);

  // Setup imgui renderer backends
  ImGui_ImplWGPU_InitInfo init_info;
  init_info.Device = (*agent_->device)->Get();
  init_info.NumFramesInFlight = 3;
  init_info.RenderTargetFormat =
      static_cast<WGPUTextureFormat>(agent_->device->SurfaceFormat());
  init_info.DepthStencilFormat = WGPUTextureFormat_Undefined;
  ImGui_ImplWGPU_Init(&init_info);
  ImGui_ImplWGPU_NewFrame();

  // Create screen buffer
  ResetScreenBufferInternal();
}

void RenderScreenImpl::DestroyGraphicsDeviceInternal() {
  // Cleanup IMGUI Components
  ImGui_ImplWGPU_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  // Release agent resource
  delete agent_;
}

void RenderScreenImpl::PresentScreenBufferInternal(
    wgpu::Texture* render_target) {
  // Update drawing viewport
  UpdateWindowViewportInternal();

  // Flip screen for Y
  base::WeakPtr<ui::Widget> window = agent_->device->GetWindow();
  window->GetMouseState().resolution = resolution_;
  window->GetMouseState().screen_offset = display_viewport_.Position();
  window->GetMouseState().screen = display_viewport_.Size();

  // Get screen surface
  auto* hardware_surface = agent_->device->GetSurface();

  wgpu::SurfaceTexture surface_texture;
  hardware_surface->GetCurrentTexture(&surface_texture);

  // Blit internal screen buffer to screen surface
  auto* commander = agent_->context->GetImmediateEncoder();

  wgpu::RenderPassColorAttachment attachment;
  attachment.view = surface_texture.texture.CreateView();
  attachment.loadOp = wgpu::LoadOp::Clear;
  attachment.storeOp = wgpu::StoreOp::Store;
  attachment.clearValue = {0, 0, 0, 1};

  wgpu::RenderPassDescriptor renderpass;
  renderpass.colorAttachmentCount = 1;
  renderpass.colorAttachments = &attachment;

  if (render_target) {
    // Setup binding and vertex
    wgpu::RenderPipeline& pipeline =
        *agent_->present_pipeline->GetPipeline(renderer::BlendType::NO_BLEND);
    wgpu::BindGroup world_binding, texture_binding;

    {
      renderer::FullVertexLayout transient_vertices[4];
      renderer::FullVertexLayout::SetPositionRect(transient_vertices,
                                                  display_viewport_);
      renderer::FullVertexLayout::SetTexCoordRect(transient_vertices,
                                                  base::Rect(resolution_));
      renderer::FullVertexLayout::SetColor(transient_vertices, base::Vec4(1));
      commander->WriteBuffer(agent_->present_vertex, 0,
                             reinterpret_cast<uint8_t*>(transient_vertices),
                             sizeof(transient_vertices));
    }

    {
      renderer::WorldMatrixUniform world_matrix;
      renderer::MakeProjectionMatrix(world_matrix.projection,
                                     window->GetSize());
      renderer::MakeIdentityMatrix(world_matrix.transform);

      wgpu::Buffer world_matrix_uniform =
          renderer::CreateUniformBuffer<renderer::WorldMatrixUniform>(
              **agent_->device, "present.world.uniform",
              wgpu::BufferUsage::None, &world_matrix);

      world_binding = renderer::WorldMatrixUniform::CreateGroup(
          **agent_->device, world_matrix_uniform);
    }

    {
      renderer::TextureBindingUniform uniform;
      uniform.texture_size = base::MakeInvert(resolution_);
      wgpu::Buffer uniform_buffer =
          renderer::CreateUniformBuffer<renderer::TextureBindingUniform>(
              **agent_->device, "present.texture", wgpu::BufferUsage::None,
              &uniform);

      texture_binding = renderer::TextureBindingUniform::CreateGroup(
          **agent_->device, render_target->CreateView(),
          (*agent_->device)->CreateSampler(), uniform_buffer);
    }

    // Start screen render
    {
      auto pass = commander->BeginRenderPass(&renderpass);
      pass.SetPipeline(*agent_->present_pipeline->GetPipeline(
          renderer::BlendType::NO_BLEND));
      pass.SetViewport(0, 0, window->GetSize().x, window->GetSize().y, 0, 0);
      pass.SetBindGroup(0, world_binding);
      pass.SetBindGroup(1, texture_binding);
      pass.SetVertexBuffer(0, agent_->present_vertex);
      pass.SetIndexBuffer(**agent_->index_cache, agent_->index_cache->format());
      pass.DrawIndexed(6);
      pass.End();
    }
  }

  // Start imgui layer render
  {
    // New frame
    ImGui_ImplWGPU_NewFrame();

    // Rendering IMGUI
    ImGui::Render();

    attachment.loadOp = wgpu::LoadOp::Load;
    auto pass = commander->BeginRenderPass(&renderpass);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass.Get());
    pass.End();
  }

  // Flush command buffer and present WGPU surface
  agent_->context->Flush();
  hardware_surface->Present();
}

void RenderScreenImpl::FrameProcessInternal(wgpu::Texture* present_target) {
  // Setup target
  agent_->present_target = present_target;

  // Increase frame render count
  ++frame_count_;

  // Tick callback
  tick_observers_.Notify();

  // Switch to primary fiber
  fiber_switch(cc_->primary_fiber);
}

void RenderScreenImpl::UpdateWindowViewportInternal() {
  auto window_size = agent_->device->GetWindow()->GetSize();

  if (!(window_size == window_size_)) {
    window_size_ = window_size;

    // Configure of new surface
    wgpu::SurfaceConfiguration config;
    config.device = **agent_->device;
    config.format = agent_->device->SurfaceFormat();
    config.width = window_size_.x;
    config.height = window_size_.y;

    // Resize screen surface
    auto* hardware_surface = agent_->device->GetSurface();
    hardware_surface->Configure(&config);
  }

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
}

void RenderScreenImpl::ResetScreenBufferInternal() {
  wgpu::TextureUsage usage =
      wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::CopySrc |
      wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;

  agent_->screen_buffer = renderer::CreateTexture2D(
      **GetDevice(), "screen.buffer", usage, resolution_);
  agent_->frozen_buffer = renderer::CreateTexture2D(
      **GetDevice(), "screen.buffer", usage, resolution_);
  agent_->transition_buffer = renderer::CreateTexture2D(
      **GetDevice(), "screen.buffer", usage, resolution_);
}

int RenderScreenImpl::DetermineRepeatNumberInternal(double delta_rate) {
  smooth_delta_time_ *= 0.8;
  smooth_delta_time_ += std::fmin(delta_rate, 2) * 0.2;

  if (smooth_delta_time_ >= 0.9) {
    elapsed_time_ = 0;
    return std::round(smooth_delta_time_);
  } else {
    elapsed_time_ += delta_rate;
    if (elapsed_time_ >= 1) {
      elapsed_time_ -= 1;
      return 1;
    }
  }

  return 0;
}

void RenderScreenImpl::FrameBeginRenderPassInternal(
    wgpu::Texture* render_target) {
  auto* encoder = agent_->context->GetImmediateEncoder();
  base::Vec2i target_size(render_target->GetWidth(),
                          render_target->GetHeight());

  wgpu::RenderPassColorAttachment attachment;
  attachment.view = render_target->CreateView();
  attachment.loadOp = wgpu::LoadOp::Clear;
  attachment.storeOp = wgpu::StoreOp::Store;
  attachment.clearValue = {0, 0, 0, 1};

  wgpu::RenderPassDescriptor renderpass;
  renderpass.colorAttachmentCount = 1;
  renderpass.colorAttachments = &attachment;

  // Update world matrix size
  renderer::WorldMatrixUniform world_matrix;
  renderer::MakeProjectionMatrix(world_matrix.projection, target_size);
  renderer::MakeIdentityMatrix(world_matrix.transform);
  encoder->WriteBuffer(agent_->world_buffer, 0,
                       reinterpret_cast<uint8_t*>(&world_matrix),
                       sizeof(world_matrix));

  // Update brightness
  if (brightness_ < 255) {
    renderer::FullVertexLayout effect_vertices[4];
    renderer::FullVertexLayout::SetPositionRect(effect_vertices,
                                                base::Rect(target_size));
    renderer::FullVertexLayout::SetColor(
        effect_vertices, base::Vec4(0, 0, 0, (255 - brightness_) / 255.0f));
    encoder->WriteBuffer(agent_->effect_vertex, 0,
                         reinterpret_cast<uint8_t*>(effect_vertices),
                         sizeof(effect_vertices));
  }

  // Begin render pass
  agent_->render_pass = encoder->BeginRenderPass(&renderpass);
  agent_->render_pass.SetViewport(0, 0, target_size.x, target_size.y, 0, 0);
  agent_->render_pass.SetScissorRect(0, 0, target_size.x, target_size.y);
}

void RenderScreenImpl::FrameEndRenderPassInternal() {
  if (brightness_ < 255) {
    // Apply brightness effect
    auto& pipeline_set = agent_->device->GetPipelines()->color;
    auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NORMAL);

    agent_->render_pass.SetPipeline(*pipeline);
    agent_->render_pass.SetBindGroup(0, agent_->world_binding);
    agent_->render_pass.SetVertexBuffer(0, agent_->effect_vertex);
    agent_->render_pass.SetIndexBuffer(**agent_->index_cache,
                                       agent_->index_cache->format());
    agent_->render_pass.DrawIndexed(6);
  }

  // End render pass
  agent_->render_pass.End();
}

void RenderScreenImpl::CreateTransitionUniformInternal(
    wgpu::Texture* transition_mapping) {
  if (transition_mapping)
    agent_->transition_binding = renderer::VagueTransitionUniform::CreateGroup(
        **GetDevice(), agent_->frozen_buffer.CreateView(),
        agent_->transition_buffer.CreateView(),
        transition_mapping->CreateView(), (*GetDevice())->CreateSampler());
  else
    agent_->transition_binding = renderer::AlphaTransitionUniform::CreateGroup(
        **GetDevice(), agent_->frozen_buffer.CreateView(),
        agent_->transition_buffer.CreateView(),
        (*GetDevice())->CreateSampler());
}

void RenderScreenImpl::RenderAlphaTransitionFrameInternal(float progress) {
  auto* command_encoder = agent_->context->GetImmediateEncoder();

  wgpu::RenderPassColorAttachment attachment;
  attachment.view = agent_->screen_buffer.CreateView();
  attachment.loadOp = wgpu::LoadOp::Load;
  attachment.storeOp = wgpu::StoreOp::Store;

  wgpu::RenderPassDescriptor renderpass;
  renderpass.colorAttachmentCount = 1;
  renderpass.colorAttachments = &attachment;

  // Update transition uniform
  renderer::FullVertexLayout transient_vertices[4];
  renderer::FullVertexLayout::SetPositionRect(
      transient_vertices, base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::FullVertexLayout::SetTexCoordRect(
      transient_vertices, base::RectF(0.0f, 0.0f, 1.0f, 1.0f));
  renderer::FullVertexLayout::SetColor(transient_vertices,
                                       base::Vec4(progress));
  command_encoder->WriteBuffer(agent_->effect_vertex, 0,
                               reinterpret_cast<uint8_t*>(transient_vertices),
                               sizeof(transient_vertices));

  // Composite transition frame
  auto& pipeline_set = agent_->device->GetPipelines()->alphatrans;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);

  wgpu::RenderPassEncoder render_pass =
      command_encoder->BeginRenderPass(&renderpass);
  render_pass.SetPipeline(*pipeline);
  render_pass.SetViewport(0, 0, agent_->screen_buffer.GetWidth(),
                          agent_->screen_buffer.GetHeight(), 0, 0);
  render_pass.SetBindGroup(0, agent_->transition_binding);
  render_pass.SetVertexBuffer(0, agent_->effect_vertex);
  render_pass.SetIndexBuffer(**agent_->index_cache,
                             agent_->index_cache->format());
  render_pass.DrawIndexed(6);
  render_pass.End();
}

void RenderScreenImpl::RenderVagueTransitionFrameInternal(float progress,
                                                          float vague) {
  auto* command_encoder = agent_->context->GetImmediateEncoder();

  wgpu::RenderPassColorAttachment attachment;
  attachment.view = agent_->screen_buffer.CreateView();
  attachment.loadOp = wgpu::LoadOp::Load;
  attachment.storeOp = wgpu::StoreOp::Store;

  wgpu::RenderPassDescriptor renderpass;
  renderpass.colorAttachmentCount = 1;
  renderpass.colorAttachments = &attachment;

  // Update transition uniform
  renderer::FullVertexLayout transient_vertices[4];
  renderer::FullVertexLayout::SetPositionRect(
      transient_vertices, base::RectF(-1.0f, -1.0f, 2.0f, 2.0f));
  renderer::FullVertexLayout::SetTexCoordRect(
      transient_vertices, base::RectF(0.0f, 0.0f, 1.0f, 1.0f));
  renderer::FullVertexLayout::SetColor(transient_vertices,
                                       base::Vec4(vague, 0, 0, progress));
  command_encoder->WriteBuffer(agent_->effect_vertex, 0,
                               reinterpret_cast<uint8_t*>(transient_vertices),
                               sizeof(transient_vertices));

  // Composite transition frame
  auto& pipeline_set = agent_->device->GetPipelines()->mappedtrans;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NO_BLEND);

  wgpu::RenderPassEncoder render_pass =
      command_encoder->BeginRenderPass(&renderpass);
  render_pass.SetPipeline(*pipeline);
  render_pass.SetViewport(0, 0, agent_->screen_buffer.GetWidth(),
                          agent_->screen_buffer.GetHeight(), 0, 0);
  render_pass.SetBindGroup(0, agent_->transition_binding);
  render_pass.SetVertexBuffer(0, agent_->effect_vertex);
  render_pass.SetIndexBuffer(**agent_->index_cache,
                             agent_->index_cache->format());
  render_pass.DrawIndexed(6);
  render_pass.End();
}

void RenderScreenImpl::AddDisposable(base::LinkNode<Disposable>* disp) {
  disposable_elements_.Append(disp);
}

uint32_t RenderScreenImpl::Get_FrameRate(ExceptionState&) {
  return frame_rate_;
}

void RenderScreenImpl::Put_FrameRate(const uint32_t& rate, ExceptionState&) {
  frame_rate_ = rate;
}

uint32_t RenderScreenImpl::Get_FrameCount(ExceptionState&) {
  return frame_count_;
}

void RenderScreenImpl::Put_FrameCount(const uint32_t& count, ExceptionState&) {
  frame_count_ = count;
}

uint32_t RenderScreenImpl::Get_Brightness(ExceptionState&) {
  return brightness_;
}

void RenderScreenImpl::Put_Brightness(const uint32_t& value, ExceptionState&) {
  brightness_ = value;
}

}  // namespace content

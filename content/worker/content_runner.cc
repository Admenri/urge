// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/content_runner.h"

#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/imgui.h"
#include "magic_enum/magic_enum.hpp"

#include "components/version/version.h"
#include "content/context/execution_context.h"
#include "content/profile/command_ids.h"

namespace content {

namespace {

void DrawEngineInfoGUI(I18NProfile* i18n_profile) {
  if (ImGui::CollapsingHeader(
          i18n_profile->GetI18NString(IDS_SETTINGS_ABOUT, "About").c_str())) {
    ImGui::Text("Universal Ruby Game Engine (URGE) Runtime");
    ImGui::Separator();
    ImGui::Text("Copyright (C) 2018-2025 Admenri Adev.");
    ImGui::TextWrapped(
        "The URGE is licensed under the BSD-2-Clause License, see LICENSE for "
        "more information.");
    ImGui::Separator();
    ImGui::Text("Revision: %s", URGE_GIT_REVISION);
    ImGui::Text("Build: %s", URGE_BUILD_DATE);
    ImGui::Separator();

    if (ImGui::Button("Website"))
      SDL_OpenURL("https://urge.admenri.com/");
    if (ImGui::Button("Github"))
      SDL_OpenURL("https://github.com/Admenri/urge");
  }
}

}  // namespace

ContentRunner::ContentRunner(ContentProfile* profile,
                             std::unique_ptr<EngineBindingBase> binding)
    : profile_(profile),
      binding_(std::move(binding)),
      binding_quit_flag_(0),
      binding_reset_flag_(0),
      background_running_(false),
      show_settings_menu_(false),
      show_fps_monitor_(false),
      last_tick_(SDL_GetPerformanceCounter()),
      total_delta_(0),
      frame_count_(0),
#if defined(OS_EMSCRIPTEN)
      elapsed_time_(0.0),
      smooth_delta_time_(1.0),
      last_count_time_(SDL_GetPerformanceCounter()),
#endif  //! OS_EMSCRIPTEN
      screen_bound_() {
}

ContentRunner::~ContentRunner() {
  // Remove watch
  SDL_RemoveEventWatch(&ContentRunner::EventWatchHandlerInternal, this);

  // Destory GUI context
  DestroyIMGUIContextInternal();
}

void ContentRunner::RunMainLoop() {
#if defined(OS_EMSCRIPTEN)
  using EmscriptenClosure = std::function<void()>;
  auto main_loop_proc = EmscriptenClosure([this]() {
#endif  // !OS_EMSCRIPTEN
    // Make module context
    EngineBindingBase::ScopedModuleContext module_context;
    module_context.graphics = graphics_impl_.get();
    module_context.input = keyboard_impl_.get();
    module_context.audio = audio_impl_.get();
    module_context.mouse = mouse_impl_.get();
    module_context.engine = engine_impl_.get();

    // Binding lifecycle
    binding_->PreEarlyInitialization(execution_context_->engine_profile,
                                     execution_context_->io_service);
    binding_->OnMainMessageLoopRun(execution_context_.get(), &module_context);
    binding_->PostMainLoopRunning();

#if defined(OS_EMSCRIPTEN)
    // End of emscripten looping
    emscripten_cancel_main_loop();
    emscripten_fiber_swap(&main_loop_fiber_, &primary_fiber_);
  });

  // Create fiber for emscripten
  emscripten_fiber_init_from_current_context(&primary_fiber_,
                                             primary_asyncify_stack_,
                                             sizeof(primary_asyncify_stack_));
  emscripten_fiber_init(
      &main_loop_fiber_,
      [](void* proc) {
        auto* lambda_proc = static_cast<EmscriptenClosure*>(proc);
        (*lambda_proc)();
      },
      &main_loop_proc, main_stack_, sizeof(main_stack_), main_asyncify_stack_,
      sizeof(main_asyncify_stack_));

  auto execute_main_loop = EmscriptenClosure([this]() {
    // Pause rendering when enter background on emscripten
    if (background_running_)
      return;

    // Determine update repeat time
    const uint64_t now_time = SDL_GetPerformanceCounter();
    const uint64_t delta_time = now_time - last_count_time_;
    last_count_time_ = now_time;

    // Desired frame time
    const double desired_delta_time =
        SDL_GetPerformanceFrequency() / graphics_impl_->FrameRate();

    // Discard frames if need
    if (delta_time > desired_delta_time * 2) {
      elapsed_time_ = 0.0;
      smooth_delta_time_ = 1.0;
    }

    // Calculate smooth frame rate
    const double delta_rate =
        delta_time / static_cast<double>(desired_delta_time);
    const int32_t repeat_time = DetermineRepeatNumberInternal(delta_rate);

    for (int32_t i = 0; i < repeat_time; ++i)
      emscripten_fiber_swap(&primary_fiber_, &main_loop_fiber_);

    // Update audio thread
    if (audio_server_)
      audio_impl_->MeThreadMonitorInternal();
  });

  // Start main loop
  emscripten_set_main_loop_arg(
      [](void* proc) {
        auto* lambda_proc = static_cast<EmscriptenClosure*>(proc);
        (*lambda_proc)();
      },
      &execute_main_loop, 0, true);
#endif  // !OS_EMSCRIPTEN
}

std::unique_ptr<ContentRunner> ContentRunner::Create(InitParams params) {
  auto* runner = new ContentRunner(params.profile, std::move(params.entry));
  if (!runner->InitializeComponents(params.io_service, params.font_context,
                                    params.i18n_profile, params.window))
    return nullptr;

  return std::unique_ptr<ContentRunner>(runner);
}

bool ContentRunner::InitializeComponents(filesystem::IOService* io_service,
                                         ScopedFontData* font_context,
                                         I18NProfile* i18n_profile,
                                         base::WeakPtr<ui::Widget> window) {
  // Initialize components
  auto [render_device, render_context] = renderer::RenderDevice::Create(
      window,
      magic_enum::enum_cast<renderer::DriverType>(profile_->driver_backend)
          .value_or(renderer::DriverType::UNDEFINED),
      profile_->render_validation);
  if (!render_device || !render_context) {
    LOG(ERROR) << "[Content] Error when creating video device.";
    return false;
  }

  render_device_ = std::move(render_device);
  device_context_ = std::move(render_context);

  // Initialize execution context
  execution_context_ = std::make_unique<ExecutionContext>();
  execution_context_->resolution = profile_->resolution;
  execution_context_->window = window;
  execution_context_->render_device = render_device_.get();
  execution_context_->primary_render_context = device_context_;

  // Rendering components
  CreateRenderComponents();

  canvas_scheduler_ = std::make_unique<CanvasScheduler>(
      render_device_.get(), device_context_,
      execution_context_->render.pipeline_loader.get());
  sprite_batcher_ = std::make_unique<SpriteBatch>(
      render_device_.get(), execution_context_->render.pipeline_loader.get());
  event_controller_ = std::make_unique<EventController>();
  audio_server_ = audioservice::AudioService::Create(io_service);
#if !defined(OS_EMSCRIPTEN)
  network_service_ = network::NetworkService::Create();
#endif

  // System components
  execution_context_->engine_profile = profile_;
  execution_context_->font_context = font_context;
  execution_context_->i18n_profile = i18n_profile;
  execution_context_->io_service = io_service;
  execution_context_->canvas_scheduler = canvas_scheduler_.get();
  execution_context_->sprite_batcher = sprite_batcher_.get();
  execution_context_->event_controller = event_controller_.get();
  execution_context_->audio_server = audio_server_.get();
#if !defined(OS_EMSCRIPTEN)
  execution_context_->network_service = network_service_.get();
#endif

  // Create engine objects
  engine_impl_ = base::MakeRefCounted<EngineImpl>(execution_context_.get());
  execution_context_->disposable_parent = engine_impl_.get();

  // Create and hook graphics event loop
  graphics_impl_ = base::MakeRefCounted<RenderScreenImpl>(
      execution_context_.get(), profile_->frame_rate);
  graphics_impl_->SetupTicker(base::BindRepeating(
      &ContentRunner::TickHandlerInternal, base::Unretained(this)));
  execution_context_->screen_drawable_node =
      graphics_impl_->GetDrawableController();

  // Other modules
  keyboard_impl_ =
      base::MakeRefCounted<KeyboardControllerImpl>(execution_context_.get());
  audio_impl_ = base::MakeRefCounted<AudioImpl>(execution_context_.get());
  mouse_impl_ = base::MakeRefCounted<MouseImpl>(execution_context_.get());

  // Create imgui context
  CreateIMGUIContextInternal();

  // Initialize viewport
  UpdateWindowViewportInternal();

  // Background watch
  if (!SDL_AddEventWatch(&ContentRunner::EventWatchHandlerInternal, this)) {
    LOG(ERROR) << "[Engine] " << SDL_GetError();
    return false;
  }

  LOG(INFO) << "[Engine] Git Revision: " << URGE_GIT_REVISION;
  LOG(INFO) << "[Engine] Build Date: " << URGE_BUILD_DATE;

  return true;
}

void ContentRunner::CreateRenderComponents() {
  // Generic quad index cache
  execution_context_->render.quad_index =
      std::make_unique<renderer::QuadIndexCache>(
          **execution_context_->render_device,
          profile_->u32_draw_index ? Diligent::VT_UINT32 : Diligent::VT_UINT16);
  execution_context_->render.quad_index->Allocate(1 << 10);

  // Pipeline loaders
  renderer::PipelineInitParams pipeline_params;
  pipeline_params.render_device = **execution_context_->render_device;

  // Default sampler
  auto sampler_filter =
      static_cast<Diligent::FILTER_TYPE>(profile_->pipeline_default_sampler);
  if (sampler_filter == Diligent::FILTER_TYPE_UNKNOWN)
    sampler_filter = Diligent::FILTER_TYPE_POINT;

  pipeline_params.immutable_sampler =
      Diligent::SamplerDesc{sampler_filter,
                            sampler_filter,
                            sampler_filter,
                            Diligent::TEXTURE_ADDRESS_CLAMP,
                            Diligent::TEXTURE_ADDRESS_CLAMP,
                            Diligent::TEXTURE_ADDRESS_CLAMP};
  execution_context_->render.pipeline_loader =
      std::make_unique<renderer::PipelineSet>(pipeline_params);

  // Pipeline states
  auto* loader = execution_context_->render.pipeline_loader.get();
  execution_context_->render.pipeline_states =
      std::make_unique<PipelineCollection>(loader);
}

void ContentRunner::TickHandlerInternal(Diligent::ITexture* present_buffer) {
  // Poll event
  UpdateEventInternal();

#if !defined(OS_EMSCRIPTEN)
  // Update network service queue
  network_service_->DispatchEvent();
#endif

  // Increase frame step
  frame_count_++;

  // Update fps
  UpdateDisplayFPSInternal();

  // Update swapchain & viewport
  UpdateWindowViewportInternal();

  // Render engine primitive UI
  RenderGUIInternal(present_buffer);

  // Present screen buffer
  graphics_impl_->PresentScreenBuffer(imgui_.get());

  // Dispatch flag message
  if (binding_quit_flag_.load()) {
    binding_quit_flag_.store(0);
    binding_->ExitSignalRequired();
  } else if (binding_reset_flag_.load()) {
    binding_reset_flag_.store(0);
    binding_->ResetSignalRequired();
  }

#if defined(OS_EMSCRIPTEN)
  // Switch to primary fiber
  emscripten_fiber_swap(&main_loop_fiber_, &primary_fiber_);
#endif  //! OS_EMSCRIPTEN
}

void ContentRunner::UpdateDisplayFPSInternal() {
  const uint64_t current_tick = SDL_GetPerformanceCounter();
  const int64_t delta_tick = current_tick - last_tick_;
  last_tick_ = current_tick;

  total_delta_ += delta_tick;
  const uint64_t timer_freq = SDL_GetPerformanceFrequency();
  if (total_delta_ >= static_cast<int64_t>(timer_freq)) {
    const float fps_scale = static_cast<float>(
        SDL_GetPerformanceFrequency() / static_cast<float>(total_delta_));
    const float current_fps = static_cast<float>(frame_count_) * fps_scale;

    total_delta_ = 0;
    frame_count_ = 0;

    fps_history_.push_back(current_fps);
    if (fps_history_.size() > 20)
      fps_history_.erase(fps_history_.begin());
  }
}

void ContentRunner::UpdateWindowViewportInternal() {
  auto& window = execution_context_->window;
  const auto window_size = window->GetSize();
  auto* swapchain = render_device_->GetSwapChain();

  if (window_size.x != static_cast<int32_t>(swapchain->GetDesc().Width) ||
      window_size.y != static_cast<int32_t>(swapchain->GetDesc().Height)) {
    // Resize screen surface
    swapchain->Resize(window_size.x, window_size.y,
                      Diligent::SURFACE_TRANSFORM_OPTIMAL);
  }
}

void ContentRunner::RenderGUIInternal(Diligent::ITexture* present_buffer) {
  // Setup renderer new frame
  const Diligent::SwapChainDesc& swapchain_desc =
      execution_context_->render_device->GetSwapChain()->GetDesc();
  imgui_->NewFrame(swapchain_desc.Width, swapchain_desc.Height,
                   swapchain_desc.PreTransform);

  // Setup sdl new frame
  ImGui_ImplSDL3_NewFrame();

  // Setup imgui new frame
  ImGui::NewFrame();

  // Render settings menu
  if (show_settings_menu_)
    RenderSettingsGUIInternal();

  // Render fps monitor
  if (show_fps_monitor_)
    RenderFPSMonitorGUIInternal();

  // Present game window
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(swapchain_desc.Width, swapchain_desc.Height));

  // Primary viewport
  bool process_game_input = false;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  if (ImGui::Begin("Game", nullptr,
                   ImGuiWindowFlags_NoBackground |
                       ImGuiWindowFlags_NoDecoration |
                       ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoBringToFrontOnFocus)) {
    auto client_max = ImGui::GetWindowContentRegionMax();
    auto client_min = ImGui::GetWindowContentRegionMin();

    // Update real display viewport for game screen
    const auto window_size =
        base::Vec2i(client_max.x - client_min.x, client_max.y - client_min.y);
    const auto screen_size = execution_context_->resolution;

    const float window_ratio =
        static_cast<float>(window_size.x) / window_size.y;
    const float screen_ratio =
        static_cast<float>(screen_size.x) / screen_size.y;

    ImVec2 display_size(window_size.x, window_size.y);
    if (screen_ratio > window_ratio)
      display_size.y = display_size.x / screen_ratio;
    if (screen_ratio < window_ratio)
      display_size.x = display_size.y * screen_ratio;

    ImVec2 display_pos((window_size.x - display_size.x) / 2.0f,
                       (window_size.y - display_size.y) / 2.0f);
    display_pos.x += client_min.x;
    display_pos.y += client_min.y;

    // Viewer image component
    auto* screen_image_view =
        present_buffer->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    auto tex_id = reinterpret_cast<ImTextureID>(screen_image_view);
    ImGui::SetCursorPos(display_pos);
    ImGui::Image(tex_id, display_size);

    // Input region
    const auto widget_min = ImGui::GetItemRectMin();
    const auto widget_max = ImGui::GetItemRectMax();
    screen_bound_.x = widget_min.x;
    screen_bound_.y = widget_min.y;
    screen_bound_.width = widget_max.x - widget_min.x;
    screen_bound_.height = widget_max.y - widget_min.y;

    process_game_input = ImGui::IsWindowHovered();
  }
  ImGui::End();
  ImGui::PopStyleVar(3);

  // Enable input dispatch for input & mouse
  if (process_game_input) {
    for (auto& it : event_controller_->key_events())
      keyboard_impl_->ProcessEvent(it);
    for (auto& it : event_controller_->mouse_events())
      mouse_impl_->ProcessEvent(it);
  } else {
    keyboard_impl_->ProcessEvent(std::nullopt);
    mouse_impl_->ProcessEvent(std::nullopt);
  }

  // Render gui
  ImGui::Render();
}

void ContentRunner::RenderSettingsGUIInternal() {
  ImGui::SetNextWindowPos(ImVec2(), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(execution_context_->i18n_profile
                       ->GetI18NString(IDS_MENU_SETTINGS, "Settings")
                       .c_str())) {
    // Graphics settings
    graphics_impl_->CreateButtonGUISettings();

    // Audio settings
    audio_impl_->CreateButtonGUISettings();

    // Engine Info
    DrawEngineInfoGUI(execution_context_->i18n_profile);
  }
  ImGui::End();
}

void ContentRunner::RenderFPSMonitorGUIInternal() {
  if (ImGui::Begin("FPS Monitor")) {
    // Draw current fps
    double current_fps = fps_history_.back();
    ImGui::Text("FPS: %.2f", current_fps);
    // Draw plot for fps monitor
    ImGui::PlotHistogram("##FPSDisplay", fps_history_.data(),
                         static_cast<int32_t>(fps_history_.size()), 0, nullptr,
                         0.0f, FLT_MAX, ImVec2(300, 80));
  }
  ImGui::End();
}

void ContentRunner::UpdateEventInternal() {
  // Clear pending events
  event_controller_->ClearPendingEvents();

  // Poll event queue
  SDL_Event queued_event;
  while (SDL_PollEvent(&queued_event)
#if !defined(OS_EMSCRIPTEN)
         || background_running_
#endif  //! OS_EMSCRIPTEN
  ) {
    // Quit event
    if (queued_event.type == SDL_EVENT_QUIT) {
      binding_quit_flag_.store(1);
      break;
    }

    // GUI event process
    ImGui_ImplSDL3_ProcessEvent(&queued_event);

    // Shortcut process
    if (queued_event.type == SDL_EVENT_KEY_UP &&
        queued_event.key.windowID ==
            execution_context_->window->GetWindowID()) {
      if (queued_event.key.scancode == SDL_SCANCODE_F1) {
        show_settings_menu_ =
            !show_settings_menu_ && !profile_->disable_settings;
      } else if (queued_event.key.scancode == SDL_SCANCODE_F2) {
        show_fps_monitor_ =
            !show_fps_monitor_ && !profile_->disable_fps_monitor;
      } else if (queued_event.key.scancode == SDL_SCANCODE_F12) {
        if (!profile_->disable_reset)
          binding_reset_flag_.store(1);
      }
    }

    // Game input process
    const auto window_size = execution_context_->window->GetSize();
    const auto screen_size = execution_context_->resolution;
    event_controller_->DispatchEvent(&queued_event, window_size, screen_size,
                                     screen_bound_);
  }
}

bool ContentRunner::EventWatchHandlerInternal(void* userdata,
                                              SDL_Event* event) {
  ContentRunner* self = static_cast<ContentRunner*>(userdata);
#if !defined(OS_ANDROID)
  if (self->profile_->background_running)
    return true;
#endif

  const bool is_focus_lost =
#if !defined(OS_ANDROID)
      event->type == SDL_EVENT_WINDOW_FOCUS_LOST;
#else
      event->type == SDL_EVENT_WILL_ENTER_BACKGROUND;
#endif
  const bool is_focus_gained =
#if !defined(OS_ANDROID)
      event->type == SDL_EVENT_WINDOW_FOCUS_GAINED;
#else
      event->type == SDL_EVENT_DID_ENTER_FOREGROUND;
#endif

  if (is_focus_lost) {
    LOG(INFO) << "[Content] Enter background running.";
    if (self->audio_server_)
      self->audio_server_->PauseDevice();
    self->execution_context_->render_device->SuspendContext();
    self->background_running_ = true;
  } else if (is_focus_gained) {
    LOG(INFO) << "[Content] Resume foreground running.";
    self->execution_context_->render_device->ResumeContext(
        self->execution_context_->primary_render_context);
    if (self->audio_server_)
      self->audio_server_->ResumeDevice();
    self->graphics_impl_->ResetFPSCounter();
    self->background_running_ = false;

#if defined(OS_EMSCRIPTEN)
    LOG(INFO) << "[Emscripten] Reset elapsed frame time.";
    self->elapsed_time_ = 0.0;
    self->smooth_delta_time_ = 1.0;
    self->last_count_time_ = SDL_GetPerformanceCounter();
#endif  // !OS_EMSCRIPTEN
  }

  return true;
}

void ContentRunner::CreateIMGUIContextInternal() {
  auto* render_device = execution_context_->render_device;

  // Derive atlas limit info
  const auto& adapter_info = (*render_device)->GetAdapterInfo();
  uint32_t max_texture_size = adapter_info.Texture.MaxTexture2DDimension;

  // Setup context
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  // Apply DPI Settings
  int32_t display_w, display_h;
  SDL_GetWindowSizeInPixels(execution_context_->window->AsSDLWindow(),
                            &display_w, &display_h);
  io.DisplaySize =
      ImVec2(static_cast<float>(display_w), static_cast<float>(display_h));
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

  const float window_scale =
      SDL_GetWindowDisplayScale(execution_context_->window->AsSDLWindow());
  ImGui::GetStyle().ScaleAllSizes(window_scale);

  // Apply default font
  int64_t font_data_size;
  auto* font_data =
      execution_context_->font_context->GetUIDefaultFont(&font_data_size);

  ImFontConfig font_config;
  font_config.FontDataOwnedByAtlas = false;
  io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
  io.Fonts->TexMaxWidth = max_texture_size;
  io.Fonts->TexMaxHeight = max_texture_size;
  io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(font_data), font_data_size,
                                 16.0f * window_scale, &font_config,
                                 io.Fonts->GetGlyphRangesChineseFull());

  // Setup Dear ImGui style
  ImGui::StyleColorsClassic();

  // Setup imgui platform backends
  ImGui_ImplSDL3_InitForOther(execution_context_->window->AsSDLWindow());

  // Setup renderer backend
  Diligent::ImGuiDiligentCreateInfo imgui_create_info(
      **render_device, render_device->GetSwapChain()->GetDesc());
  imgui_create_info.MinFilter = profile_->smooth_scale_present
                                    ? Diligent::FILTER_TYPE_LINEAR
                                    : Diligent::FILTER_TYPE_POINT;
  imgui_create_info.MagFilter = imgui_create_info.MinFilter;
  imgui_create_info.MipFilter = imgui_create_info.MagFilter;
  imgui_ = std::make_unique<Diligent::ImGuiDiligentRenderer>(imgui_create_info);
}

void ContentRunner::DestroyIMGUIContextInternal() {
  imgui_.reset();
  ImGui::DestroyPlatformWindows();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

#if defined(OS_EMSCRIPTEN)
int32_t ContentRunner::DetermineRepeatNumberInternal(double delta_rate) {
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
#endif  // !OS_EMSCRIPTEN

}  // namespace content

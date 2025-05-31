// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/content_runner.h"

#include "imgui/backends/imgui_impl_sdl3.h"
#include "imgui/imgui.h"

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

    if (ImGui::Button("Website"))
      SDL_OpenURL("https://urge.admenri.com/");
    if (ImGui::Button("Github"))
      SDL_OpenURL("https://github.com/Admenri/urge");
  }
}

}  // namespace

ContentRunner::ContentRunner(ContentProfile* profile,
                             filesystem::IOService* io_service,
                             ScopedFontData* font_context,
                             I18NProfile* i18n_profile,
                             base::WeakPtr<ui::Widget> window,
                             base::ThreadWorker* render_worker,
                             std::unique_ptr<EngineBindingBase> binding)
    : profile_(profile),
      io_service_(io_service),
      font_context_(font_context),
      i18n_profile_(i18n_profile),
      window_(window),
      render_worker_(render_worker),
      binding_(std::move(binding)),
      binding_quit_flag_(0),
      binding_reset_flag_(0),
      background_running_(false),
      disable_gui_input_(false),
      show_settings_menu_(false),
      show_fps_monitor_(false),
      last_tick_(SDL_GetPerformanceCounter()),
      total_delta_(0),
      frame_count_(0) {
  // Create engine objects
  graphics_impl_ = base::MakeRefCounted<RenderScreenImpl>(
      window, render_worker, profile, i18n_profile, io_service, font_context,
      profile->resolution, profile->frame_rate);
  keyboard_impl_ = base::MakeRefCounted<KeyboardControllerImpl>(window, profile,
                                                                i18n_profile);
  audio_impl_ =
      base::MakeRefCounted<AudioImpl>(profile, io_service, i18n_profile);
  mouse_impl_ = base::MakeRefCounted<MouseImpl>(window);
  engine_impl_ = base::MakeRefCounted<MiscSystem>(window, io_service);

  // Create event router
  event_controller_.reset(new EventController(window));

  // Create imgui context
  base::ThreadWorker::PostTask(
      render_worker_, base::BindOnce(&ContentRunner::CreateIMGUIContextInternal,
                                     base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);

  // Hook graphics event loop
  tick_observer_ = graphics_impl_->AddTickObserver(base::BindRepeating(
      &ContentRunner::TickHandlerInternal, base::Unretained(this)));

  // Background watch
  SDL_AddEventWatch(&ContentRunner::EventWatchHandlerInternal, this);
}

ContentRunner::~ContentRunner() {
  // Remove watch
  SDL_RemoveEventWatch(&ContentRunner::EventWatchHandlerInternal, this);

  // Destory GUI context
  base::ThreadWorker::PostTask(
      render_worker_,
      base::BindOnce(&ContentRunner::DestroyIMGUIContextInternal,
                     base::Unretained(this)));
  base::ThreadWorker::WaitWorkerSynchronize(render_worker_);
}

void ContentRunner::RunMainLoop() {
  // Before running loop handler
  binding_->PreEarlyInitialization(profile_, io_service_);

  // Make script binding execution context
  // Call binding boot handler before running loop handler
  ExecutionContext execution_context;
  execution_context.font_context = font_context_;
  execution_context.event_controller = event_controller_.get();
  execution_context.canvas_scheduler = graphics_impl_->GetCanvasScheduler();
  execution_context.graphics = graphics_impl_.get();
  execution_context.io_service = io_service_;

  // Make module context
  EngineBindingBase::ScopedModuleContext module_context;
  module_context.graphics = graphics_impl_.get();
  module_context.input = keyboard_impl_.get();
  module_context.audio = audio_impl_.get();
  module_context.mouse = mouse_impl_.get();
  module_context.engine = engine_impl_.get();

  // Execute main loop
  binding_->OnMainMessageLoopRun(&execution_context, &module_context);

  // End of running
  binding_->PostMainLoopRunning();
}

std::unique_ptr<ContentRunner> ContentRunner::Create(InitParams params) {
  auto* runner =
      new ContentRunner(params.profile, params.io_service, params.font_context,
                        params.i18n_profile, params.window,
                        params.render_worker, std::move(params.entry));
  return std::unique_ptr<ContentRunner>(runner);
}

void ContentRunner::TickHandlerInternal() {
  frame_count_++;

  // Update fps
  UpdateDisplayFPSInternal();

  // Render GUI if need
  bool handle_event = !RenderGUIInternal();

  // Poll event queue
  SDL_Event queued_event;
  while (SDL_PollEvent(&queued_event)) {
    // Quit event
    if (queued_event.type == SDL_EVENT_QUIT)
      binding_quit_flag_.store(1);

    // GUI event process
    if (!disable_gui_input_)
      ImGui_ImplSDL3_ProcessEvent(&queued_event);

    // Shortcut
    if (queued_event.type == SDL_EVENT_KEY_UP &&
        queued_event.key.windowID == window_->GetWindowID()) {
      if (queued_event.key.scancode == SDL_SCANCODE_F1) {
        show_settings_menu_ = !show_settings_menu_;
      } else if (queued_event.key.scancode == SDL_SCANCODE_F2) {
        show_fps_monitor_ = !show_fps_monitor_;
      } else if (queued_event.key.scancode == SDL_SCANCODE_F12) {
        binding_reset_flag_.store(1);
      }
    }

    // Widget event
    if (handle_event) {
      window_->DispatchEvent(&queued_event);
      event_controller_->DispatchEvent(&queued_event);
    }
  }

  // Wait for background running block
  while (background_running_) {
    SDL_WaitEvent(&queued_event);
    event_controller_->DispatchEvent(&queued_event);
  }

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

bool ContentRunner::RenderGUIInternal() {
  bool window_hovered = false;

  // Setup renderer new frame
  const Diligent::SwapChainDesc& swapchain_desc =
      graphics_impl_->GetDevice()->GetSwapChain()->GetDesc();
  imgui_->NewFrame(swapchain_desc.Width, swapchain_desc.Height,
                   swapchain_desc.PreTransform);

  // Setup sdl new frame
  ImGui_ImplSDL3_NewFrame();

  // Setup imgui new frame
  ImGui::NewFrame();

  // Render settings menu
  if (show_settings_menu_)
    window_hovered |= RenderSettingsGUIInternal();

  // Render fps monitor
  if (show_fps_monitor_)
    RenderFPSMonitorGUIInternal();

  // Final gui render
  ImGui::Render();

  return window_hovered;
}

bool ContentRunner::RenderSettingsGUIInternal() {
  ImGui::SetNextWindowPos(ImVec2(), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);

  bool window_hovered = false;
  if (ImGui::Begin(i18n_profile_->GetI18NString(IDS_MENU_SETTINGS, "Settings")
                       .c_str())) {
    window_hovered = ImGui::IsWindowHovered();

    // Button settings
    disable_gui_input_ = keyboard_impl_->CreateButtonGUISettings();

    // Graphics settings
    graphics_impl_->CreateButtonGUISettings();

    // Audio settings
    audio_impl_->CreateButtonGUISettings();

    // Engine Info
    DrawEngineInfoGUI(i18n_profile_);
  }

  // End window create
  ImGui::End();

  return window_hovered;
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

  // End window render
  ImGui::End();
}

bool ContentRunner::EventWatchHandlerInternal(void* userdata,
                                              SDL_Event* event) {
  ContentRunner* self = static_cast<ContentRunner*>(userdata);

#if !defined(OS_ANDROID)
  if (self->graphics_impl_->AllowBackgroundRunning())
    return true;
#endif

  const bool is_focus_lost =
#if defined(OS_ANDROID)
      event->type == SDL_EVENT_WINDOW_FOCUS_LOST;
#else
      event->type == SDL_EVENT_WILL_ENTER_BACKGROUND;
#endif
  const bool is_focus_gained =
#if defined(OS_ANDROID)
      event->type == SDL_EVENT_WINDOW_FOCUS_GAINED;
#else
      event->type == SDL_EVENT_DID_ENTER_FOREGROUND;
#endif

  if (is_focus_lost) {
    LOG(INFO) << "[Content] Enter background running.";
    self->graphics_impl_->SuspendRenderingContext();
    self->background_running_ = true;
    return false;
  }
  if (is_focus_gained) {
    LOG(INFO) << "[Content] Resume foreground running.";
    self->graphics_impl_->ResumeRenderingContext();
    self->background_running_ = false;
    return false;
  }

  return true;
}

void ContentRunner::CreateIMGUIContextInternal() {
  auto* render_device = graphics_impl_->GetDevice();

  // Derive atlas limit info
  const auto& adapter_info = (*render_device)->GetAdapterInfo();
  uint32_t max_texture_size = adapter_info.Texture.MaxTexture2DDimension;

  // Setup context
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |=
      ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

  // Apply DPI Settings
  int32_t display_w, display_h;
  SDL_GetWindowSizeInPixels(window_->AsSDLWindow(), &display_w, &display_h);
  io.DisplaySize =
      ImVec2(static_cast<float>(display_w), static_cast<float>(display_h));
  io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
  float window_scale = SDL_GetWindowDisplayScale(window_->AsSDLWindow());
  ImGui::GetStyle().ScaleAllSizes(window_scale);

  // Apply default font
  int64_t font_data_size;
  auto* font_data = font_context_->GetUIDefaultFont(&font_data_size);

  ImFontConfig font_config;
  font_config.FontDataOwnedByAtlas = false;
  io.Fonts->Flags |= ImFontAtlasFlags_NoPowerOfTwoHeight;
  io.Fonts->TexDesiredWidth = max_texture_size;
  io.Fonts->AddFontFromMemoryTTF(const_cast<void*>(font_data), font_data_size,
                                 16.0f * window_scale, &font_config,
                                 io.Fonts->GetGlyphRangesChineseFull());

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  // Setup imgui platform backends
  ImGui_ImplSDL3_InitForOther(window_->AsSDLWindow());

  // Setup renderer backend
  Diligent::ImGuiDiligentCreateInfo imgui_create_info(
      **render_device,
      render_device->GetSwapChain()->GetDesc().ColorBufferFormat,
      Diligent::TEX_FORMAT_UNKNOWN);
  imgui_.reset(new Diligent::ImGuiDiligentRenderer(imgui_create_info));
}

void ContentRunner::DestroyIMGUIContextInternal() {
  imgui_.reset();

  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

}  // namespace content

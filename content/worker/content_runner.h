// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_CONTENT_RUNNER_H_
#define CONTENT_WORKER_CONTENT_RUNNER_H_

#include "imgui/backends/ImGuiDiligentRenderer.hpp"

#include "base/worker/thread_worker.h"
#include "components/filesystem/io_service.h"
#include "content/canvas/font_context.h"
#include "content/input/keyboard_controller.h"
#include "content/input/mouse_controller.h"
#include "content/media/audio_impl.h"
#include "content/misc/misc_system.h"
#include "content/profile/content_profile.h"
#include "content/profile/i18n_profile.h"
#include "content/screen/renderscreen_impl.h"
#include "content/worker/engine_binding.h"
#include "content/worker/event_controller.h"
#include "ui/widget/widget.h"

namespace content {

class ContentRunner {
 public:
  struct InitParams {
    // Engine profile configure,
    // require a configure unique object.
    ContentProfile* profile;

    // Runner filesystem service.
    filesystem::IOService* io_service;

    // Global font context
    ScopedFontData* font_context;

    // i18n profile data set
    I18NProfile* i18n_profile;

    // Graphics renderer target.
    base::WeakPtr<ui::Widget> window;

    // Binding boot entry,
    // require an unique one.
    std::unique_ptr<EngineBindingBase> entry;
  };

  ~ContentRunner();

  ContentRunner(const ContentRunner&) = delete;
  ContentRunner& operator=(const ContentRunner&) = delete;

  static std::unique_ptr<ContentRunner> Create(InitParams params);

  void RunMainLoop();

 private:
  ContentRunner(ContentProfile* profile,
                std::unique_ptr<EngineBindingBase> binding);
  bool InitializeComponents(filesystem::IOService* io_service,
                            ScopedFontData* font_context,
                            I18NProfile* i18n_profile,
                            base::WeakPtr<ui::Widget> window);
  void TickHandlerInternal();
  void UpdateDisplayFPSInternal();
  bool RenderGUIInternal();
  bool RenderSettingsGUIInternal();
  void RenderFPSMonitorGUIInternal();

  static bool EventWatchHandlerInternal(void* userdata, SDL_Event* event);

  void CreateIMGUIContextInternal();
  void DestroyIMGUIContextInternal();

  ContentProfile* profile_;
  std::unique_ptr<ExecutionContext> execution_context_;
  std::unique_ptr<EngineBindingBase> binding_;

  std::unique_ptr<renderer::RenderDevice> render_device_;
  RRefPtr<Diligent::IDeviceContext> device_context_;
  std::unique_ptr<CanvasScheduler> canvas_scheduler_;
  std::unique_ptr<SpriteBatch> sprite_batcher_;
  std::unique_ptr<EventController> event_controller_;
  std::unique_ptr<Diligent::ImGuiDiligentRenderer> imgui_;
  std::unique_ptr<audioservice::AudioService> audio_server_;
  std::unique_ptr<network::NetworkService> network_service_;

  scoped_refptr<RenderScreenImpl> graphics_impl_;
  scoped_refptr<KeyboardControllerImpl> keyboard_impl_;
  scoped_refptr<AudioImpl> audio_impl_;
  scoped_refptr<MouseImpl> mouse_impl_;
  scoped_refptr<MiscSystem> engine_impl_;

  std::atomic<int32_t> binding_quit_flag_;
  std::atomic<int32_t> binding_reset_flag_;

  bool background_running_;
  bool disable_gui_input_;
  bool show_settings_menu_;
  bool show_fps_monitor_;

  uint64_t last_tick_;
  int64_t total_delta_;
  int32_t frame_count_;
  std::vector<float> fps_history_;
};

}  // namespace content

#endif  //! CONTENT_WORKER_CONTENT_RUNNER_H_

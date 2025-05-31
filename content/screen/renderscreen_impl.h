// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SCREEN_RENDERSCREEN_IMPL_H_
#define CONTENT_SCREEN_RENDERSCREEN_IMPL_H_

#include "backends/ImGuiDiligentRenderer.hpp"

#include "base/worker/thread_worker.h"
#include "components/fpslimiter/fpslimiter.h"
#include "content/canvas/canvas_scheduler.h"
#include "content/canvas/font_context.h"
#include "content/context/disposable.h"
#include "content/profile/content_profile.h"
#include "content/profile/i18n_profile.h"
#include "content/public/engine_graphics.h"
#include "content/render/drawable_controller.h"
#include "content/render/sprite_batch.h"
#include "renderer/device/render_device.h"
#include "renderer/layout/uniform_layout.h"

namespace content {

struct RenderGraphicsAgent {
  base::OwnedPtr<renderer::RenderDevice> device;
  base::OwnedPtr<renderer::RenderContext> context;

  base::OwnedPtr<CanvasScheduler> canvas_scheduler;
  base::OwnedPtr<SpriteBatch> sprite_batch;

  RRefPtr<Diligent::ITexture> screen_buffer;
  RRefPtr<Diligent::ITexture> frozen_buffer;
  RRefPtr<Diligent::ITexture> transition_buffer;
  RRefPtr<Diligent::IBuffer> root_transform;
  RRefPtr<Diligent::IBuffer> world_transform;

  base::OwnedPtr<renderer::QuadBatch> effect_quads;
  base::OwnedPtr<renderer::Binding_Color> effect_binding;

  base::OwnedPtr<renderer::QuadBatch> transition_quads;
  base::OwnedPtr<renderer::Binding_AlphaTrans> transition_binding_alpha;
  base::OwnedPtr<renderer::Binding_VagueTrans> transition_binding_vague;

  Diligent::ITexture** present_target = nullptr;
  base::OwnedPtr<renderer::Pipeline_Present> present_pipeline;

  struct {
    base::String device;
    base::String vendor;
    base::String description;
  } renderer_info;
};

class GraphicsChild {
 public:
  GraphicsChild(RenderScreenImpl* screen) : screen_(screen) {}
  virtual ~GraphicsChild() = default;

  GraphicsChild(const GraphicsChild&) = delete;
  GraphicsChild& operator=(const GraphicsChild&) = delete;

  RenderScreenImpl* screen() const { return screen_; }

 private:
  RenderScreenImpl* screen_;
};

class RenderScreenImpl : public Graphics, public DisposableCollection {
 public:
  RenderScreenImpl(base::WeakPtr<ui::Widget> window,
                   base::ThreadWorker* render_worker,
                   ContentProfile* profile,
                   I18NProfile* i18n_profile,
                   filesystem::IOService* io_service,
                   ScopedFontData* scoped_font,
                   const base::Vec2i& resolution,
                   uint32_t frame_rate);
  ~RenderScreenImpl() override;

  RenderScreenImpl(const RenderScreenImpl&) = delete;
  RenderScreenImpl& operator=(const RenderScreenImpl&) = delete;

  // Present current screen buffer to window.
  // This function will wait for delta time to clamp fps.
  void PresentScreenBuffer(Diligent::ImGuiDiligentRenderer* gui_renderer);

  // Hungup rendering context if system enter background,
  // only calling on Android system
  void SuspendRenderingContext();
  void ResumeRenderingContext();

  void CreateButtonGUISettings();

  DrawNodeController* GetDrawableController() { return &controller_; }
  base::ThreadWorker* GetRenderRunner() const { return render_worker_; }
  base::Vec2i GetResolution() const { return resolution_; }

  renderer::RenderDevice* GetDevice() const { return agent_->device.get(); }
  renderer::RenderContext* GetContext() const { return agent_->context.get(); }
  SpriteBatch* GetSpriteBatch() const { return agent_->sprite_batch.get(); }
  ScopedFontData* GetScopedFontContext() const { return scoped_font_; }
  CanvasScheduler* GetCanvasScheduler() const {
    return agent_->canvas_scheduler.get();
  }

  ContentProfile::APIVersion GetAPIVersion() const {
    return profile_->api_version;
  }

  // Add tick monitor handler
  base::CallbackListSubscription AddTickObserver(
      const base::RepeatingClosure& handler);

  // DisposableCollection methods
  void AddDisposable(Disposable* disp) override;

  void PostTask(base::OnceClosure task);
  void WaitWorkerSynchronize();

  bool AllowBackgroundRunning() const { return background_running_; }

 public:
  void Update(ExceptionState& exception_state) override;
  void Wait(uint32_t duration, ExceptionState& exception_state) override;
  void FadeOut(uint32_t duration, ExceptionState& exception_state) override;
  void FadeIn(uint32_t duration, ExceptionState& exception_state) override;
  void Freeze(ExceptionState& exception_state) override;
  void Transition(ExceptionState& exception_state) override;
  void Transition(uint32_t duration, ExceptionState& exception_state) override;
  void Transition(uint32_t duration,
                  const base::String& filename,
                  ExceptionState& exception_state) override;
  void Transition(uint32_t duration,
                  const base::String& filename,
                  uint32_t vague,
                  ExceptionState& exception_state) override;
  void TransitionWithBitmap(uint32_t duration,
                            scoped_refptr<Bitmap> bitmap,
                            uint32_t vague,
                            ExceptionState& exception_state) override;
  scoped_refptr<Bitmap> SnapToBitmap(ExceptionState& exception_state) override;
  void FrameReset(ExceptionState& exception_state) override;
  uint32_t Width(ExceptionState& exception_state) override;
  uint32_t Height(ExceptionState& exception_state) override;
  void ResizeScreen(uint32_t width,
                    uint32_t height,
                    ExceptionState& exception_state) override;
  void Reset(ExceptionState& exception_state) override;
  void PlayMovie(const base::String& filename,
                 ExceptionState& exception_state) override;
  void MoveWindow(int32_t x,
                  int32_t y,
                  int32_t width,
                  int32_t height,
                  ExceptionState& exception_state) override;
  scoped_refptr<Rect> GetWindowRect(ExceptionState& exception_state) override;
  uint32_t GetDisplayID(ExceptionState& exception_state) override;
  void SetWindowIcon(scoped_refptr<Bitmap> icon,
                     ExceptionState& exception_state) override;
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(FrameRate, uint32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(FrameCount, uint32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Brightness, uint32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(VSync, uint32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Fullscreen, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Skipframe, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(KeepRatio, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(SmoothScale, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BackgroundRunning, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(WindowTitle, base::String);

 private:
  void FrameProcessInternal(Diligent::ITexture** present_target);
  void RenderFrameInternal(Diligent::ITexture** render_target);
  void UpdateWindowViewportInternal();

  DrawNodeController controller_;
  base::RepeatingClosureList tick_observers_;

  base::WeakPtr<ui::Widget> window_;
  ContentProfile* profile_;
  I18NProfile* i18n_profile_;
  base::ThreadWorker* render_worker_;
  ScopedFontData* scoped_font_;
  fpslimiter::FPSLimiter limiter_;

  RenderGraphicsAgent* agent_;
  base::LinkedList<Disposable> disposable_elements_;

  bool frozen_;
  base::Vec2i resolution_;
  base::Rect display_viewport_;
  base::Vec2i window_size_;
  int32_t brightness_;
  uint32_t vsync_;
  uint64_t frame_count_;
  uint32_t frame_rate_;
  base::Vec2i origin_;

  bool keep_ratio_;
  bool smooth_scale_;
  bool allow_skip_frame_;
  bool background_running_;
};

}  // namespace content

#endif  //! CONTENT_SCREEN_RENDERSCREEN_IMPL_H_

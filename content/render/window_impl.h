// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_WINDOW_IMPL_H_
#define CONTENT_RENDER_WINDOW_IMPL_H_

#include "content/canvas/canvas_impl.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/common/tone_impl.h"
#include "content/context/disposable.h"
#include "content/public/engine_window.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"
#include "renderer/device/render_device.h"

namespace content {

class WindowImpl : public Window, public EngineObject, public Disposable {
 public:
  struct Agent {
    base::Vector<renderer::Quad> background_cache;
    renderer::QuadBatch background_batch;
    base::Vector<renderer::Quad> control_cache;
    renderer::QuadBatch control_batch;

    renderer::Binding_Base shader_binding;

    int32_t control_draw_count;
    int32_t contents_draw_count;
  };

  WindowImpl(ExecutionContext* execution_context,
             scoped_refptr<ViewportImpl> parent,
             int32_t scale);
  ~WindowImpl() override;

  WindowImpl(const WindowImpl&) = delete;
  WindowImpl& operator=(const WindowImpl&) = delete;

  void SetLabel(const base::String& label,
                ExceptionState& exception_state) override;

  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void Update(ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Windowskin, scoped_refptr<Bitmap>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Contents, scoped_refptr<Bitmap>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Stretch, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(CursorRect, scoped_refptr<Rect>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Active, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Pause, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(X, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Y, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Width, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Height, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Opacity, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BackOpacity, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ContentsOpacity, int32_t);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "Window"; }
  void BackgroundNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);
  void ControlNodeHandlerInternal(DrawableNode::RenderStage stage,
                                  DrawableNode::RenderControllerParams* params);

  void GPUCreateWindowInternal();
  void GPUCompositeBackgroundLayerInternal(
      renderer::RenderContext* render_context,
      CanvasImpl::Agent* windowskin);
  void GPUCompositeControlLayerInternal(renderer::RenderContext* render_context,
                                        CanvasImpl::Agent* windowskin,
                                        CanvasImpl::Agent* contents);
  void GPURenderBackgroundLayerInternal(renderer::RenderContext* render_context,
                                        Diligent::IBuffer* world_binding,
                                        const base::Rect& last_viewport,
                                        const base::Vec2i& last_origin,
                                        CanvasImpl::Agent* windowskin);
  void GPURenderControlLayerInternal(renderer::RenderContext* render_context,
                                     Diligent::IBuffer* world_binding,
                                     const base::Rect& last_viewport,
                                     const base::Vec2i& last_origin,
                                     CanvasImpl::Agent* windowskin,
                                     CanvasImpl::Agent* contents);

  DrawableNode background_node_;
  DrawableNode control_node_;
  Agent agent_;
  int32_t scale_ = 2;
  int32_t pause_index_ = 0;
  int32_t cursor_opacity_ = 255;
  bool cursor_fade_ = false;

  scoped_refptr<ViewportImpl> viewport_;
  scoped_refptr<CanvasImpl> windowskin_;
  scoped_refptr<CanvasImpl> contents_;
  scoped_refptr<RectImpl> cursor_rect_;

  bool stretch_ = true;
  bool active_ = true;
  bool pause_ = false;

  base::Rect bound_;
  base::Vec2i origin_;

  int32_t opacity_ = 255;
  int32_t back_opacity_ = 255;
  int32_t contents_opacity_ = 255;
};

}  // namespace content

#endif  //! CONTENT_RENDER_WINDOW_IMPL_H_

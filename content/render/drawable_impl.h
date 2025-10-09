// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/engine_drawable.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"

namespace content {

class RenderScissorStackImpl : public RenderScissorStack {
 public:
  RenderScissorStackImpl(ScissorStack* stack);
  ~RenderScissorStackImpl() override;

  RenderScissorStackImpl(const RenderScissorStackImpl&) = delete;
  RenderScissorStackImpl& operator=(const RenderScissorStackImpl&) = delete;

  // Reset internal rawptr state
  void ResetState() { stack_ = nullptr; }

 public:
  scoped_refptr<Rect> GetCurrent(ExceptionState& exception_state) override;
  void Push(scoped_refptr<Rect> region,
            ExceptionState& exception_state) override;
  void Pop(ExceptionState& exception_state) override;
  void Reset(ExceptionState& exception_state) override;

 private:
  ScissorStack* stack_;
};

class DrawableImpl : public Drawable, public EngineObject, public Disposable {
 public:
  DrawableImpl(ExecutionContext* execution_context,
               scoped_refptr<ViewportImpl> parent);
  ~DrawableImpl() override;

  DrawableImpl(const DrawableImpl&) = delete;
  DrawableImpl& operator=(const DrawableImpl&) = delete;

  void SetLabel(const std::string& label,
                ExceptionState& exception_state) override;
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  scoped_refptr<Rect> GetParentRect(ExceptionState& exception_state) override;
  int32_t GetParentOX(ExceptionState& exception_state) override;
  int32_t GetParentOY(ExceptionState& exception_state) override;
  void SetupRender(Stage stage,
                   RenderCallback callback,
                   ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "CustomDrawable"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  DrawableNode node_;

  RenderCallback callbacks_[STAGE_NUMS];
  scoped_refptr<ViewportImpl> viewport_;
};

}  // namespace content

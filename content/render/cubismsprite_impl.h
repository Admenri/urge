// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_CUBISMSPRITE_IMPL_H_
#define CONTENT_RENDER_CUBISMSPRITE_IMPL_H_

#include "base/memory/allocator.h"
#include "content/context/disposable.h"
#include "content/public/engine_cubismsprite.h"
#include "content/screen/renderscreen_impl.h"
#include "content/screen/viewport_impl.h"

namespace content {

class CubismSpriteImpl : public CubismSprite,
                         public EngineObject,
                         public Disposable {
 public:
  CubismSpriteImpl();
  ~CubismSpriteImpl() override;

  CubismSpriteImpl(const CubismSpriteImpl&) = delete;
  CubismSpriteImpl& operator=(const CubismSpriteImpl&) = delete;

 public:
  void SetLabel(const base::String& label,
                ExceptionState& exception_state) override;
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void SetDragging(float x, float y, ExceptionState& exception_state) override;
  void SetExpression(const base::String& expression_name,
                     ExceptionState& exception_state) override;
  void SetRandomExpression(ExceptionState& exception_state) override;
  void StartMotion(const base::String& group,
                   int32_t no,
                   int32_t priority,
                   ExceptionState& exception_state) override;
  void StartRandomMotion(const base::String& group,
                         int32_t priority,
                         ExceptionState& exception_state) override;
  bool HitTest(const base::String& area_name,
               float x,
               float y,
               ExceptionState& exception_state) override;
  float GetParameter(const base::String& name,
                     ExceptionState& exception_state) override;
  float SetParameter(const base::String& name,
                     float value,
                     float weight,
                     ExceptionState& exception_state) override;
  void AddParameter(const base::String& name,
                    float value,
                    float weight,
                    ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(X, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Y, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomX, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomY, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Opacity, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BlendType, int32_t);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "CubismSprite"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  DrawableNode node_;

  scoped_refptr<ViewportImpl> viewport_;
};

}  // namespace content

#endif  // !CONTENT_RENDER_CUBISMSPRITE_IMPL_H_

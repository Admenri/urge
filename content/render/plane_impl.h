// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_PLANE_IMPL_H_
#define CONTENT_RENDER_PLANE_IMPL_H_

#include "content/canvas/canvas_impl.h"
#include "content/public/engine_plane.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"
#include "renderer/resource/render_buffer.h"

namespace content {

class PlaneImpl : public Plane, public EngineObject, public Disposable {
 public:
  struct Agent {
    renderer::QuadBatch batch;
    std::vector<renderer::Quad> cache;
    uint32_t quad_size;

    renderer::Binding_Flat shader_binding;
    RRefPtr<Diligent::IBuffer> uniform_buffer;
  };

  PlaneImpl(ExecutionContext* execution_context,
            scoped_refptr<ViewportImpl> parent);
  ~PlaneImpl();

  PlaneImpl(const PlaneImpl&) = delete;
  PlaneImpl& operator=(const PlaneImpl&) = delete;

  void SetLabel(const std::string& label,
                ExceptionState& exception_state) override;

  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Bitmap, scoped_refptr<Bitmap>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(SrcRect, scoped_refptr<Rect>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomX, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomY, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Opacity, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BlendType, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Color, scoped_refptr<Color>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Tone, scoped_refptr<Tone>);

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "Plane"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  void GPUCreatePlaneInternal();
  void GPUUpdatePlaneQuadArrayInternal(Diligent::IDeviceContext* render_context,
                                       const base::Rect& src_rect,
                                       const base::Vec2i& viewport_size,
                                       const base::Vec2& scale,
                                       const base::Vec2i& origin);
  void GPUOnViewportRenderingInternal(Diligent::IDeviceContext* render_context,
                                      Diligent::IBuffer* world_binding);

  DrawableNode node_;
  Agent agent_;

  scoped_refptr<ViewportImpl> viewport_;
  scoped_refptr<RectImpl> src_rect_;
  scoped_refptr<CanvasImpl> bitmap_;
  base::Vec2i origin_;
  base::Vec2 scale_;
  int32_t opacity_ = 255;
  int32_t blend_type_ = 0;

  scoped_refptr<ColorImpl> color_;
  scoped_refptr<ToneImpl> tone_;
};

}  // namespace content

#endif  //! CONTENT_RENDER_PLANE_IMPL_H_

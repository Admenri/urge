// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SCREEN_VIEWPORT_IMPL_H_
#define CONTENT_SCREEN_VIEWPORT_IMPL_H_

#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/common/tone_impl.h"
#include "content/context/disposable.h"
#include "content/gpu/pipeline_state_impl.h"
#include "content/gpu/resource_binding_impl.h"
#include "content/public/engine_viewport.h"
#include "content/render/drawable_controller.h"
#include "content/screen/renderscreen_impl.h"

namespace content {

class ViewportImpl : public Viewport, public EngineObject, public Disposable {
 public:
  struct Agent {
    RRefPtr<Diligent::IBuffer> world_uniform;

    struct {
      base::Vec2i layer_size;
      renderer::QuadBatch quads;
      renderer::Binding_Flat binding;
      RRefPtr<Diligent::ITexture> intermediate_layer;
      RRefPtr<Diligent::IBuffer> uniform_buffer;
    } effect;
  };

  ViewportImpl(ExecutionContext* execution_context,
               scoped_refptr<ViewportImpl> parent,
               const base::Rect& region);
  ~ViewportImpl() override;

  ViewportImpl(const ViewportImpl&) = delete;
  ViewportImpl& operator=(const ViewportImpl&) = delete;

  static scoped_refptr<ViewportImpl> From(scoped_refptr<Viewport> host);

  URGE_DECLARE_DISPOSABLE;
  void Flash(scoped_refptr<Color> color,
             uint32_t duration,
             ExceptionState& exception_state) override;
  void Update(ExceptionState& exception_state) override;
  void Render(scoped_refptr<Bitmap> target,
              bool clear_target,
              ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Rect, scoped_refptr<Rect>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Color, scoped_refptr<Color>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Tone, scoped_refptr<Tone>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(PipelineState,
                                  scoped_refptr<GPUPipelineState>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ResourceBinding,
                                  scoped_refptr<GPUResourceBinding>);

  DrawNodeController* GetDrawableController() { return &controller_; }

 private:
  void OnObjectDisposed() override;
  std::string DisposedObjectName() override { return "Viewport"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  void GPUCreateViewportAgent();
  void GPUUpdateViewportTransform(Diligent::IDeviceContext* render_context,
                                  const base::Rect& region);
  void GPUResetIntermediateLayer(const base::Vec2i& effect_size);
  void GPUApplyViewportEffect(Diligent::IDeviceContext* render_context,
                              Diligent::ITexture* screen_buffer,
                              Diligent::ITexture* screen_depth_stencil,
                              Diligent::IBuffer* root_world,
                              const base::Rect& effect_region,
                              const base::Vec4& color);
  void GPUFrameBeginRenderPassInternal(Diligent::IDeviceContext* render_context,
                                       GPUBitmapData* render_target,
                                       bool clear_target);

  DrawableNode node_;
  DrawNodeController controller_;

  Agent agent_;
  DrawableFlashController flash_emitter_;
  scoped_refptr<ViewportImpl> viewport_;
  scoped_refptr<RectImpl> rect_;
  base::Vec2i origin_;

  base::Rect transform_cache_;
  base::Vec2i effect_layer_cache_;

  scoped_refptr<PipelineStateImpl> custom_pipeline_;
  scoped_refptr<ResourceBindingImpl> custom_binding_;

  scoped_refptr<ColorImpl> color_;
  scoped_refptr<ToneImpl> tone_;
};

}  // namespace content

#endif  //! CONTENT_SCREEN_VIEWPORT_IMPL_H_

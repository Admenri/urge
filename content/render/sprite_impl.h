// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_SPRITE_IMPL_H_
#define CONTENT_RENDER_SPRITE_IMPL_H_

#include "content/canvas/canvas_impl.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/common/tone_impl.h"
#include "content/context/disposable.h"
#include "content/public/engine_sprite.h"
#include "content/render/drawable_controller.h"
#include "content/screen/viewport_impl.h"
#include "renderer/device/render_device.h"

namespace content {

class SpriteImpl : public Sprite, public EngineObject, public Disposable {
 public:
  struct Agent {
    renderer::Quad quad;
    base::Vector<renderer::Quad> wave_cache;
    RRefPtr<Diligent::IBuffer> single_uniform;

    uint32_t instance_offset = 0;
    uint32_t instance_count = 0;
  };

  struct WaveParams {
    int32_t amp = 0;
    int32_t length = 180;
    int32_t speed = 360;
    float phase = 0.0f;
    bool dirty = false;
  };

  SpriteImpl(ExecutionContext* execution_context,
             scoped_refptr<ViewportImpl> parent);
  ~SpriteImpl() override;

  SpriteImpl(const SpriteImpl&) = delete;
  SpriteImpl& operator=(const SpriteImpl&) = delete;

  void SetLabel(const base::String& label,
                ExceptionState& exception_state) override;

  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  void Flash(scoped_refptr<Color> color,
             uint32_t duration,
             ExceptionState& exception_state) override;
  void Update(ExceptionState& exception_state) override;
  uint32_t Width(ExceptionState& exception_state) override;
  uint32_t Height(ExceptionState& exception_state) override;

  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Bitmap, scoped_refptr<Bitmap>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(SrcRect, scoped_refptr<Rect>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Viewport, scoped_refptr<Viewport>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Visible, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(X, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Y, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Z, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Ox, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Oy, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomX, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(ZoomY, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Angle, float);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(WaveAmp, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(WaveLength, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(WaveSpeed, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(WavePhase, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Mirror, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BushDepth, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BushOpacity, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Opacity, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(BlendType, int32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Color, scoped_refptr<Color>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Tone, scoped_refptr<Tone>);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "Sprite"; }
  void DrawableNodeHandlerInternal(
      DrawableNode::RenderStage stage,
      DrawableNode::RenderControllerParams* params);

  void SrcRectChangedInternal();
  BitmapAgent* GetOtherRenderBatchableTextureInternal(SpriteImpl* other);

  void GPUCreateSpriteInternal();
  void GPUUpdateWaveSpriteInternal(BitmapAgent* texture,
                                   const base::Rect& src_rect);
  void GPUUpdateBatchSpriteInternal(Diligent::IDeviceContext* render_context,
                                    BitmapAgent* texture,
                                    BitmapAgent* next_texture,
                                    const base::Rect& src_rect);
  void GPUOnSpriteRenderingInternal(Diligent::IDeviceContext* render_context,
                                    Diligent::IBuffer* world_binding,
                                    BitmapAgent* texture);

  DrawableNode node_;
  DrawableFlashController flash_emitter_;
  Agent agent_;
  bool rgss2_style_;

  scoped_refptr<ViewportImpl> viewport_;
  scoped_refptr<CanvasImpl> bitmap_;
  scoped_refptr<RectImpl> src_rect_;

  renderer::Binding_Sprite::Params uniform_params_;
  WaveParams wave_;

  struct {
    int32_t depth = 0;
    int32_t opacity = 128;
  } bush_;

  float angle_ = 0.0f;
  int32_t mirror_ = 0;
  int32_t opacity_ = 255;
  int32_t blend_type_ = 0;
  scoped_refptr<ColorImpl> color_;
  scoped_refptr<ToneImpl> tone_;

  base::CallbackListSubscription src_rect_observer_;
  bool src_rect_dirty_ = false;
};

}  // namespace content

#endif  //! CONTENT_RENDER_SPRITE_IMPL_H_

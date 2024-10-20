// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_SPRITE_H_
#define CONTENT_PUBLIC_SPRITE_H_

#include "base/math/transform.h"
#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/flashable.h"
#include "content/public/graphics.h"
#include "content/public/utility.h"
#include "content/public/viewport.h"
#include "renderer/device/render_device.h"
#include "renderer/drawable/quad_array.h"

namespace content {

class Sprite : public base::RefCounted<Sprite>,
               public GraphicsElement,
               public Disposable,
               public ViewportChild,
               public Flashable {
 public:
  Sprite(scoped_refptr<Graphics> screen,
         scoped_refptr<Viewport> viewport = nullptr);
  ~Sprite() override;

  Sprite(const Sprite&) = delete;
  Sprite& operator=(const Sprite&) = delete;

  CONTENT_EXPORT int GetWidth() const {
    CheckIsDisposed();
    return src_rect_->GetWidth();
  }

  CONTENT_EXPORT int GetHeight() const {
    CheckIsDisposed();
    return src_rect_->GetHeight();
  }

  CONTENT_EXPORT void SetBitmap(scoped_refptr<Bitmap> bitmap);
  CONTENT_EXPORT scoped_refptr<Bitmap> GetBitmap() {
    CheckIsDisposed();
    return bitmap_;
  }

  CONTENT_EXPORT void SetSrcRect(scoped_refptr<Rect> rect);
  CONTENT_EXPORT scoped_refptr<Rect> GetSrcRect() {
    CheckIsDisposed();
    return src_rect_;
  }

  CONTENT_EXPORT void SetMirror(bool mirror);
  CONTENT_EXPORT bool GetMirror() const {
    CheckIsDisposed();
    return mirror_;
  }

  CONTENT_EXPORT void SetOpacity(int opacity) {
    CheckIsDisposed();

    opacity = std::clamp(opacity, 0, 255);
    opacity_ = opacity;
  }

  CONTENT_EXPORT int GetOpacity() const {
    CheckIsDisposed();
    return opacity_;
  }

  CONTENT_EXPORT void SetBlendType(renderer::BlendType blend_type) {
    CheckIsDisposed();
    blend_mode_ = blend_type;
  }

  CONTENT_EXPORT renderer::BlendType GetBlendType() const {
    CheckIsDisposed();
    return blend_mode_;
  }

  /* Bush depth & opacity */
  CONTENT_EXPORT void SetBushDepth(int depth) {
    CheckIsDisposed();
    bush_.depth = depth;
  }

  CONTENT_EXPORT int GetBushDepth() {
    CheckIsDisposed();
    return bush_.depth;
  }

  CONTENT_EXPORT void SetBushOpacity(int bushOpacity) {
    CheckIsDisposed();
    bush_.opacity = bushOpacity;
  }

  CONTENT_EXPORT int GetBushOpacity() {
    CheckIsDisposed();
    return bush_.opacity;
  }

  /* Wave emit */
  CONTENT_EXPORT void SetWaveAmp(int wave_amp) {
    CheckIsDisposed();
    wave_.amp = wave_amp;
  }

  CONTENT_EXPORT int GetWaveAmp() {
    CheckIsDisposed();
    return wave_.amp;
  }

  CONTENT_EXPORT void SetWaveLength(int length) {
    CheckIsDisposed();
    wave_.length = length;
  }

  CONTENT_EXPORT int GetWaveLength() {
    CheckIsDisposed();
    return wave_.length;
  }

  CONTENT_EXPORT void SetWaveSpeed(int speed) {
    CheckIsDisposed();
    wave_.speed = speed;
  }

  CONTENT_EXPORT int GetWaveSpeed() {
    CheckIsDisposed();
    return wave_.speed;
  }

  CONTENT_EXPORT void SetWavePhase(float phase) {
    CheckIsDisposed();
    wave_.phase = phase;
  }

  CONTENT_EXPORT float GetWavePhase() {
    CheckIsDisposed();
    return wave_.phase;
  }

  CONTENT_EXPORT scoped_refptr<Color> GetColor() const {
    CheckIsDisposed();
    return color_;
  }

  CONTENT_EXPORT void SetColor(scoped_refptr<Color> color) {
    CheckIsDisposed();
    *color_ = *color;
  }

  CONTENT_EXPORT scoped_refptr<Tone> GetTone() const {
    CheckIsDisposed();
    return tone_;
  }

  CONTENT_EXPORT void SetTone(scoped_refptr<Tone> tone) {
    CheckIsDisposed();
    *tone_ = *tone;
  }

  CONTENT_EXPORT int GetX() const {
    CheckIsDisposed();
    return transform_.GetPosition().x;
  }

  CONTENT_EXPORT void SetX(int v) {
    CheckIsDisposed();
    const base::Vec2& i = transform_.GetPosition();
    if (i.x == v)
      return;
    transform_.SetPosition(base::Vec2((float)v, i.y));
  }

  CONTENT_EXPORT int GetY() const {
    CheckIsDisposed();
    return transform_.GetPosition().y;
  }

  CONTENT_EXPORT void SetY(int v) {
    CheckIsDisposed();
    const base::Vec2& i = transform_.GetPosition();
    if (i.y == v)
      return;
    transform_.SetPosition(base::Vec2(i.x, (float)v));
    if (screen()->api_version() >= APIVersion::RGSS2)
      Drawable::SetSpriteY(v);
  }

  CONTENT_EXPORT int GetOX() const {
    CheckIsDisposed();
    return transform_.GetOrigin().x;
  }

  CONTENT_EXPORT void SetOX(int v) {
    CheckIsDisposed();
    const base::Vec2& i = transform_.GetOrigin();
    if (i.x == v)
      return;
    transform_.SetOrigin(base::Vec2((float)v, i.y));
  }

  CONTENT_EXPORT int GetOY() const {
    CheckIsDisposed();
    return transform_.GetOrigin().y;
  }

  CONTENT_EXPORT void SetOY(int v) {
    CheckIsDisposed();
    const base::Vec2& i = transform_.GetOrigin();
    if (i.y == v)
      return;
    transform_.SetOrigin(base::Vec2(i.x, (float)v));
  }

  CONTENT_EXPORT float GetZoomX() const {
    CheckIsDisposed();
    return transform_.GetScale().x;
  }

  CONTENT_EXPORT void SetZoomX(float v) {
    CheckIsDisposed();
    const base::Vec2& i = transform_.GetScale();
    if (i.x == v)
      return;
    transform_.SetScale(base::Vec2(v, i.y));
  }

  CONTENT_EXPORT float GetZoomY() const {
    CheckIsDisposed();
    return transform_.GetScale().y;
  }

  CONTENT_EXPORT void SetZoomY(float v) {
    CheckIsDisposed();
    const base::Vec2& i = transform_.GetScale();
    if (i.y == v)
      return;
    transform_.SetScale(base::Vec2(i.x, v));
  }

  CONTENT_EXPORT float GetAngle() const {
    CheckIsDisposed();
    return transform_.GetRotation();
  }

  CONTENT_EXPORT void SetAngle(float v) {
    CheckIsDisposed();
    if (transform_.GetRotation() == v)
      return;
    transform_.SetRotation(v);
  }

  CONTENT_EXPORT void Update() override;

 private:
  void InitAttributeInternal();

  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Sprite"; }

  void PrepareDraw() override;
  void OnDraw(CompositeTargetInfo* target_info) override;
  void CheckObjectDisposed() const override { CheckIsDisposed(); }
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo& viewport_rect) override;
  void OnSrcRectChangedInternal();
  void UpdateWaveQuadsInternal();
  void UpdateVisibilityInternal();

  scoped_refptr<Bitmap> bitmap_;
  scoped_refptr<Rect> src_rect_;
  base::TransformMatrix transform_;

  bool mirror_ = false;
  int opacity_ = 255;
  renderer::BlendType blend_mode_ = renderer::BlendType::Normal;
  scoped_refptr<Color> color_;
  scoped_refptr<Tone> tone_;

  struct {
    bool active = false;
    int amp = 0;
    int length = 180;
    int speed = 360;
    float phase = 0.0f;

    bool need_update = true;
  } wave_;

  struct {
    int depth = 0;
    int opacity = 128;
  } bush_;

  std::unique_ptr<renderer::QuadDrawable> drawable_quad_;
  std::unique_ptr<renderer::QuadArray> wave_quads_;

  base::CallbackListSubscription src_rect_observer_;

  bool need_invisible_ = false;
  bool src_rect_need_update_ = false;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_SPRITE_H_
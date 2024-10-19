// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_VIEWPORT_H_
#define CONTENT_PUBLIC_VIEWPORT_H_

#include "base/memory/ref_counted.h"
#include "content/public/disposable.h"
#include "content/public/drawable.h"
#include "content/public/flashable.h"
#include "content/public/graphics.h"

namespace content {

class Viewport;

class ViewportChild : public Drawable {
 public:
  ViewportChild(scoped_refptr<Graphics> screen,
                scoped_refptr<Viewport> viewport,
                int z = 0,
                int sprite_y = 0);

  ViewportChild(const ViewportChild&) = delete;
  ViewportChild& operator=(const ViewportChild&) = delete;

  virtual void SetViewport(scoped_refptr<Viewport> viewport);
  scoped_refptr<Viewport> GetViewport() {
    CheckObjectDisposed();
    return viewport_;
  }

 private:
  scoped_refptr<Graphics> screen_;
  scoped_refptr<Viewport> viewport_;
};

class Viewport : public base::RefCounted<Viewport>,
                 public GraphicsElement,
                 public Disposable,
                 public Flashable,
                 public DrawableParent,
                 public ViewportChild {
 public:
  Viewport(scoped_refptr<Graphics> screen);
  Viewport(scoped_refptr<Graphics> screen, const base::Rect& rect);
  Viewport(scoped_refptr<Graphics> screen, scoped_refptr<Viewport> viewport);
  ~Viewport() override;

  Viewport(const Viewport&) = delete;
  Viewport& operator=(const Viewport&) = delete;

  CONTENT_EXPORT void SetOX(int ox);
  CONTENT_EXPORT int GetOX() {
    CheckIsDisposed();

    return viewport_rect().origin.x;
  }

  CONTENT_EXPORT void SetOY(int oy);
  CONTENT_EXPORT int GetOY() {
    CheckIsDisposed();

    return viewport_rect().origin.y;
  }

  CONTENT_EXPORT scoped_refptr<Color> GetColor() {
    CheckIsDisposed();
    return color_;
  }

  CONTENT_EXPORT void SetColor(scoped_refptr<Color> color) {
    CheckIsDisposed();
    *color_ = *color;
  }

  CONTENT_EXPORT scoped_refptr<Tone> GetTone() {
    CheckIsDisposed();
    return tone_;
  }

  CONTENT_EXPORT void SetTone(scoped_refptr<Tone> tone) {
    CheckIsDisposed();
    *tone_ = *tone;
  }

  CONTENT_EXPORT void SetRect(scoped_refptr<Rect> rect);
  CONTENT_EXPORT scoped_refptr<Rect> GetRect() {
    CheckIsDisposed();
    return rect_;
  }

  CONTENT_EXPORT void SetViewport(scoped_refptr<Viewport> viewport) override;
  CONTENT_EXPORT void SnapToBitmap(scoped_refptr<Bitmap> target);

 private:
  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Viewport"; }

  void PrepareDraw() override;
  void OnDraw(CompositeTargetInfo* target_info) override;
  void CheckObjectDisposed() const override { CheckIsDisposed(); }
  void OnParentViewportRectChanged(const ViewportInfo& rect) override;

  void InitViewportInternal(const base::Rect& initial_rect);
  void OnRectChangedInternal();

  void ApplyViewportEffect(Diligent::ITextureView* target_buffer,
                           const base::Rect& blend_area,
                           const base::Vec4& color,
                           const base::Vec4& tone);

  scoped_refptr<Rect> rect_;
  scoped_refptr<Color> color_;
  scoped_refptr<Tone> tone_;
  base::CallbackListSubscription rect_observer_;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_VIEWPORT_H_
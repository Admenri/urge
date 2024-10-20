// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_PLANE_H_
#define CONTENT_PUBLIC_PLANE_H_

#include "content/public/bitmap.h"
#include "content/public/disposable.h"
#include "content/public/graphics.h"
#include "content/public/viewport.h"
#include "renderer/drawable/quad_array.h"

namespace content {

class Plane : public base::RefCounted<Plane>,
              public GraphicsElement,
              public Disposable,
              public ViewportChild {
 public:
  Plane(scoped_refptr<Graphics> screen,
        scoped_refptr<Viewport> viewport = nullptr);
  ~Plane() override;

  Plane(const Plane&) = delete;
  Plane& operator=(const Plane&) = delete;

  void SetBitmap(scoped_refptr<Bitmap> bitmap);
  scoped_refptr<Bitmap> GetBitmap() const {
    CheckIsDisposed();
    return bitmap_;
  }

  void SetOX(int ox);
  int GetOX() const {
    CheckIsDisposed();
    return ox_;
  }

  void SetOY(int oy);
  int GetOY() const {
    CheckIsDisposed();
    return oy_;
  }

  void SetZoomX(double zoom_x);
  double GetZoomX() const {
    CheckIsDisposed();
    return zoom_x_;
  }

  void SetZoomY(double zoom_y);
  double GetZoomY() const {
    CheckIsDisposed();
    return zoom_y_;
  }

  void SetOpacity(int opacity) {
    CheckIsDisposed();
    opacity_ = opacity;
  }

  int GetOpacity() const {
    CheckIsDisposed();
    return opacity_;
  }

  void SetBlendType(renderer::BlendType blend_type) {
    CheckIsDisposed();
    blend_type_ = blend_type;
  }

  renderer::BlendType GetBlendType() const {
    CheckIsDisposed();
    return blend_type_;
  }

  void SetColor(scoped_refptr<Color> color) {
    CheckIsDisposed();
    *color_ = *color;
  }

  scoped_refptr<Color> GetColor() const {
    CheckIsDisposed();
    return color_;
  }

  void SetTone(scoped_refptr<Tone> tone) {
    CheckIsDisposed();
    *tone_ = *tone;
  }

  scoped_refptr<Tone> GetTone() const {
    CheckIsDisposed();
    return tone_;
  }

 private:
  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Plane"; }

  void PrepareDraw() override;
  void OnDraw(CompositeTargetInfo* target_info) override;
  void CheckObjectDisposed() const override { CheckIsDisposed(); }
  void OnParentViewportRectChanged(
      const DrawableParent::ViewportInfo& rect) override;

  void UpdateQuadArray();

  scoped_refptr<Bitmap> bitmap_;
  int ox_ = 0, oy_ = 0;
  double zoom_x_ = 1.0, zoom_y_ = 1.0;
  int opacity_ = 255;
  renderer::BlendType blend_type_ = renderer::BlendType::Normal;

  scoped_refptr<Color> color_;
  scoped_refptr<Tone> tone_;

  std::unique_ptr<renderer::QuadArray> quad_array_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> cache_layer_;

  bool quad_array_dirty_ = false;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_PLANE_H_
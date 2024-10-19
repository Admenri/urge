// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BITMAP_H_
#define CONTENT_PUBLIC_BITMAP_H_

#include "SDL3/SDL_pixels.h"
#include "SDL3/SDL_surface.h"

#include <memory>
#include <string>
#include <variant>

#include "base/math/rectangle.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_utils.h"
#include "content/public/disposable.h"
#include "content/public/graphics.h"
#include "content/public/utility.h"

namespace content {

enum class TextAlign {
  Left = 0,
  Center,
  Right,
};

class Bitmap : public base::RefCounted<Bitmap>,
               public GraphicsElement,
               public Disposable {
 public:
  Bitmap(scoped_refptr<Graphics> host, const base::Vec2i& size);
  Bitmap(scoped_refptr<Graphics> host,
         filesystem::Filesystem* io,
         const std::string& filename);
  ~Bitmap() override;

  Bitmap(const Bitmap&) = delete;
  Bitmap& operator=(const Bitmap&) = delete;

  CONTENT_EXPORT scoped_refptr<Bitmap> Clone();
  CONTENT_EXPORT base::Vec2i GetSize() const;
  CONTENT_EXPORT scoped_refptr<Rect> GetRect() { return new Rect(GetSize()); }

  CONTENT_EXPORT void Blt(const base::Vec2i& pos,
                          scoped_refptr<Bitmap> src_bitmap,
                          const base::Rect& src_rect,
                          int opacity = 255);
  CONTENT_EXPORT void StretchBlt(const base::Rect& dest_rect,
                                 scoped_refptr<Bitmap> src_bitmap,
                                 const base::Rect& src_rect,
                                 int opacity = 255);

  CONTENT_EXPORT void FillRect(const base::Rect& rect,
                               scoped_refptr<Color> color);

  CONTENT_EXPORT void GradientFillRect(const base::Rect& rect,
                                       scoped_refptr<Color> color1,
                                       scoped_refptr<Color> color2,
                                       bool vertical = false);

  CONTENT_EXPORT void Clear();
  CONTENT_EXPORT void ClearRect(const base::Rect& rect);

  CONTENT_EXPORT scoped_refptr<Color> GetPixel(const base::Vec2i& pos);
  CONTENT_EXPORT void SetPixel(const base::Vec2i& pos,
                               scoped_refptr<Color> color);

  CONTENT_EXPORT void HueChange(int hue);
  CONTENT_EXPORT void Blur();
  CONTENT_EXPORT void RadialBlur(int angle, int division);

  CONTENT_EXPORT void DrawText(const base::Rect& rect,
                               const std::string& str,
                               TextAlign align = TextAlign::Left);
  CONTENT_EXPORT scoped_refptr<Rect> TextSize(const std::string& str);

  CONTENT_EXPORT scoped_refptr<Font> GetFont() const;
  CONTENT_EXPORT void SetFont(scoped_refptr<Font> font);

  SDL_Surface* SurfaceRequired();
  void UpdateSurface();

  Diligent::RefCntAutoPtr<Diligent::ITexture> GetHandle() { return texture_; }

  base::CallbackListSubscription AddBitmapObserver(
      base::RepeatingClosure observer) {
    return observers_.Add(std::move(observer));
  }

 protected:
  void OnObjectDisposed() override;
  std::string_view DisposedObjectName() const override { return "Bitmap"; }

 private:
  void NeedUpdateSurface();

  Diligent::RefCntAutoPtr<Diligent::ITexture> texture_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> read_cache_;
  Diligent::RefCntAutoPtr<Diligent::ITexture> text_cache_;

  scoped_refptr<Font> font_;
  base::RepeatingClosureList observers_;
  SDL_Surface* surface_buffer_;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_BITMAP_H_

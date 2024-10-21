// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_bitmap.h"

#include "binding/mri/init_font.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/mri_template.h"

#include "SDL_image.h"

namespace binding {

static std::string AsString(VALUE obj) {
  VALUE str = rb_obj_as_string(obj);
  return std::string(RSTRING_PTR(str), RSTRING_LEN(str));
}

MRI_DEFINE_DATATYPE_REF(Bitmap, "Bitmap", content::Bitmap);
MRI_DEFINE_DATATYPE(PixelArray, "PixelArray", RUBY_NEVER_FREE);

void bitmap_init_prop(scoped_refptr<content::Bitmap> bitmap, VALUE self) {
  scoped_refptr<content::Font> f = bitmap->GetFont();
  VALUE font = MriWrapProperty(self, f, "_font", kFontDataType);
  font_init_prop(f, font);
}

MRI_METHOD(bitmap_initialize) {
  scoped_refptr<content::Bitmap> bitmap;
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();
  filesystem::Filesystem* io = MriGetGlobalRunner()->io();

  MRI_GUARD_BEGIN;

  switch (argc) {
    default:
    case 1: {
      std::string filename;
      MriParseArgsTo(argc, argv, "s", &filename);

      bitmap = new content::Bitmap(screen, io, filename);
    } break;
    case 2: {
      int width, height;
      MriParseArgsTo(argc, argv, "ii", &width, &height);

      bitmap = new content::Bitmap(screen, base::Vec2i(width, height));
    } break;
  }

  MRI_GUARD_END;

  bitmap->AddRef();
  MriSetStructData(self, bitmap.get());

  bitmap_init_prop(bitmap, self);

  return self;
}

MRI_METHOD(bitmap_initialize_copy) {
  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  scoped_refptr<content::Bitmap> other_obj =
      MriGetStructData<content::Bitmap>(other);

  scoped_refptr<content::Bitmap> obj;
  MRI_GUARD(obj = other_obj->Clone(););
  obj->AddRef();
  MriSetStructData(self, obj.get());

  bitmap_init_prop(obj, self);

  return self;
}

MRI_METHOD(bitmap_width) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int v;
  MRI_GUARD(v = obj->GetSize().x;);

  return rb_fix_new(v);
}

MRI_METHOD(bitmap_height) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int v;
  MRI_GUARD(v = obj->GetSize().y;);

  return rb_fix_new(v);
}

MRI_METHOD(bitmap_rect) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  scoped_refptr<content::Rect> v;
  MRI_GUARD(v = obj->GetRect(););

  return MriWrapObject(v, kRectDataType);
}

MRI_METHOD(bitmap_blt) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int x, y, opacity = 255;
  VALUE src;
  VALUE src_rect;
  MriParseArgsTo(argc, argv, "iioo|i", &x, &y, &src, &src_rect, &opacity);

  scoped_refptr<content::Bitmap> src_bitmap_obj =
      MriCheckStructData<content::Bitmap>(src, kBitmapDataType);
  scoped_refptr<content::Rect> src_rect_obj =
      MriCheckStructData<content::Rect>(src_rect, kRectDataType);

  MRI_GUARD(obj->Blt(base::Vec2i(x, y), src_bitmap_obj, src_rect_obj->AsBase(),
                     opacity););

  return self;
}

MRI_METHOD(bitmap_stretch_blt) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  VALUE dst;
  int opacity = 255;
  VALUE src;
  VALUE src_rect;
  MriParseArgsTo(argc, argv, "ooo|i", &dst, &src, &src_rect, &opacity);

  scoped_refptr<content::Rect> dst_rect_obj =
      MriCheckStructData<content::Rect>(dst, kRectDataType);
  scoped_refptr<content::Bitmap> src_bitmap_obj =
      MriCheckStructData<content::Bitmap>(src, kBitmapDataType);
  scoped_refptr<content::Rect> src_rect_obj =
      MriCheckStructData<content::Rect>(src_rect, kRectDataType);

  MRI_GUARD(obj->StretchBlt(dst_rect_obj->AsBase(), src_bitmap_obj,
                            src_rect_obj->AsBase(), opacity););

  return self;
}

MRI_METHOD(bitmap_fill_rect) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  switch (argc) {
    default:
    case 2: {
      VALUE rect, color;
      MriParseArgsTo(argc, argv, "oo", &rect, &color);

      scoped_refptr<content::Rect> rect_obj =
          MriCheckStructData<content::Rect>(rect, kRectDataType);
      scoped_refptr<content::Color> color_obj =
          MriCheckStructData<content::Color>(color, kColorDataType);

      MRI_GUARD(obj->FillRect(rect_obj->AsBase(), color_obj););
    } break;
    case 5: {
      int x, y, w, h;
      VALUE color;
      MriParseArgsTo(argc, argv, "iiiio", &x, &y, &w, &h, &color);

      scoped_refptr<content::Color> color_obj =
          MriCheckStructData<content::Color>(color, kColorDataType);

      MRI_GUARD(obj->FillRect(base::Rect(x, y, w, h), color_obj););
    } break;
  }

  return self;
}

MRI_METHOD(bitmap_gradient_fill_rect) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  if (argc <= 4) {
    VALUE rect, color1, color2, vertical = false;
    MriParseArgsTo(argc, argv, "ooo|b", &rect, &color1, &color2, &vertical);

    scoped_refptr<content::Rect> rect_obj =
        MriCheckStructData<content::Rect>(rect, kRectDataType);
    scoped_refptr<content::Color> color1_obj =
        MriCheckStructData<content::Color>(color1, kColorDataType);
    scoped_refptr<content::Color> color2_obj =
        MriCheckStructData<content::Color>(color2, kColorDataType);

    MRI_GUARD(obj->GradientFillRect(rect_obj->AsBase(), color1_obj, color2_obj,
                                    vertical););
  } else {
    int x, y, w, h;
    VALUE color1, color2, vertical = false;
    MriParseArgsTo(argc, argv, "iiiioo|b", &x, &y, &w, &h, &color1, &color2,
                   &vertical);

    scoped_refptr<content::Color> color1_obj =
        MriCheckStructData<content::Color>(color1, kColorDataType);
    scoped_refptr<content::Color> color2_obj =
        MriCheckStructData<content::Color>(color2, kColorDataType);
    MRI_GUARD(obj->GradientFillRect(base::Rect(x, y, w, h), color1_obj,
                                    color2_obj, vertical););
  }

  return self;
}

MRI_METHOD(bitmap_clear) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  MRI_GUARD(obj->Clear(););

  return self;
}

MRI_METHOD(bitmap_clear_rect) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  switch (argc) {
    default:
    case 1: {
      VALUE rect;
      MriParseArgsTo(argc, argv, "o", &rect);

      scoped_refptr<content::Rect> rect_obj =
          MriCheckStructData<content::Rect>(rect, kRectDataType);

      MRI_GUARD(obj->ClearRect(rect_obj->AsBase()););
    } break;
    case 4: {
      int x, y, w, h;
      MriParseArgsTo(argc, argv, "iiii", &x, &y, &w, &h);

      MRI_GUARD(obj->ClearRect(base::Rect(x, y, w, h)););
    } break;
  }

  return self;
}

MRI_METHOD(bitmap_get_pixel) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int x, y;
  MriParseArgsTo(argc, argv, "ii", &x, &y);

  scoped_refptr<content::Color> pixel;
  MRI_GUARD(pixel = obj->GetPixel(base::Vec2i(x, y)););
  if (!pixel)
    return Qnil;

  return MriWrapObject(pixel, kColorDataType);
}

MRI_METHOD(bitmap_set_pixel) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int x, y;
  VALUE pixel;
  MriParseArgsTo(argc, argv, "iio", &x, &y, &pixel);

  scoped_refptr<content::Color> color_obj =
      MriCheckStructData<content::Color>(pixel, kColorDataType);

  MRI_GUARD(obj->SetPixel(base::Vec2i(x, y), color_obj););

  return self;
}

MRI_METHOD(bitmap_draw_text) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  if (argc <= 3) {
    VALUE rect;
    VALUE str;
    int align = 0;
    MriParseArgsTo(argc, argv, "oo|i", &rect, &str, &align);

    scoped_refptr<content::Rect> rect_obj =
        MriCheckStructData<content::Rect>(rect, kRectDataType);

    MRI_GUARD(obj->DrawText(rect_obj->AsBase(), AsString(str),
                            (content::TextAlign)align););
  } else {
    int x, y, w, h;
    VALUE str;
    int align = 0;
    MriParseArgsTo(argc, argv, "iiiio|i", &x, &y, &w, &h, &str, &align);

    MRI_GUARD(obj->DrawText(base::Rect(x, y, w, h), AsString(str),
                            (content::TextAlign)align););
  }

  return self;
}

MRI_METHOD(bitmap_text_size) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  std::string str;
  MriParseArgsTo(argc, argv, "s", &str);

  scoped_refptr<content::Rect> rect;
  MRI_GUARD(rect = obj->TextSize(str););

  return MriWrapObject(rect, kRectDataType);
}

MRI_METHOD(bitmap_hue_change) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int hue;
  MriParseArgsTo(argc, argv, "i", &hue);

  MRI_GUARD(obj->HueChange(hue););

  return self;
}

MRI_METHOD(bitmap_blur) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  MRI_GUARD(obj->Blur(););

  return self;
}

MRI_METHOD(bitmap_radial_blur) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int angle, division;
  MriParseArgsTo(argc, argv, "ii", &angle, &division);

  MRI_GUARD(obj->RadialBlur(angle, division););

  return self;
}

MRI_METHOD(bitmap_get_font) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  MRI_GUARD(obj->CheckIsDisposed(););

  return rb_iv_get(self, "_font");
}

MRI_METHOD(bitmap_set_font) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  VALUE f;
  MriParseArgsTo(argc, argv, "o", &f);
  scoped_refptr<content::Font> font_obj =
      MriCheckStructData<content::Font>(f, kFontDataType);

  MRI_GUARD(obj->SetFont(font_obj););

  return self;
}

MRI_METHOD(bitmap_save_png) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  std::string path;
  MriParseArgsTo(argc, argv, "s", &path);

  SDL_Surface* auto_surf = nullptr;
  MRI_GUARD(auto_surf = obj->SurfaceRequired(););
  IMG_SavePNG(auto_surf, path.c_str());

  return self;
}

MRI_METHOD(bitmap_process_pixel) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  VALUE pa_klass =
      rb_const_get(rb_cObject, rb_intern(kPixelArrayDataType.wrap_struct_name));
  VALUE pa_obj = rb_obj_alloc(pa_klass);
  MriSetStructData<content::Bitmap>(pa_obj, obj.get());
  rb_iv_set(self, "_pixel_array", pa_obj);

  // Cached pixels in memory
  obj->SurfaceRequired();

  VALUE yield = rb_block_proc();
  rb_funcall2(yield, rb_intern("call"), 1, &pa_obj);

  // Update memory to GPU memory
  obj->UpdateSurface();
  rb_iv_set(self, "_pixel_array", Qnil);

  return self;
}

MRI_METHOD(pixelarray_get_color) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int index;
  MriParseArgsTo(argc, argv, "i", &index);

  SDL_Surface* surf = obj->SurfaceRequired();
  uint32_t pixel = *(static_cast<uint32_t*>(surf->pixels) + index);

  return ULONG2NUM(pixel);
}

MRI_METHOD(pixelarray_set_color) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  int index;
  VALUE pixel;
  MriParseArgsTo(argc, argv, "io", &index, &pixel);

  SDL_Surface* surf = obj->SurfaceRequired();
  *(static_cast<uint32_t*>(surf->pixels) + index) = NUM2ULONG(pixel);

  return Qnil;
}

MRI_METHOD(pixelarray_savedata) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  VALUE buf;
  MriParseArgsTo(argc, argv, "o", &buf);

  if (!RB_TYPE_P(buf, RUBY_T_STRING))
    rb_raise(rb_eTypeError, "Argument 0: Expected string");

  SDL_Surface* surf = obj->SurfaceRequired();
  memcpy(RSTRING_PTR(buf), surf->pixels, RSTRING_LEN(buf));

  return buf;
}

MRI_METHOD(pixelarray_loaddata) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  VALUE buf;
  MriParseArgsTo(argc, argv, "o", &buf);

  if (!RB_TYPE_P(buf, RUBY_T_STRING))
    rb_raise(rb_eTypeError, "Argument 0: Expected string");

  SDL_Surface* surf = obj->SurfaceRequired();
  memcpy(surf->pixels, RSTRING_PTR(buf), RSTRING_LEN(buf));

  return buf;
}

MRI_METHOD(pixelarray_size) {
  scoped_refptr<content::Bitmap> obj = MriGetStructData<content::Bitmap>(self);

  SDL_Surface* surf = obj->SurfaceRequired();
  return INT2FIX(surf->w * surf->h * 4);
}

void InitBitmapBinding() {
  VALUE klass = rb_define_class("Bitmap", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kBitmapDataType>);

  MriDefineMethod(klass, "initialize", bitmap_initialize);
  MriDefineMethod(klass, "initialize_copy", bitmap_initialize_copy);

  MriInitDisposableBinding<content::Bitmap>(klass);

  MriDefineMethod(klass, "width", bitmap_width);
  MriDefineMethod(klass, "height", bitmap_height);
  MriDefineMethod(klass, "rect", bitmap_rect);
  MriDefineMethod(klass, "blt", bitmap_blt);
  MriDefineMethod(klass, "stretch_blt", bitmap_stretch_blt);
  MriDefineMethod(klass, "fill_rect", bitmap_fill_rect);
  MriDefineMethod(klass, "gradient_fill_rect", bitmap_gradient_fill_rect);
  MriDefineMethod(klass, "clear", bitmap_clear);
  MriDefineMethod(klass, "clear_rect", bitmap_clear_rect);
  MriDefineMethod(klass, "get_pixel", bitmap_get_pixel);
  MriDefineMethod(klass, "set_pixel", bitmap_set_pixel);
  MriDefineMethod(klass, "hue_change", bitmap_hue_change);
  MriDefineMethod(klass, "blur", bitmap_blur);
  MriDefineMethod(klass, "radial_blur", bitmap_radial_blur);
  MriDefineMethod(klass, "draw_text", bitmap_draw_text);
  MriDefineMethod(klass, "text_size", bitmap_text_size);
  MriDefineAttr(klass, "font", bitmap, font);

  MriDefineMethod(klass, "save_png", bitmap_save_png);
  MriDefineMethod(klass, "process_pixel", bitmap_process_pixel);

  /* Pixel Process extension */
  VALUE pixel_array = rb_define_class("PixelArray", rb_cObject);
  rb_define_alloc_func(pixel_array, MriClassAllocate<&kPixelArrayDataType>);

  MriDefineAttr(pixel_array, "[]", pixelarray, color);
  MriDefineMethod(pixel_array, "save_data", pixelarray_savedata);
  MriDefineMethod(pixel_array, "load_data", pixelarray_loaddata);
  MriDefineMethod(pixel_array, "size", pixelarray_size);
}

}  // namespace binding

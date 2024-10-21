// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_sprite.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/mri_template.h"
#include "content/public/sprite.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Sprite, "Sprite", content::Sprite);

MRI_METHOD(sprite_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();
  scoped_refptr<content::Sprite> sprite_obj =
      MriInitializeViewportchild<content::Sprite>(screen, argc, argv, self);

  MriWrapProperty(self, sprite_obj->GetSrcRect(), "_src_rect", kRectDataType);
  MriWrapProperty(self, sprite_obj->GetColor(), "_color", kColorDataType);
  MriWrapProperty(self, sprite_obj->GetTone(), "_tone", kToneDataType);

  return self;
}

MRI_METHOD(sprite_width) {
  scoped_refptr<content::Sprite> obj = MriGetStructData<content::Sprite>(self);
  int v;
  MRI_GUARD(v = obj->GetWidth(););
  return rb_fix_new(v);
}

MRI_METHOD(sprite_height) {
  scoped_refptr<content::Sprite> obj = MriGetStructData<content::Sprite>(self);
  int v;
  MRI_GUARD(v = obj->GetHeight(););
  return rb_fix_new(v);
}

MRI_METHOD(sprite_get_bitmap) {
  scoped_refptr<content::Sprite> obj = MriGetStructData<content::Sprite>(self);
  return rb_iv_get(self, "_bitmap");
}

MRI_METHOD(sprite_set_bitmap) {
  MriCheckArgc(argc, 1);
  scoped_refptr<content::Sprite> obj = MriGetStructData<content::Sprite>(self);
  VALUE propObj = *argv;
  scoped_refptr<content::Bitmap> prop;
  if (!NIL_P(propObj))
    prop = MriCheckStructData<content::Bitmap>(propObj, kBitmapDataType);
  MRI_GUARD(obj->SetBitmap(prop););
  rb_iv_set(self, "_bitmap", *argv);
  return propObj;
}

#define DEFINE_SPRITE_ATTR_VAL(name, iv, ty)                       \
  MRI_METHOD(sprite_get_##name) {                                  \
    scoped_refptr<content::Sprite> obj =                           \
        MriGetStructData<content::Sprite>(self);                   \
    MRI_GUARD(obj->CheckIsDisposed(););                            \
    return rb_iv_get(self, iv);                                    \
  }                                                                \
  MRI_METHOD(sprite_set_##name) {                                  \
    MriCheckArgc(argc, 1);                                         \
    scoped_refptr<content::Sprite> obj =                           \
        MriGetStructData<content::Sprite>(self);                   \
    VALUE propObj = *argv;                                         \
    scoped_refptr<content::ty> prop =                              \
        MriCheckStructData<content::ty>(propObj, k##ty##DataType); \
    MRI_GUARD(obj->Set##name(prop););                              \
    return propObj;                                                \
  }

DEFINE_SPRITE_ATTR_VAL(SrcRect, "_src_rect", Rect);
DEFINE_SPRITE_ATTR_VAL(Color, "_color", Color);
DEFINE_SPRITE_ATTR_VAL(Tone, "_tone", Tone);

#define DEFINE_SPRITE_INT_ATTR(name)             \
  MRI_METHOD(sprite_get_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    int v;                                       \
    MRI_GUARD(v = obj->Get##name(););            \
    return rb_fix_new(v);                        \
  }                                              \
  MRI_METHOD(sprite_set_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    int v;                                       \
    MriParseArgsTo(argc, argv, "i", &v);         \
    MRI_GUARD(obj->Set##name(v););               \
    return *argv;                                \
  }

#define DEFINE_SPRITE_INT_ATTR_T(name, ty)       \
  MRI_METHOD(sprite_get_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    int v;                                       \
    MRI_GUARD(v = (int)obj->Get##name(););       \
    return rb_fix_new(v);                        \
  }                                              \
  MRI_METHOD(sprite_set_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    int v;                                       \
    MriParseArgsTo(argc, argv, "i", &v);         \
    MRI_GUARD(obj->Set##name((ty)v););           \
    return *argv;                                \
  }

#define DEFINE_SPRITE_BOOL_ATTR(name)            \
  MRI_METHOD(sprite_get_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    bool v;                                      \
    MRI_GUARD(v = obj->Get##name(););            \
    return MRI_BOOL_NEW(v);                      \
  }                                              \
  MRI_METHOD(sprite_set_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    bool v;                                      \
    MriParseArgsTo(argc, argv, "b", &v);         \
    MRI_GUARD(obj->Set##name(v););               \
    return *argv;                                \
  }

#define DEFINE_SPRITE_FLOAT_ATTR(name)           \
  MRI_METHOD(sprite_get_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    float v;                                     \
    MRI_GUARD(v = obj->Get##name(););            \
    return rb_float_new(v);                      \
  }                                              \
  MRI_METHOD(sprite_set_##name) {                \
    scoped_refptr<content::Sprite> obj =         \
        MriGetStructData<content::Sprite>(self); \
    double v;                                    \
    MriParseArgsTo(argc, argv, "f", &v);         \
    MRI_GUARD(obj->Set##name((float)v););        \
    return *argv;                                \
  }

DEFINE_SPRITE_INT_ATTR(X);
DEFINE_SPRITE_INT_ATTR(Y);
DEFINE_SPRITE_INT_ATTR(OX);
DEFINE_SPRITE_INT_ATTR(OY);
DEFINE_SPRITE_INT_ATTR(BushDepth);
DEFINE_SPRITE_INT_ATTR(BushOpacity);
DEFINE_SPRITE_INT_ATTR(Opacity);
DEFINE_SPRITE_INT_ATTR_T(BlendType, renderer::BlendType);
DEFINE_SPRITE_INT_ATTR(WaveAmp);
DEFINE_SPRITE_INT_ATTR(WaveLength);
DEFINE_SPRITE_INT_ATTR(WaveSpeed);
DEFINE_SPRITE_BOOL_ATTR(Mirror);
DEFINE_SPRITE_FLOAT_ATTR(ZoomX);
DEFINE_SPRITE_FLOAT_ATTR(ZoomY);
DEFINE_SPRITE_FLOAT_ATTR(Angle);
DEFINE_SPRITE_FLOAT_ATTR(WavePhase);

MRI_METHOD(sprite_get_shader) {
  scoped_refptr<content::Sprite> obj = MriGetStructData<content::Sprite>(self);
  return rb_iv_get(self, "_shader");
}

void InitSpriteBinding() {
  VALUE klass = rb_define_class("Sprite", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kSpriteDataType>);

  MriDefineMethod(klass, "initialize", sprite_initialize);
  MriDefineMethod(klass, "width", sprite_width);
  MriDefineMethod(klass, "height", sprite_height);

  MriInitDisposableBinding<content::Sprite>(klass);
  MriInitFlashableBinding<content::Sprite>(klass);
  MriInitViewportChildBinding<content::Sprite>(klass);

  MriDefineAttr(klass, "bitmap", sprite, bitmap);
  MriDefineAttr(klass, "src_rect", sprite, SrcRect);
  MriDefineAttr(klass, "color", sprite, Color);
  MriDefineAttr(klass, "tone", sprite, Tone);

  MriDefineAttr(klass, "x", sprite, X);
  MriDefineAttr(klass, "y", sprite, Y);
  MriDefineAttr(klass, "ox", sprite, OX);
  MriDefineAttr(klass, "oy", sprite, OY);
  MriDefineAttr(klass, "bush_depth", sprite, BushDepth);
  MriDefineAttr(klass, "bush_opacity", sprite, BushOpacity);
  MriDefineAttr(klass, "opacity", sprite, Opacity);
  MriDefineAttr(klass, "blend_type", sprite, BlendType);
  MriDefineAttr(klass, "wave_amp", sprite, WaveAmp);
  MriDefineAttr(klass, "wave_length", sprite, WaveLength);
  MriDefineAttr(klass, "wave_speed", sprite, WaveSpeed);
  MriDefineAttr(klass, "wave_phase", sprite, WavePhase);
  MriDefineAttr(klass, "mirror", sprite, Mirror);
  MriDefineAttr(klass, "zoom_x", sprite, ZoomX);
  MriDefineAttr(klass, "zoom_y", sprite, ZoomY);
  MriDefineAttr(klass, "angle", sprite, Angle);
}

}  // namespace binding

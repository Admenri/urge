// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_viewport.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/mri_template.h"
#include "content/public/viewport.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Viewport, "Viewport", content::Viewport);

MRI_METHOD(viewport_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();
  scoped_refptr<content::Viewport> obj;

  if (argc == 0) {
    obj = new content::Viewport(screen);
  } else if (argc == 1) {
    VALUE rect;
    MriParseArgsTo(argc, argv, "o", &rect);

    if (rb_typeddata_is_kind_of(rect, &kRectDataType)) {
      scoped_refptr<content::Rect> rect_obj =
          MriCheckStructData<content::Rect>(rect, kRectDataType);

      obj = new content::Viewport(screen, rect_obj->AsBase());
    } else if (rb_typeddata_is_kind_of(rect, &kViewportDataType)) {
      scoped_refptr<content::Viewport> vp_obj =
          MriCheckStructData<content::Viewport>(rect, kViewportDataType);

      obj = new content::Viewport(screen, vp_obj);
    } else {
      // Default create
      obj = new content::Viewport(screen);
    }
  } else {
    int x, y, width, height;

    MriParseArgsTo(argc, argv, "iiii", &x, &y, &width, &height);

    obj = new content::Viewport(screen, base::Rect(x, y, width, height));
  }

  obj->AddRef();
  MriSetStructData<content::Viewport>(self, obj.get());

  MriWrapProperty(self, obj->GetRect(), "_rect", kRectDataType);
  MriWrapProperty(self, obj->GetColor(), "_color", kColorDataType);
  MriWrapProperty(self, obj->GetTone(), "_tone", kToneDataType);

  return self;
}

MRI_METHOD(viewport_snap_to_bitmap) {
  scoped_refptr<content::Viewport> obj =
      MriGetStructData<content::Viewport>(self);

  VALUE bitmap;
  MriParseArgsTo(argc, argv, "o", &bitmap);

  scoped_refptr<content::Bitmap> target =
      MriCheckStructData<content::Bitmap>(bitmap, kBitmapDataType);
  obj->SnapToBitmap(target);

  return bitmap;
}

#define VIEWPORT_DEFINE_INT_ATTR(name)             \
  MRI_METHOD(viewport_get_##name) {                \
    scoped_refptr<content::Viewport> obj =         \
        MriGetStructData<content::Viewport>(self); \
    int v = 0;                                     \
    MRI_GUARD(v = obj->Get##name(););              \
    return rb_fix_new(v);                          \
  }                                                \
  MRI_METHOD(viewport_set_##name) {                \
    scoped_refptr<content::Viewport> obj =         \
        MriGetStructData<content::Viewport>(self); \
    int v = 0;                                     \
    MriParseArgsTo(argc, argv, "i", &v);           \
    MRI_GUARD(obj->Set##name(v););                 \
    return self;                                   \
  }

#define VIEWPORT_DEFINE_FLOAT_ATTR(name)           \
  MRI_METHOD(viewport_get_##name) {                \
    scoped_refptr<content::Viewport> obj =         \
        MriGetStructData<content::Viewport>(self); \
    float v = 0;                                   \
    MRI_GUARD(v = obj->Get##name(););              \
    return rb_float_new(v);                        \
  }                                                \
  MRI_METHOD(viewport_set_##name) {                \
    scoped_refptr<content::Viewport> obj =         \
        MriGetStructData<content::Viewport>(self); \
    double v = 0;                                  \
    MriParseArgsTo(argc, argv, "f", &v);           \
    MRI_GUARD(obj->Set##name(v););                 \
    return self;                                   \
  }

VIEWPORT_DEFINE_INT_ATTR(OX);
VIEWPORT_DEFINE_INT_ATTR(OY);

#define VIEWPORT_DEFINE_VAL_ATTR(name, ivname)                         \
  MRI_METHOD(viewport_get_##name) {                                    \
    scoped_refptr<content::Viewport> obj =                             \
        MriGetStructData<content::Viewport>(self);                     \
    MRI_GUARD(obj->CheckIsDisposed(););                                \
    return rb_iv_get(self, #ivname);                                   \
  }                                                                    \
  MRI_METHOD(viewport_set_##name) {                                    \
    MriCheckArgc(argc, 1);                                             \
    scoped_refptr<content::Viewport> obj =                             \
        MriGetStructData<content::Viewport>(self);                     \
    VALUE propObj = *argv;                                             \
    scoped_refptr<content::name> prop =                                \
        MriCheckStructData<content::name>(propObj, k##name##DataType); \
    MRI_GUARD(obj->Set##name(prop);)                                   \
    return propObj;                                                    \
  }

VIEWPORT_DEFINE_VAL_ATTR(Rect, _rect);
VIEWPORT_DEFINE_VAL_ATTR(Color, _color);
VIEWPORT_DEFINE_VAL_ATTR(Tone, _tone);

MRI_METHOD(viewport_get_shader) {
  scoped_refptr<content::Viewport> obj =
      MriGetStructData<content::Viewport>(self);
  return rb_iv_get(self, "_shader");
}

void InitViewportBinding() {
  VALUE klass = rb_define_class("Viewport", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kViewportDataType>);

  MriInitDisposableBinding<content::Viewport>(klass);
  MriInitFlashableBinding<content::Viewport>(klass);
  MriInitDrawableBinding<content::Viewport>(klass);
  MriInitViewportChildBinding<content::Viewport>(klass);

  MriDefineMethod(klass, "initialize", viewport_initialize);
  MriDefineMethod(klass, "snap_to_bitmap", viewport_snap_to_bitmap);

  MriDefineAttr(klass, "ox", viewport, OX);
  MriDefineAttr(klass, "oy", viewport, OY);
  MriDefineAttr(klass, "rect", viewport, Rect);
  MriDefineAttr(klass, "color", viewport, Color);
  MriDefineAttr(klass, "tone", viewport, Tone);
}

}  // namespace binding

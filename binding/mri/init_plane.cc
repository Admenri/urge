// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_plane.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/mri_template.h"
#include "content/public/plane.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Plane, "Plane", content::Plane);

MRI_METHOD(plane_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  scoped_refptr<content::Plane> plane =
      MriInitializeViewportchild<content::Plane>(screen, argc, argv, self);

  MriWrapProperty(self, plane->GetColor(), "_color", kColorDataType);
  MriWrapProperty(self, plane->GetTone(), "_tone", kToneDataType);

  return self;
}

#define PLANE_DEFINE_VAL_ATTR(name, ivname)                            \
  MRI_METHOD(plane_get_##name) {                                       \
    scoped_refptr<content::Plane> obj =                                \
        MriGetStructData<content::Plane>(self);                        \
    MRI_GUARD(obj->CheckIsDisposed(););                                \
    return rb_iv_get(self, #ivname);                                   \
  }                                                                    \
  MRI_METHOD(plane_set_##name) {                                       \
    MriCheckArgc(argc, 1);                                             \
    scoped_refptr<content::Plane> obj =                                \
        MriGetStructData<content::Plane>(self);                        \
    VALUE propObj = *argv;                                             \
    scoped_refptr<content::name> prop =                                \
        MriCheckStructData<content::name>(propObj, k##name##DataType); \
    MRI_GUARD(obj->Set##name(prop););                                  \
    return propObj;                                                    \
  }

PLANE_DEFINE_VAL_ATTR(Color, _color);
PLANE_DEFINE_VAL_ATTR(Tone, _tone);

#define DEFINE_PLANE_ATTR(name, ty, p, f)       \
  MRI_METHOD(plane_get_##name) {                \
    scoped_refptr<content::Plane> obj =         \
        MriGetStructData<content::Plane>(self); \
    ty v;                                       \
    MRI_GUARD(v = obj->Get##name(););           \
    return f(v);                                \
  }                                             \
  MRI_METHOD(plane_set_##name) {                \
    scoped_refptr<content::Plane> obj =         \
        MriGetStructData<content::Plane>(self); \
    ty v;                                       \
    MriParseArgsTo(argc, argv, #p, &v);         \
    MRI_GUARD(obj->Set##name(v););              \
    return self;                                \
  }

DEFINE_PLANE_ATTR(OX, int, i, rb_fix_new);
DEFINE_PLANE_ATTR(OY, int, i, rb_fix_new);
DEFINE_PLANE_ATTR(Opacity, int, i, rb_fix_new);

MRI_METHOD(plane_get_blendtype) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  long v;
  MRI_GUARD(v = (long)obj->GetBlendType(););
  return rb_fix_new(v);
}

MRI_METHOD(plane_set_blendtype) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  int v;
  MriParseArgsTo(argc, argv, "i", &v);
  MRI_GUARD(obj->SetBlendType((renderer::BlendType)v););
  return self;
}

MRI_METHOD(plane_get_bitmap) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  return rb_iv_get(self, "_bitmap");
}

MRI_METHOD(plane_set_bitmap) {
  MriCheckArgc(argc, 1);
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  VALUE propObj = *argv;
  scoped_refptr<content::Bitmap> prop;
  if (!NIL_P(propObj))
    prop = MriCheckStructData<content::Bitmap>(propObj, kBitmapDataType);
  MRI_GUARD(obj->SetBitmap(prop););
  rb_iv_set(self, "_bitmap", *argv);
  return propObj;
}

MRI_METHOD(plane_get_zoom_x) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  double v;
  MRI_GUARD(v = (double)obj->GetZoomX(););
  return rb_float_new(v);
}

MRI_METHOD(plane_set_zoom_x) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  double v;
  MriParseArgsTo(argc, argv, "f", &v);
  MRI_GUARD(obj->SetZoomX((float)v););
  return self;
}

MRI_METHOD(plane_get_zoom_y) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  double v;
  MRI_GUARD(v = (double)obj->GetZoomY(););
  return rb_float_new(v);
}

MRI_METHOD(plane_set_zoom_y) {
  scoped_refptr<content::Plane> obj = MriGetStructData<content::Plane>(self);
  double v;
  MriParseArgsTo(argc, argv, "f", &v);
  MRI_GUARD(obj->SetZoomY((float)v););
  return self;
}

void InitPlaneBinding() {
  VALUE klass = rb_define_class("Plane", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kPlaneDataType>);

  MriInitViewportChildBinding<content::Plane>(klass);
  MriInitDisposableBinding<content::Plane>(klass);

  MriDefineMethod(klass, "initialize", plane_initialize);

  MriDefineAttr(klass, "bitmap", plane, bitmap);
  MriDefineAttr(klass, "color", plane, Color);
  MriDefineAttr(klass, "tone", plane, Tone);
  MriDefineAttr(klass, "ox", plane, OX);
  MriDefineAttr(klass, "oy", plane, OY);
  MriDefineAttr(klass, "zoom_x", plane, zoom_x);
  MriDefineAttr(klass, "zoom_y", plane, zoom_y);
  MriDefineAttr(klass, "opacity", plane, Opacity);
  MriDefineAttr(klass, "blend_type", plane, blendtype);
}

}  // namespace binding

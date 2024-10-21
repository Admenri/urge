// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_tilemap2.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_table.h"
#include "binding/mri/mri_template.h"
#include "content/public/tilemap2.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Tilemap2, "Tilemap", content::Tilemap2);

MRI_DEFINE_DATATYPE(BitmapArray, "BitmapArray", RUBY_NEVER_FREE);

MRI_METHOD(tilemap2_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  VALUE v = Qnil;
  int tilesize = 32;
  MriParseArgsTo(argc, argv, "|oi", &v, &tilesize);

  scoped_refptr<content::Viewport> viewport;
  if (!NIL_P(v))
    viewport = MriCheckStructData<content::Viewport>(v, kViewportDataType);

  scoped_refptr<content::Tilemap2> obj;
  MRI_GUARD(obj = new content::Tilemap2(screen, viewport, tilesize););
  obj->AddRef();
  MriSetStructData(self, obj.get());

  rb_iv_set(self, "_viewport", v);

  VALUE klass = rb_const_get(rb_cObject, rb_intern("BitmapArray"));
  VALUE bitmap_array = rb_obj_alloc(klass);
  MriSetStructData(bitmap_array, obj.get());

  VALUE ary = rb_ary_new2(9);
  for (int i = 0; i < 9; ++i)
    rb_ary_push(ary, Qnil);
  rb_iv_set(bitmap_array, "_bitmaps", ary);

  rb_iv_set(self, "_bitmap_array", bitmap_array);

  return self;
}

MRI_METHOD(tilemap2_update) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);

  MRI_GUARD(obj->Update(););
  return self;
}

MRI_METHOD(tilemap2_bitmap_array) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  MRI_GUARD(obj->CheckIsDisposed(););

  return rb_iv_get(self, "_bitmap_array");
}

MRI_METHOD(tilemap2_get_ox) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  int v;
  MRI_GUARD(v = obj->GetOX(););
  return rb_fix_new(v);
}

MRI_METHOD(tilemap2_set_ox) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  int v;
  MriParseArgsTo(argc, argv, "i", &v);
  MRI_GUARD(obj->SetOX(v););
  return self;
}

MRI_METHOD(tilemap2_get_oy) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  int v;
  MRI_GUARD(v = obj->GetOY(););
  return rb_fix_new(v);
}

MRI_METHOD(tilemap2_set_oy) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  int v;
  MriParseArgsTo(argc, argv, "i", &v);
  MRI_GUARD(obj->SetOY(v););
  return self;
}

MRI_METHOD(tilemap2_get_visible) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  bool v;
  MRI_GUARD(v = obj->GetVisible(););
  return v ? Qtrue : Qfalse;
}

MRI_METHOD(tilemap2_set_visible) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);
  bool v;
  MriParseArgsTo(argc, argv, "b", &v);
  MRI_GUARD(obj->SetVisible(v););
  return self;
}

#define DEFINE_TILEMAP2_ATTR_TABLE(name, iv)                     \
  MRI_METHOD(tilemap2_get_##name) {                              \
    scoped_refptr<content::Tilemap2> obj =                       \
        MriGetStructData<content::Tilemap2>(self);               \
    return rb_iv_get(self, iv);                                  \
  }                                                              \
  MRI_METHOD(tilemap2_set_##name) {                              \
    scoped_refptr<content::Tilemap2> obj =                       \
        MriGetStructData<content::Tilemap2>(self);               \
    VALUE v;                                                     \
    MriParseArgsTo(argc, argv, "o", &v);                         \
    scoped_refptr<content::Table> t;                             \
    if (!NIL_P(v))                                               \
      t = MriCheckStructData<content::Table>(v, kTableDataType); \
    MRI_GUARD(obj->Set##name(t););                               \
    rb_iv_set(self, iv, v);                                      \
    return v;                                                    \
  }

DEFINE_TILEMAP2_ATTR_TABLE(MapData, "_map_data");
DEFINE_TILEMAP2_ATTR_TABLE(FlashData, "_flash_data");
DEFINE_TILEMAP2_ATTR_TABLE(Flags, "_flags");

MRI_METHOD(tilemap2_get_viewport) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);

  MRI_GUARD(obj->CheckIsDisposed(););

  return rb_iv_get(self, "_viewport");
}

MRI_METHOD(tilemap2_set_viewport) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);

  VALUE v;
  MriParseArgsTo(argc, argv, "o", &v);

  scoped_refptr<content::Viewport> viewport;
  if (!NIL_P(v))
    viewport = MriCheckStructData<content::Viewport>(v, kViewportDataType);

  MRI_GUARD(obj->SetViewport(viewport););

  rb_iv_set(self, "_viewport", v);

  return v;
}

MRI_METHOD(bitmaparray_get_bitmaps) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);

  int i;
  MriParseArgsTo(argc, argv, "i", &i);
  i = std::clamp(i, 0, 8);

  VALUE ary = rb_iv_get(self, "_bitmaps");
  return rb_ary_entry(ary, i);
}

MRI_METHOD(bitmaparray_set_bitmaps) {
  scoped_refptr<content::Tilemap2> obj =
      MriGetStructData<content::Tilemap2>(self);

  int i;
  VALUE o;
  MriParseArgsTo(argc, argv, "io", &i, &o);
  i = std::clamp(i, 0, 8);

  scoped_refptr<content::Bitmap> prop;
  if (!NIL_P(o))
    prop = MriCheckStructData<content::Bitmap>(o, kBitmapDataType);
  obj->SetBitmap(i, prop);

  VALUE ary = rb_iv_get(self, "_bitmaps");
  rb_ary_store(ary, i, o);

  return o;
}

void InitTilemap2Binding() {
  VALUE klass = rb_define_class("Tilemap", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kTilemap2DataType>);

  MriInitDisposableBinding<content::Tilemap2>(klass);

  MriDefineMethod(klass, "initialize", tilemap2_initialize);
  MriDefineMethod(klass, "update", tilemap2_update);

  MriDefineMethod(klass, "bitmaps", tilemap2_bitmap_array);
  MriDefineAttr(klass, "ox", tilemap2, ox);
  MriDefineAttr(klass, "oy", tilemap2, oy);
  MriDefineAttr(klass, "visible", tilemap2, visible);
  MriDefineAttr(klass, "viewport", tilemap2, viewport);
  MriDefineAttr(klass, "map_data", tilemap2, MapData);
  MriDefineAttr(klass, "flash_data", tilemap2, FlashData);
  if (MriGetGlobalRunner()->config()->content_version() >=
      content::APIVersion::RGSS3) {
    MriDefineAttr(klass, "flags", tilemap2, Flags);
  } else {
    MriDefineAttr(klass, "passages", tilemap2, Flags);
  }

  VALUE bitmaps = rb_define_class("BitmapArray", rb_cObject);
  rb_define_alloc_func(bitmaps, MriClassAllocate<&kBitmapArrayDataType>);

  MriDefineAttr(bitmaps, "[]", bitmaparray, bitmaps);
}

}  // namespace binding

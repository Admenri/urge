// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_tilemap.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_table.h"
#include "binding/mri/mri_template.h"
#include "content/public/tilemap.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Tilemap, "Tilemap", content::Tilemap);

MRI_DEFINE_DATATYPE(TilemapAutotiles, "TilemapAutotiles", RUBY_NEVER_FREE);

MRI_METHOD(tilemap_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  VALUE v = Qnil;
  int tilesize = 32;
  MriParseArgsTo(argc, argv, "|oi", &v, &tilesize);

  scoped_refptr<content::Viewport> viewport;
  if (!NIL_P(v))
    viewport = MriCheckStructData<content::Viewport>(v, kViewportDataType);

  scoped_refptr<content::Tilemap> obj;
  MRI_GUARD(obj = new content::Tilemap(screen, viewport, tilesize););
  obj->AddRef();
  MriSetStructData(self, obj.get());

  rb_iv_set(self, "_viewport", v);

  VALUE klass = rb_const_get(rb_cObject, rb_intern("TilemapAutotiles"));
  VALUE bitmap_array = rb_obj_alloc(klass);
  MriSetStructData(bitmap_array, obj.get());

  VALUE ary = rb_ary_new2(7);
  for (int i = 0; i < 7; ++i)
    rb_ary_push(ary, Qnil);
  rb_iv_set(bitmap_array, "_autotiles", ary);

  rb_iv_set(self, "_tilemap_autotiles", bitmap_array);

  return self;
}

MRI_METHOD(tilemap_update) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);

  MRI_GUARD(obj->Update(););
  return self;
}

MRI_METHOD(tilemap_autotiles) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  MRI_GUARD(obj->CheckIsDisposed(););

  return rb_iv_get(self, "_tilemap_autotiles");
}

MRI_METHOD(tilemap_get_ox) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  int v;
  MRI_GUARD(v = obj->GetOX(););
  return rb_fix_new(v);
}

MRI_METHOD(tilemap_set_ox) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  int v;
  MriParseArgsTo(argc, argv, "i", &v);
  MRI_GUARD(obj->SetOX(v););
  return self;
}

MRI_METHOD(tilemap_get_oy) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  int v;
  MRI_GUARD(v = obj->GetOY(););
  return rb_fix_new(v);
}

MRI_METHOD(tilemap_set_oy) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  int v;
  MriParseArgsTo(argc, argv, "i", &v);
  MRI_GUARD(obj->SetOY(v););
  return self;
}

MRI_METHOD(tilemap_get_visible) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  bool v;
  MRI_GUARD(v = obj->GetVisible(););
  return v ? Qtrue : Qfalse;
}

MRI_METHOD(tilemap_set_visible) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  bool v;
  MriParseArgsTo(argc, argv, "b", &v);
  MRI_GUARD(obj->SetVisible(v););
  return self;
}

#define DEFINE_TILEMAP_ATTR_TABLE(name, iv)                      \
  MRI_METHOD(tilemap_get_##name) {                               \
    scoped_refptr<content::Tilemap> obj =                        \
        MriGetStructData<content::Tilemap>(self);                \
    return rb_iv_get(self, iv);                                  \
  }                                                              \
  MRI_METHOD(tilemap_set_##name) {                               \
    scoped_refptr<content::Tilemap> obj =                        \
        MriGetStructData<content::Tilemap>(self);                \
    VALUE v;                                                     \
    MriParseArgsTo(argc, argv, "o", &v);                         \
    scoped_refptr<content::Table> t;                             \
    if (!NIL_P(v))                                               \
      t = MriCheckStructData<content::Table>(v, kTableDataType); \
    MRI_GUARD(obj->Set##name(t););                               \
    rb_iv_set(self, iv, v);                                      \
    return v;                                                    \
  }

DEFINE_TILEMAP_ATTR_TABLE(MapData, "_map_data");
DEFINE_TILEMAP_ATTR_TABLE(FlashData, "_flash_data");
DEFINE_TILEMAP_ATTR_TABLE(Priorities, "_priorities");

MRI_METHOD(tilemap_get_Tileset) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  return rb_iv_get(self, "_tileset");
}

MRI_METHOD(tilemap_set_Tileset) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);
  VALUE v;
  MriParseArgsTo(argc, argv, "o", &v);
  scoped_refptr<content::Bitmap> t;
  if (!NIL_P(v))
    t = MriCheckStructData<content::Bitmap>(v, kBitmapDataType);
  MRI_GUARD(obj->SetTileset(t););
  rb_iv_set(self, "_tileset", v);
  return v;
}

MRI_METHOD(tilemap_get_viewport) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);

  MRI_GUARD(obj->CheckIsDisposed(););

  return rb_iv_get(self, "_viewport");
}

MRI_METHOD(tilemapautotiles_get_bitmaps) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);

  int i;
  MriParseArgsTo(argc, argv, "i", &i);
  i = std::clamp(i, 0, 6);

  VALUE ary = rb_iv_get(self, "_autotiles");
  return rb_ary_entry(ary, i);
}

MRI_METHOD(tilemapautotiles_set_bitmaps) {
  scoped_refptr<content::Tilemap> obj =
      MriGetStructData<content::Tilemap>(self);

  int i;
  VALUE o;
  MriParseArgsTo(argc, argv, "io", &i, &o);
  i = std::clamp(i, 0, 6);

  scoped_refptr<content::Bitmap> prop;
  if (!NIL_P(o))
    prop = MriCheckStructData<content::Bitmap>(o, kBitmapDataType);
  obj->SetAutotiles(i, prop);

  VALUE ary = rb_iv_get(self, "_autotiles");
  rb_ary_store(ary, i, o);

  return o;
}

void InitTilemapBinding() {
  VALUE klass = rb_define_class("Tilemap", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kTilemapDataType>);

  MriInitDisposableBinding<content::Tilemap>(klass);

  MriDefineMethod(klass, "initialize", tilemap_initialize);
  MriDefineMethod(klass, "update", tilemap_update);
  MriDefineMethod(klass, "viewport", tilemap_get_viewport);
  MriDefineMethod(klass, "autotiles", tilemap_autotiles);

  MriDefineAttr(klass, "ox", tilemap, ox);
  MriDefineAttr(klass, "oy", tilemap, oy);
  MriDefineAttr(klass, "visible", tilemap, visible);
  MriDefineAttr(klass, "map_data", tilemap, MapData);
  MriDefineAttr(klass, "flash_data", tilemap, FlashData);
  MriDefineAttr(klass, "priorities", tilemap, Priorities);
  MriDefineAttr(klass, "tileset", tilemap, Tileset);

  VALUE bitmaps = rb_define_class("TilemapAutotiles", rb_cObject);
  rb_define_alloc_func(bitmaps, MriClassAllocate<&kTilemapAutotilesDataType>);

  MriDefineAttr(bitmaps, "[]", tilemapautotiles, bitmaps);
}

}  // namespace binding

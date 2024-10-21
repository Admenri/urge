// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_window2.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/mri_template.h"
#include "content/public/window2.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Window2, "Window", content::Window2);

MRI_METHOD(window2_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();
  scoped_refptr<content::Window2> obj;

  if (MriGetGlobalRunner()->config()->content_version() >=
      content::APIVersion::RGSS3) {
    int x = 0, y = 0, w = 0, h = 0;
    if (argc == 4)
      MriParseArgsTo(argc, argv, "iiii", &x, &y, &w, &h);
    else if (argc > 0)
      MriCheckArgc(argc, 4);
    MRI_GUARD(obj = new content::Window2(screen, base::Rect(x, y, w, h)););

    obj->AddRef();
    MriSetStructData(self, obj.get());
  } else {
    obj =
        MriInitializeViewportchild<content::Window2>(screen, argc, argv, self);
  }

  MriWrapProperty(self, obj->GetCursorRect(), "_cursor_rect", kRectDataType);
  MriWrapProperty(self, obj->GetTone(), "_tone", kToneDataType);

  VALUE contents = MriWrapObject(obj->GetContents(), kBitmapDataType);
  bitmap_init_prop(obj->GetContents(), contents);
  rb_iv_set(self, "_contents", contents);

  return self;
}

MRI_METHOD(window2_update) {
  scoped_refptr<content::Window2> obj =
      MriGetStructData<content::Window2>(self);
  MRI_GUARD(obj->Update(););
  return self;
}

MRI_METHOD(window2_move) {
  scoped_refptr<content::Window2> obj =
      MriGetStructData<content::Window2>(self);
  int x, y, w, h;
  MriParseArgsTo(argc, argv, "iiii", &x, &y, &w, &h);
  obj->Move(x, y, w, h);
  return self;
}

MRI_METHOD(window2_is_opened) {
  scoped_refptr<content::Window2> obj =
      MriGetStructData<content::Window2>(self);
  bool v;
  MRI_GUARD(v = obj->IsOpened(););
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(window2_is_closed) {
  scoped_refptr<content::Window2> obj =
      MriGetStructData<content::Window2>(self);
  bool v;
  MRI_GUARD(v = obj->IsClosed(););
  return MRI_BOOL_NEW(v);
}

#define WINDOW2_DEFINE_ATTR(name, ty, f, p)       \
  MRI_METHOD(window2_get_##name) {                \
    scoped_refptr<content::Window2> obj =         \
        MriGetStructData<content::Window2>(self); \
    ty v;                                         \
    MRI_GUARD(v = obj->Get##name(););             \
    return f(v);                                  \
  }                                               \
  MRI_METHOD(window2_set_##name) {                \
    scoped_refptr<content::Window2> obj =         \
        MriGetStructData<content::Window2>(self); \
    ty v;                                         \
    MriParseArgsTo(argc, argv, p, &v);            \
    MRI_GUARD(obj->Set##name(v););                \
    return self;                                  \
  }

WINDOW2_DEFINE_ATTR(Active, bool, MRI_BOOL_NEW, "b");
WINDOW2_DEFINE_ATTR(ArrowsVisible, bool, MRI_BOOL_NEW, "b");
WINDOW2_DEFINE_ATTR(Pause, bool, MRI_BOOL_NEW, "b");
WINDOW2_DEFINE_ATTR(X, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(Y, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(Width, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(Height, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(OX, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(OY, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(Padding, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(PaddingBottom, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(Opacity, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(BackOpacity, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(ContentsOpacity, int, rb_fix_new, "i");
WINDOW2_DEFINE_ATTR(Openness, int, rb_fix_new, "i");

#define WINDOW2_DEFINE_REF_ATTR(name, iv, ty)                           \
  MRI_METHOD(window2_get_##name) {                                      \
    scoped_refptr<content::Window2> obj =                               \
        MriGetStructData<content::Window2>(self);                       \
    return rb_iv_get(self, iv);                                         \
  }                                                                     \
  MRI_METHOD(window2_set_##name) {                                      \
    MriCheckArgc(argc, 1);                                              \
    scoped_refptr<content::Window2> obj =                               \
        MriGetStructData<content::Window2>(self);                       \
    VALUE propObj = *argv;                                              \
    scoped_refptr<content::ty> prop;                                    \
    if (!NIL_P(propObj))                                                \
      prop = MriCheckStructData<content::ty>(propObj, k##ty##DataType); \
    MRI_GUARD(obj->Set##name(prop););                                   \
    rb_iv_set(self, iv, *argv);                                         \
    return propObj;                                                     \
  }

#define WINDOW2_DEFINE_VAL_ATTR(name, iv, ty)                      \
  MRI_METHOD(window2_get_##name) {                                 \
    scoped_refptr<content::Window2> obj =                          \
        MriGetStructData<content::Window2>(self);                  \
    MRI_GUARD(obj->CheckIsDisposed(););                            \
    return rb_iv_get(self, iv);                                    \
  }                                                                \
  MRI_METHOD(window2_set_##name) {                                 \
    MriCheckArgc(argc, 1);                                         \
    scoped_refptr<content::Window2> obj =                          \
        MriGetStructData<content::Window2>(self);                  \
    VALUE propObj = *argv;                                         \
    scoped_refptr<content::ty> prop =                              \
        MriCheckStructData<content::ty>(propObj, k##ty##DataType); \
    MRI_GUARD(obj->Set##name(prop););                              \
    return propObj;                                                \
  }

WINDOW2_DEFINE_REF_ATTR(Windowskin, "_windowskin", Bitmap);
WINDOW2_DEFINE_REF_ATTR(Contents, "_contents", Bitmap);

WINDOW2_DEFINE_VAL_ATTR(CursorRect, "_cursor_rect", Rect);
WINDOW2_DEFINE_VAL_ATTR(Tone, "_tone", Tone);

void InitWindow2Binding() {
  VALUE klass = rb_define_class("Window", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kWindow2DataType>);

  MriInitViewportChildBinding<content::Window2>(klass);
  MriInitDisposableBinding<content::Window2>(klass);

  MriDefineMethod(klass, "initialize", window2_initialize);
  MriDefineMethod(klass, "update", window2_update);

  MriDefineMethod(klass, "move", window2_move);
  MriDefineMethod(klass, "open?", window2_is_opened);
  MriDefineMethod(klass, "close?", window2_is_closed);

  MriDefineAttr(klass, "windowskin", window2, Windowskin);
  MriDefineAttr(klass, "contents", window2, Contents);
  MriDefineAttr(klass, "cursor_rect", window2, CursorRect);
  MriDefineAttr(klass, "active", window2, Active);
  MriDefineAttr(klass, "arrows_visible", window2, ArrowsVisible);
  MriDefineAttr(klass, "pause", window2, Pause);
  MriDefineAttr(klass, "x", window2, X);
  MriDefineAttr(klass, "y", window2, Y);
  MriDefineAttr(klass, "width", window2, Width);
  MriDefineAttr(klass, "height", window2, Height);
  MriDefineAttr(klass, "ox", window2, OX);
  MriDefineAttr(klass, "oy", window2, OY);
  MriDefineAttr(klass, "padding", window2, Padding);
  MriDefineAttr(klass, "padding_bottom", window2, PaddingBottom);
  MriDefineAttr(klass, "opacity", window2, Opacity);
  MriDefineAttr(klass, "back_opacity", window2, BackOpacity);
  MriDefineAttr(klass, "contents_opacity", window2, ContentsOpacity);
  MriDefineAttr(klass, "openness", window2, Openness);
  MriDefineAttr(klass, "tone", window2, Tone);
}

}  // namespace binding

// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_window.h"

#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/mri_template.h"
#include "content/public/window.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Window, "Window", content::Window);

MRI_METHOD(window_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  scoped_refptr<content::Window> obj =
      MriInitializeViewportchild<content::Window>(screen, argc, argv, self);
  MriWrapProperty(self, obj->GetCursorRect(), "_cursor_rect", kRectDataType);

  return self;
}

MRI_METHOD(window_update) {
  scoped_refptr<content::Window> obj = MriGetStructData<content::Window>(self);
  MRI_GUARD(obj->Update(););
  return Qnil;
}

#define WINDOW_DEFINE_ATTR(name, ty, f, p)       \
  MRI_METHOD(window_get_##name) {                \
    scoped_refptr<content::Window> obj =         \
        MriGetStructData<content::Window>(self); \
    ty v;                                        \
    MRI_GUARD(v = obj->Get##name(););            \
    return f(v);                                 \
  }                                              \
  MRI_METHOD(window_set_##name) {                \
    scoped_refptr<content::Window> obj =         \
        MriGetStructData<content::Window>(self); \
    ty v;                                        \
    MriParseArgsTo(argc, argv, p, &v);           \
    MRI_GUARD(obj->Set##name(v););               \
    return self;                                 \
  }

WINDOW_DEFINE_ATTR(Stretch, bool, MRI_BOOL_NEW, "b");
WINDOW_DEFINE_ATTR(Active, bool, MRI_BOOL_NEW, "b");
WINDOW_DEFINE_ATTR(Pause, bool, MRI_BOOL_NEW, "b");
WINDOW_DEFINE_ATTR(X, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(Y, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(Width, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(Height, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(OX, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(OY, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(Opacity, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(BackOpacity, int, rb_fix_new, "i");
WINDOW_DEFINE_ATTR(ContentsOpacity, int, rb_fix_new, "i");

#define WINDOW_DEFINE_REF_ATTR(name, iv, ty)                            \
  MRI_METHOD(window_get_##name) {                                       \
    scoped_refptr<content::Window> obj =                                \
        MriGetStructData<content::Window>(self);                        \
    return rb_iv_get(self, iv);                                         \
  }                                                                     \
  MRI_METHOD(window_set_##name) {                                       \
    MriCheckArgc(argc, 1);                                              \
    scoped_refptr<content::Window> obj =                                \
        MriGetStructData<content::Window>(self);                        \
    VALUE propObj = *argv;                                              \
    scoped_refptr<content::ty> prop;                                    \
    if (!NIL_P(propObj))                                                \
      prop = MriCheckStructData<content::ty>(propObj, k##ty##DataType); \
    MRI_GUARD(obj->Set##name(prop););                                   \
    rb_iv_set(self, iv, *argv);                                         \
    return propObj;                                                     \
  }

#define WINDOW_DEFINE_VAL_ATTR(name, iv, ty)                       \
  MRI_METHOD(window_get_##name) {                                  \
    scoped_refptr<content::Window> obj =                           \
        MriGetStructData<content::Window>(self);                   \
    MRI_GUARD(obj->CheckIsDisposed(););                            \
    return rb_iv_get(self, iv);                                    \
  }                                                                \
  MRI_METHOD(window_set_##name) {                                  \
    MriCheckArgc(argc, 1);                                         \
    scoped_refptr<content::Window> obj =                           \
        MriGetStructData<content::Window>(self);                   \
    VALUE propObj = *argv;                                         \
    scoped_refptr<content::ty> prop =                              \
        MriCheckStructData<content::ty>(propObj, k##ty##DataType); \
    MRI_GUARD(obj->Set##name(prop););                              \
    return propObj;                                                \
  }

WINDOW_DEFINE_REF_ATTR(Windowskin, "_windowskin", Bitmap);
WINDOW_DEFINE_REF_ATTR(Contents, "_contents", Bitmap);
WINDOW_DEFINE_VAL_ATTR(CursorRect, "_cursor_rect", Rect);

void InitWindowBinding() {
  VALUE klass = rb_define_class("Window", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kWindowDataType>);

  MriInitViewportChildBinding<content::Window>(klass);
  MriInitDisposableBinding<content::Window>(klass);

  MriDefineMethod(klass, "initialize", window_initialize);
  MriDefineMethod(klass, "update", window_update);

  MriDefineAttr(klass, "windowskin", window, Windowskin);
  MriDefineAttr(klass, "contents", window, Contents);
  MriDefineAttr(klass, "cursor_rect", window, CursorRect);
  MriDefineAttr(klass, "stretch", window, Stretch);
  MriDefineAttr(klass, "active", window, Active);
  MriDefineAttr(klass, "pause", window, Pause);
  MriDefineAttr(klass, "x", window, X);
  MriDefineAttr(klass, "y", window, Y);
  MriDefineAttr(klass, "width", window, Width);
  MriDefineAttr(klass, "height", window, Height);
  MriDefineAttr(klass, "ox", window, OX);
  MriDefineAttr(klass, "oy", window, OY);
  MriDefineAttr(klass, "opacity", window, Opacity);
  MriDefineAttr(klass, "back_opacity", window, BackOpacity);
  MriDefineAttr(klass, "contents_opacity", window, ContentsOpacity);
}

}  // namespace binding

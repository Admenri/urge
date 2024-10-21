// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_utility.h"

#include "binding/mri/mri_template.h"
#include "content/public/utility.h"

namespace binding {

MRI_DEFINE_DATATYPE_REF(Rect, "Rect", content::Rect);

MRI_METHOD(rect_initialize) {
  scoped_refptr<content::Rect> obj;

  if (argc) {
    int x, y, w, h;
    MriParseArgsTo(argc, argv, "iiii", &x, &y, &w, &h);

    obj = new content::Rect(base::Rect(x, y, w, h));
  } else {
    obj = new content::Rect();
  }

  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(rect_initialize_copy) {
  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  scoped_refptr<content::Rect> other_obj =
      MriGetStructData<content::Rect>(other);

  scoped_refptr<content::Rect> obj = new content::Rect(*other_obj);
  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(rect_set) {
  scoped_refptr<content::Rect> obj = MriGetStructData<content::Rect>(self);

  if (argc == 1) {
    VALUE other;
    MriParseArgsTo(argc, argv, "o", &other);
    scoped_refptr<content::Rect> other_obj =
        MriCheckStructData<content::Rect>(other, kRectDataType);

    obj->Set(other_obj);
  } else {
    int x, y, w, h;
    MriParseArgsTo(argc, argv, "iiii", &x, &y, &w, &h);

    obj->Set(base::Rect(x, y, w, h));
  }

  return self;
}

MRI_METHOD(rect_empty) {
  scoped_refptr<content::Rect> obj = MriGetStructData<content::Rect>(self);

  obj->Empty();

  return self;
}

#define RECT_ATTR_DEFINE(name)                                                \
  MRI_METHOD(rect_attr_get_##name) {                                          \
    scoped_refptr<content::Rect> obj = MriGetStructData<content::Rect>(self); \
    return rb_fix_new(obj->Get##name());                                      \
  }                                                                           \
  MRI_METHOD(rect_attr_set_##name) {                                          \
    scoped_refptr<content::Rect> obj = MriGetStructData<content::Rect>(self); \
    int v;                                                                    \
    MriParseArgsTo(argc, argv, "i", &v);                                      \
    obj->Set##name(v);                                                        \
    return self;                                                              \
  }

RECT_ATTR_DEFINE(X);
RECT_ATTR_DEFINE(Y);
RECT_ATTR_DEFINE(Width);
RECT_ATTR_DEFINE(Height);

MRI_METHOD(rect_stringify) {
  scoped_refptr<content::Rect> obj = MriGetStructData<content::Rect>(self);

  return rb_sprintf("(%d, %d, %d, %d)", obj->GetX(), obj->GetY(),
                    obj->GetWidth(), obj->GetHeight());
}

MRI_METHOD(rect_equal) {
  scoped_refptr<content::Rect> obj = MriGetStructData<content::Rect>(self);

  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  if (!rb_typeddata_is_kind_of(other, &kRectDataType))
    return Qfalse;

  scoped_refptr<content::Rect> other_obj =
      MriCheckStructData<content::Rect>(other, kRectDataType);

  return other_obj->IsSame(*obj) ? Qtrue : Qfalse;
}

void InitRectBinding() {
  VALUE klass = rb_define_class("Rect", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kRectDataType>);

  MriDefineMethod(klass, "initialize", rect_initialize);
  MriDefineMethod(klass, "initialize_copy", rect_initialize_copy);
  MriInitSerializableBinding<content::Rect>(klass);

  MriDefineAttr(klass, "x", rect_attr, X);
  MriDefineAttr(klass, "y", rect_attr, Y);
  MriDefineAttr(klass, "width", rect_attr, Width);
  MriDefineAttr(klass, "height", rect_attr, Height);

  MriDefineMethod(klass, "set", rect_set);
  MriDefineMethod(klass, "empty", rect_empty);
  MriDefineMethod(klass, "inspect", rect_stringify);
  MriDefineMethod(klass, "to_s", rect_stringify);
  MriDefineMethod(klass, "==", rect_equal);
  MriDefineMethod(klass, "===", rect_equal);
  MriDefineMethod(klass, "eql?", rect_equal);
}

MRI_DEFINE_DATATYPE_REF(Color, "Color", content::Color);

MRI_METHOD(color_initialize) {
  scoped_refptr<content::Color> obj;

  if (argc) {
    double red, green, blue, alpha = 255.0;
    MriParseArgsTo(argc, argv, "fff|f", &red, &green, &blue, &alpha);

    obj = new content::Color(red, green, blue, alpha);
  } else {
    obj = new content::Color();
  }

  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(color_initialize_copy) {
  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  scoped_refptr<content::Color> other_obj =
      MriGetStructData<content::Color>(other);

  scoped_refptr<content::Color> obj = new content::Color(*other_obj);
  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(color_set) {
  scoped_refptr<content::Color> obj = MriGetStructData<content::Color>(self);

  if (argc == 1) {
    VALUE other;
    MriParseArgsTo(argc, argv, "o", &other);
    scoped_refptr<content::Color> other_obj =
        MriCheckStructData<content::Color>(other, kColorDataType);

    obj->Set(other_obj);
  } else {
    double red, green, blue, alpha = 255.0;
    MriParseArgsTo(argc, argv, "fff|f", &red, &green, &blue, &alpha);

    obj->Set(red, green, blue, alpha);
  }

  return self;
}

#define COLOR_ATTR_DEFINE(name)                 \
  MRI_METHOD(color_attr_get_##name) {           \
    scoped_refptr<content::Color> obj =         \
        MriGetStructData<content::Color>(self); \
    return rb_float_new(obj->Get##name());      \
  }                                             \
  MRI_METHOD(color_attr_set_##name) {           \
    scoped_refptr<content::Color> obj =         \
        MriGetStructData<content::Color>(self); \
    double v;                                   \
    MriParseArgsTo(argc, argv, "f", &v);        \
    obj->Set##name(v);                          \
    return self;                                \
  }

COLOR_ATTR_DEFINE(Red);
COLOR_ATTR_DEFINE(Green);
COLOR_ATTR_DEFINE(Blue);
COLOR_ATTR_DEFINE(Alpha);

MRI_METHOD(color_stringify) {
  scoped_refptr<content::Color> obj = MriGetStructData<content::Color>(self);

  return rb_sprintf("(%f, %f, %f, %f)", obj->GetRed(), obj->GetGreen(),
                    obj->GetBlue(), obj->GetAlpha());
}

MRI_METHOD(color_equal) {
  scoped_refptr<content::Color> obj = MriGetStructData<content::Color>(self);

  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  if (!rb_typeddata_is_kind_of(other, &kColorDataType))
    return Qfalse;

  scoped_refptr<content::Color> other_obj =
      MriCheckStructData<content::Color>(other, kColorDataType);

  return other_obj->IsSame(*obj) ? Qtrue : Qfalse;
}

void InitColorBinding() {
  VALUE klass = rb_define_class("Color", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kColorDataType>);

  MriDefineMethod(klass, "initialize", color_initialize);
  MriDefineMethod(klass, "initialize_copy", color_initialize_copy);
  MriInitSerializableBinding<content::Color>(klass);

  MriDefineAttr(klass, "red", color_attr, Red);
  MriDefineAttr(klass, "green", color_attr, Green);
  MriDefineAttr(klass, "blue", color_attr, Blue);
  MriDefineAttr(klass, "alpha", color_attr, Alpha);

  MriDefineMethod(klass, "set", color_set);
  MriDefineMethod(klass, "inspect", color_stringify);
  MriDefineMethod(klass, "to_s", color_stringify);
  MriDefineMethod(klass, "==", color_equal);
  MriDefineMethod(klass, "===", color_equal);
  MriDefineMethod(klass, "eql?", color_equal);
}

MRI_DEFINE_DATATYPE_REF(Tone, "Tone", content::Tone);

MRI_METHOD(tone_initialize) {
  scoped_refptr<content::Tone> obj;

  if (argc) {
    double red, green, blue, gray = 0.0;
    MriParseArgsTo(argc, argv, "fff|f", &red, &green, &blue, &gray);

    obj = new content::Tone(red, green, blue, gray);
  } else {
    obj = new content::Tone();
  }

  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(tone_initialize_copy) {
  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  scoped_refptr<content::Tone> other_obj =
      MriGetStructData<content::Tone>(other);

  scoped_refptr<content::Tone> obj = new content::Tone(*other_obj);
  obj->AddRef();
  MriSetStructData(self, obj.get());

  return self;
}

MRI_METHOD(tone_set) {
  scoped_refptr<content::Tone> obj = MriGetStructData<content::Tone>(self);

  if (argc == 1) {
    VALUE other;
    MriParseArgsTo(argc, argv, "o", &other);
    scoped_refptr<content::Tone> other_obj =
        MriCheckStructData<content::Tone>(other, kToneDataType);

    obj->Set(other_obj);
  } else {
    double red, green, blue, gray = 0.0;
    MriParseArgsTo(argc, argv, "fff|f", &red, &green, &blue, &gray);

    obj->Set(red, green, blue, gray);
  }

  return self;
}

#define TONE_ATTR_DEFINE(name)                                                \
  MRI_METHOD(tone_attr_get_##name) {                                          \
    scoped_refptr<content::Tone> obj = MriGetStructData<content::Tone>(self); \
    return rb_float_new(obj->Get##name());                                    \
  }                                                                           \
  MRI_METHOD(tone_attr_set_##name) {                                          \
    scoped_refptr<content::Tone> obj = MriGetStructData<content::Tone>(self); \
    double v;                                                                 \
    MriParseArgsTo(argc, argv, "f", &v);                                      \
    obj->Set##name(v);                                                        \
    return self;                                                              \
  }

TONE_ATTR_DEFINE(Red);
TONE_ATTR_DEFINE(Green);
TONE_ATTR_DEFINE(Blue);
TONE_ATTR_DEFINE(Gray);

MRI_METHOD(tone_stringify) {
  scoped_refptr<content::Tone> obj = MriGetStructData<content::Tone>(self);

  return rb_sprintf("(%f, %f, %f, %f)", obj->GetRed(), obj->GetGreen(),
                    obj->GetBlue(), obj->GetGray());
}

MRI_METHOD(tone_equal) {
  scoped_refptr<content::Tone> obj = MriGetStructData<content::Tone>(self);

  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  if (!rb_typeddata_is_kind_of(other, &kToneDataType))
    return Qfalse;

  scoped_refptr<content::Tone> other_obj =
      MriCheckStructData<content::Tone>(other, kToneDataType);

  return other_obj->IsSame(*obj) ? Qtrue : Qfalse;
}

void InitToneBinding() {
  VALUE klass = rb_define_class("Tone", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kToneDataType>);

  MriDefineMethod(klass, "initialize", tone_initialize);
  MriDefineMethod(klass, "initialize_copy", tone_initialize_copy);
  MriInitSerializableBinding<content::Tone>(klass);

  MriDefineAttr(klass, "red", tone_attr, Red);
  MriDefineAttr(klass, "green", tone_attr, Green);
  MriDefineAttr(klass, "blue", tone_attr, Blue);
  MriDefineAttr(klass, "gray", tone_attr, Gray);

  MriDefineMethod(klass, "set", tone_set);
  MriDefineMethod(klass, "inspect", tone_stringify);
  MriDefineMethod(klass, "to_s", tone_stringify);
  MriDefineMethod(klass, "==", tone_equal);
  MriDefineMethod(klass, "===", tone_equal);
  MriDefineMethod(klass, "eql?", tone_equal);
}

void InitUtilityBinding() {
  InitRectBinding();
  InitColorBinding();
  InitToneBinding();
}

}  // namespace binding

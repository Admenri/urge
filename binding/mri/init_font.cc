// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_font.h"

#include "binding/mri/init_utility.h"
#include "content/public/font.h"

namespace binding {

namespace {

void CollectStrings(VALUE obj, std::vector<std::string>& out) {
  if (RB_TYPE_P(obj, RUBY_T_STRING)) {
    out.push_back(RSTRING_PTR(obj));
    return;
  }

  if (RB_TYPE_P(obj, RUBY_T_ARRAY)) {
    for (long i = 0; i < RARRAY_LEN(obj); ++i) {
      VALUE str = rb_ary_entry(obj, i);
      if (!RB_TYPE_P(str, RUBY_T_STRING))
        continue;
      out.push_back(RSTRING_PTR(str));
    }
  }
}

}  // namespace

MRI_DEFINE_DATATYPE_REF(Font, "Font", content::Font);

void font_init_prop(scoped_refptr<content::Font> font, VALUE self) {
  MriWrapProperty(self, font->GetColor(), "_color", kColorDataType);
  MriWrapProperty(self, font->GetOutColor(), "_out_color", kColorDataType);
}

MRI_METHOD(font_initialize) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();
  scoped_refptr<content::Font> font_obj;

  switch (argc) {
    default:
    case 0:
      font_obj = new content::Font(screen->font_manager());
      break;
    case 1: {
      VALUE name;
      MriParseArgsTo(argc, argv, "o", &name);

      std::vector<std::string> name_ary;
      CollectStrings(name, name_ary);

      font_obj = new content::Font(screen->font_manager(), std::move(name_ary));
    } break;
    case 2: {
      VALUE name;
      int size;
      MriParseArgsTo(argc, argv, "oi", &name, &size);

      std::vector<std::string> name_ary;
      CollectStrings(name, name_ary);

      font_obj =
          new content::Font(screen->font_manager(), std::move(name_ary), size);
    } break;
  }

  font_init_prop(font_obj, self);

  font_obj->AddRef();
  MriSetStructData(self, font_obj.get());

  return self;
}

MRI_METHOD(font_initialize_copy) {
  VALUE other;
  MriParseArgsTo(argc, argv, "o", &other);

  if (!OBJ_INIT_COPY(self, other))
    return self;

  scoped_refptr<content::Font> f = MriGetStructData<content::Font>(other);

  scoped_refptr<content::Font> obj = new content::Font(*f);
  obj->AddRef();
  MriSetStructData(self, obj.get());

  font_init_prop(obj, self);

  return self;
}

MRI_METHOD(font_get_default_name) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();
  std::vector<std::string> names = screen->font_manager()->GetDefaultName();

  VALUE ary = rb_ary_new();
  for (auto it : names) {
    rb_ary_push(ary, rb_str_new(it.c_str(), it.size()));
  }

  return ary;
}

MRI_METHOD(font_set_default_name) {
  MriCheckArgc(argc, 1);

  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  std::vector<std::string> names;
  CollectStrings(argv[0], names);

  screen->font_manager()->SetDefaultName(std::move(names));

  return argv[0];
}

MRI_METHOD(font_get_default_color) {
  return rb_iv_get(self, "_color");
}

MRI_METHOD(font_set_default_color) {
  VALUE o;
  MriParseArgsTo(argc, argv, "o", &o);

  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  scoped_refptr<content::Color> c =
      MriCheckStructData<content::Color>(o, kColorDataType);
  screen->font_manager()->SetDefaultColor(c);

  return o;
}

MRI_METHOD(font_get_default_outcolor) {
  return rb_iv_get(self, "_out_color");
}

MRI_METHOD(font_set_default_outcolor) {
  VALUE o;
  MriParseArgsTo(argc, argv, "o", &o);

  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  scoped_refptr<content::Color> c =
      MriCheckStructData<content::Color>(o, kColorDataType);
  screen->font_manager()->SetDefaultOutColor(c);

  return o;
}

#define FONT_DEFAULT_ATTR(name, type, p, f)        \
  MRI_METHOD(font_get_##name) {                    \
    scoped_refptr<content::Graphics> screen =      \
        MriGetGlobalRunner()->graphics();          \
    return f(screen->font_manager()->Get##name()); \
  }                                                \
  MRI_METHOD(font_set_##name) {                    \
    type v;                                        \
    MriParseArgsTo(argc, argv, #p, &v);            \
    scoped_refptr<content::Graphics> screen =      \
        MriGetGlobalRunner()->graphics();          \
    screen->font_manager()->Set##name(v);          \
    return self;                                   \
  }

FONT_DEFAULT_ATTR(DefaultSize, int, i, rb_fix_new);
FONT_DEFAULT_ATTR(DefaultBold, bool, b, MRI_BOOL_NEW);
FONT_DEFAULT_ATTR(DefaultItalic, bool, b, MRI_BOOL_NEW);
FONT_DEFAULT_ATTR(DefaultOutline, bool, b, MRI_BOOL_NEW);
FONT_DEFAULT_ATTR(DefaultShadow, bool, b, MRI_BOOL_NEW);

MRI_METHOD(font_get_name) {
  scoped_refptr<content::Font> obj = MriGetStructData<content::Font>(self);

  std::vector<std::string> names = obj->GetName();

  VALUE ary = rb_ary_new();
  for (auto it : names) {
    rb_ary_push(ary, rb_str_new(it.c_str(), it.size()));
  }

  return ary;
}

MRI_METHOD(font_set_name) {
  scoped_refptr<content::Font> obj = MriGetStructData<content::Font>(self);

  MriCheckArgc(argc, 1);

  std::vector<std::string> names;
  CollectStrings(argv[0], names);

  obj->SetName(std::move(names));

  return argv[0];
}

MRI_METHOD(font_get_color) {
  return rb_iv_get(self, "_color");
}

MRI_METHOD(font_set_color) {
  scoped_refptr<content::Font> obj = MriGetStructData<content::Font>(self);

  VALUE o;
  MriParseArgsTo(argc, argv, "o", &o);

  scoped_refptr<content::Color> c =
      MriCheckStructData<content::Color>(o, kColorDataType);
  obj->SetColor(c);

  return o;
}

MRI_METHOD(font_get_outcolor) {
  return rb_iv_get(self, "_out_color");
}

MRI_METHOD(font_set_outcolor) {
  scoped_refptr<content::Font> obj = MriGetStructData<content::Font>(self);

  VALUE o;
  MriParseArgsTo(argc, argv, "o", &o);

  scoped_refptr<content::Color> c =
      MriCheckStructData<content::Color>(o, kColorDataType);
  obj->SetOutColor(c);

  return o;
}

#define DEFINE_FONT_ATTR(name, ty, p, f)                                      \
  MRI_METHOD(font_get_##name) {                                               \
    scoped_refptr<content::Font> obj = MriGetStructData<content::Font>(self); \
    return f(obj->Get##name());                                               \
  }                                                                           \
  MRI_METHOD(font_set_##name) {                                               \
    scoped_refptr<content::Font> obj = MriGetStructData<content::Font>(self); \
    ty v;                                                                     \
    MriParseArgsTo(argc, argv, #p, &v);                                       \
    obj->Set##name(v);                                                        \
    return self;                                                              \
  }

DEFINE_FONT_ATTR(Size, int, i, rb_fix_new);
DEFINE_FONT_ATTR(Bold, bool, b, MRI_BOOL_NEW);
DEFINE_FONT_ATTR(Italic, bool, b, MRI_BOOL_NEW);
DEFINE_FONT_ATTR(Outline, bool, b, MRI_BOOL_NEW);
DEFINE_FONT_ATTR(Shadow, bool, b, MRI_BOOL_NEW);

void InitFontBinding() {
  VALUE klass = rb_define_class("Font", rb_cObject);
  rb_define_alloc_func(klass, MriClassAllocate<&kFontDataType>);

  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  MriWrapProperty(klass, screen->font_manager()->GetDefaultColor(), "_color",
                  kColorDataType);
  MriWrapProperty(klass, screen->font_manager()->GetDefaultOutColor(),
                  "_out_color", kColorDataType);

  MriDefineClassAttr(klass, "default_name", font, default_name);
  MriDefineClassAttr(klass, "default_color", font, default_color);
  MriDefineClassAttr(klass, "default_out_color", font, default_outcolor);
  MriDefineClassAttr(klass, "default_size", font, DefaultSize);
  MriDefineClassAttr(klass, "default_bold", font, DefaultBold);
  MriDefineClassAttr(klass, "default_italic", font, DefaultItalic);
  MriDefineClassAttr(klass, "default_outline", font, DefaultOutline);
  MriDefineClassAttr(klass, "default_shadow", font, DefaultShadow);

  MriDefineMethod(klass, "initialize", font_initialize);
  MriDefineMethod(klass, "initialize_copy", font_initialize_copy);

  MriDefineAttr(klass, "name", font, name);
  MriDefineAttr(klass, "color", font, color);
  MriDefineAttr(klass, "out_color", font, outcolor);
  MriDefineAttr(klass, "size", font, Size);
  MriDefineAttr(klass, "bold", font, Bold);
  MriDefineAttr(klass, "italic", font, Italic);
  MriDefineAttr(klass, "outline", font, Outline);
  MriDefineAttr(klass, "shadow", font, Shadow);
}

}  // namespace binding

// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_mouse.h"

#include "binding/mri/init_bitmap.h"
#include "content/public/mouse.h"

namespace binding {

struct MouseButtonSet {
  std::string name;
  int button_id;
};

const MouseButtonSet kMouseButtonSets[] = {
    {"LEFT", content::Mouse::Button::Left},
    {"MIDDLE", content::Mouse::Button::Middle},
    {"RIGHT", content::Mouse::Button::Right},
    {"X1", content::Mouse::Button::X1},
    {"X2", content::Mouse::Button::X2},
};

const int kMouseButtonSetsSize =
    sizeof(kMouseButtonSets) / sizeof(kMouseButtonSets[0]);

MRI_METHOD(mouse_update) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  mouse->Update();
  return Qnil;
}

MRI_METHOD(mouse_x) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  return INT2NUM(mouse->GetX());
}

MRI_METHOD(mouse_y) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  return INT2NUM(mouse->GetY());
}

MRI_METHOD(mouse_set_pos) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();

  int x, y;
  MriParseArgsTo(argc, argv, "ii", &x, &y);
  mouse->SetPos(x, y);

  return Qnil;
}

MRI_METHOD(mouse_get_visible) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  return MRI_BOOL_NEW(mouse->GetVisible());
}

MRI_METHOD(mouse_set_visible) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  bool v;
  MriParseArgsTo(argc, argv, "b", &v);
  mouse->SetVisible(v);
  return Qnil;
}

MRI_METHOD(mouse_is_down) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  int id;
  MriParseArgsTo(argc, argv, "i", &id);
  return MRI_BOOL_NEW(mouse->IsDown(id));
}

MRI_METHOD(mouse_is_up) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  int id;
  MriParseArgsTo(argc, argv, "i", &id);
  return MRI_BOOL_NEW(mouse->IsUp(id));
}

MRI_METHOD(mouse_is_double_click) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  int id;
  MriParseArgsTo(argc, argv, "i", &id);
  return MRI_BOOL_NEW(mouse->IsDoubleClick(id));
}

MRI_METHOD(mouse_is_pressed) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  int id;
  MriParseArgsTo(argc, argv, "i", &id);
  return MRI_BOOL_NEW(mouse->IsPressed(id));
}

MRI_METHOD(mouse_scroll_x) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  return INT2NUM(mouse->GetScrollX());
}

MRI_METHOD(mouse_scroll_y) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  return INT2NUM(mouse->GetScrollY());
}

MRI_METHOD(mouse_cursor) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();

  if (argc == 0) {
    mouse->ClearCursor();
  } else {
    VALUE b;
    int hot_x, hot_y;
    MriParseArgsTo(argc, argv, "oii", &b, &hot_x, &hot_y);
    scoped_refptr<content::Bitmap> obj =
        MriCheckStructData<content::Bitmap>(b, kBitmapDataType);
    mouse->SetCursor(obj, hot_x, hot_y);
  }

  return Qnil;
}

MRI_METHOD(mouse_moved) {
  scoped_refptr<content::Mouse> mouse = MriGetGlobalRunner()->mouse();
  return MRI_BOOL_NEW(mouse->IsMoved());
}

void InitMouseBinding() {
  VALUE module = rb_define_module("Mouse");

  MriDefineModuleFunction(module, "update", mouse_update);
  MriDefineModuleFunction(module, "x", mouse_x);
  MriDefineModuleFunction(module, "y", mouse_y);
  MriDefineModuleFunction(module, "set_pos", mouse_set_pos);
  MriDefineModuleFunction(module, "down?", mouse_is_down);
  MriDefineModuleFunction(module, "up?", mouse_is_up);
  MriDefineModuleFunction(module, "double_click?", mouse_is_double_click);
  MriDefineModuleFunction(module, "press?", mouse_is_pressed);
  MriDefineModuleFunction(module, "scroll_x", mouse_scroll_x);
  MriDefineModuleFunction(module, "scroll_y", mouse_scroll_y);
  MriDefineModuleFunction(module, "set_cursor", mouse_cursor);
  MriDefineModuleFunction(module, "moved?", mouse_moved);

  MriDefineModuleAttr(module, "visible", mouse, visible);

  for (int i = 0; i < kMouseButtonSetsSize; ++i)
    rb_const_set(module, rb_intern(kMouseButtonSets[i].name.c_str()),
                 INT2FIX(kMouseButtonSets[i].button_id));
}

}  // namespace binding

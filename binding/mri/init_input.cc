// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_input.h"

#include "binding/mri/init_utility.h"
#include "content/public/input.h"

#include <map>

namespace binding {

namespace {

struct BindingSet {
  std::string name;
  int key_id;
};

const BindingSet kKeyboardBindings[] = {
    {"DOWN", 2},   {"LEFT", 4},  {"RIGHT", 6}, {"UP", 8},

    {"A", 11},     {"B", 12},    {"C", 13},    {"X", 14},  {"Y", 15},
    {"Z", 16},     {"L", 17},    {"R", 18},

    {"SHIFT", 21}, {"CTRL", 22}, {"ALT", 23},

    {"F5", 25},    {"F6", 26},   {"F7", 27},   {"F8", 28}, {"F9", 29},
};

const int kKeyboardBindingsSize =
    sizeof(kKeyboardBindings) / sizeof(kKeyboardBindings[0]);

std::string GetButtonSymbol(int argc, VALUE* argv) {
  MriCheckArgc(argc, 1);

  std::string sym;
  if (FIXNUM_P(*argv)) {
    int key_id = FIX2INT(*argv);
    for (int i = 0; i < kKeyboardBindingsSize; ++i)
      if (kKeyboardBindings[i].key_id == key_id)
        return kKeyboardBindings[i].name;
  } else if (SYMBOL_P(*argv)) {
    MriParseArgsTo(argc, argv, "n", &sym);
  }

  return sym;
}

}  // namespace

MRI_METHOD(input_update) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  input->Update();

  return Qnil;
}

MRI_METHOD(input_is_pressed) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  std::string key = GetButtonSymbol(argc, argv);
  bool v = input->IsPressed(key);
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(input_is_triggered) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  std::string key = GetButtonSymbol(argc, argv);
  bool v = input->IsTriggered(key);
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(input_is_repeated) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  std::string key = GetButtonSymbol(argc, argv);
  bool v = input->IsRepeated(key);
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(input_dir4) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  return rb_fix_new(input->Dir4());
}

MRI_METHOD(input_dir8) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  return rb_fix_new(input->Dir8());
}

MRI_METHOD(input_is_pressed_key) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  int key = 0;
  MriParseArgsTo(argc, argv, "i", &key);
  bool v = input->KeyPressed(key);
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(input_is_triggered_key) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  int key = 0;
  MriParseArgsTo(argc, argv, "i", &key);
  bool v = input->KeyTriggered(key);
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(input_is_repeated_key) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();
  int key = 0;
  MriParseArgsTo(argc, argv, "i", &key);
  bool v = input->KeyRepeated(key);
  return MRI_BOOL_NEW(v);
}

MRI_METHOD(input_recent_pressed_keys) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  std::vector<int> keys;
  input->GetRecentPressed(keys);
  VALUE ary = rb_ary_new2(keys.size());
  for (auto& it : keys)
    rb_ary_push(ary, rb_fix_new(it));

  return ary;
}

MRI_METHOD(input_recent_triggered_keys) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  std::vector<int> keys;
  input->GetRecentTriggered(keys);
  VALUE ary = rb_ary_new2(keys.size());
  for (auto& it : keys)
    rb_ary_push(ary, rb_fix_new(it));

  return ary;
}

MRI_METHOD(input_recent_repeated_keys) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  std::vector<int> keys;
  input->GetRecentRepeated(keys);
  VALUE ary = rb_ary_new2(keys.size());
  for (auto& it : keys)
    rb_ary_push(ary, rb_fix_new(it));

  return ary;
}

MRI_METHOD(input_get_key_name) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  int key = 0;
  MriParseArgsTo(argc, argv, "i", &key);
  std::string name = input->GetKeyName(key);

  return rb_str_new(name.c_str(), name.size());
}

MRI_METHOD(input_get_keys_from) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  std::string flag;
  MriParseArgsTo(argc, argv, "s", &flag);

  std::vector<int> keys;
  input->GetKeysFromFlag(flag, keys);
  VALUE ary = rb_ary_new2(keys.size());
  for (auto& it : keys)
    rb_ary_push(ary, rb_fix_new(it));

  return ary;
}

MRI_METHOD(input_set_keys_from) {
  scoped_refptr<content::Input> input = MriGetGlobalRunner()->input();

  std::string flag;
  VALUE ary;
  MriParseArgsTo(argc, argv, "so", &flag, &ary);

  if (rb_type(ary) != RUBY_T_ARRAY)
    rb_raise(rb_eArgError, "Argument 1: Expected array");

  std::vector<int> keys;
  for (int i = 0; i < RARRAY_LEN(ary); ++i)
    keys.push_back(NUM2INT(rb_ary_entry(ary, i)));

  input->SetKeysFromFlag(flag, keys);

  return Qnil;
}

void InitInputBinding() {
  VALUE module = rb_define_module("Input");

  MriDefineModuleFunction(module, "update", input_update);
  MriDefineModuleFunction(module, "press?", input_is_pressed);
  MriDefineModuleFunction(module, "trigger?", input_is_triggered);
  MriDefineModuleFunction(module, "repeat?", input_is_repeated);
  MriDefineModuleFunction(module, "dir4", input_dir4);
  MriDefineModuleFunction(module, "dir8", input_dir8);

  MriDefineModuleFunction(module, "press_key?", input_is_pressed_key);
  MriDefineModuleFunction(module, "trigger_key?", input_is_triggered_key);
  MriDefineModuleFunction(module, "repeat_key?", input_is_repeated_key);

  MriDefineModuleFunction(module, "recent_pressed", input_recent_pressed_keys);
  MriDefineModuleFunction(module, "recent_triggered",
                          input_recent_triggered_keys);
  MriDefineModuleFunction(module, "recent_repeated",
                          input_recent_repeated_keys);

  MriDefineModuleFunction(module, "get_key_name", input_get_key_name);
  MriDefineModuleFunction(module, "get_keys_from_flag", input_get_keys_from);
  MriDefineModuleFunction(module, "set_keys_from_flag", input_set_keys_from);

  for (int i = 0; i < kKeyboardBindingsSize; ++i) {
    auto& binding_set = kKeyboardBindings[i];

    ID key = rb_intern(binding_set.name.c_str());
    rb_const_set(module, key, INT2FIX(binding_set.key_id));
  }
}

}  // namespace binding

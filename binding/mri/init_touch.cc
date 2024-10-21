// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_touch.h"

#include "content/public/touch.h"

namespace binding {

MRI_METHOD(touch_max_fingers) {
  scoped_refptr<content::Touch> touch = MriGetGlobalRunner()->touch();

  return INT2FIX(touch->GetMaxFingersNum());
}

MRI_METHOD(touch_x) {
  scoped_refptr<content::Touch> touch = MriGetGlobalRunner()->touch();
  MriCheckArgc(argc, 1);
  return rb_float_new(touch->GetHorizontalFinger(FIX2INT(*argv)));
}

MRI_METHOD(touch_y) {
  scoped_refptr<content::Touch> touch = MriGetGlobalRunner()->touch();
  MriCheckArgc(argc, 1);
  return rb_float_new(touch->GetVerticalFinger(FIX2INT(*argv)));
}

MRI_METHOD(touch_is_pressed) {
  scoped_refptr<content::Touch> touch = MriGetGlobalRunner()->touch();
  MriCheckArgc(argc, 1);
  return MRI_BOOL_NEW(touch->IsFingerPressed(FIX2INT(*argv)));
}

void InitTouchBinding() {
  VALUE module = rb_define_module("Touch");

  MriDefineModuleFunction(module, "max_fingers", touch_max_fingers);
  MriDefineModuleFunction(module, "finger_x", touch_x);
  MriDefineModuleFunction(module, "finger_y", touch_y);
  MriDefineModuleFunction(module, "finger_press?", touch_is_pressed);
}

}  // namespace binding

// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/init_audio.h"

namespace binding {

MRI_METHOD(audio_setup_midi) {
  return Qnil;
}

MRI_METHOD(audio_bgm_play) {
  return Qnil;
}

MRI_METHOD(audio_bgm_stop) {
  return Qnil;
}

MRI_METHOD(audio_bgm_fade) {
  return Qnil;
}

MRI_METHOD(audio_bgm_pos) {
  return rb_float_new(0);
}

MRI_METHOD(audio_bgs_play) {
  return Qnil;
}

MRI_METHOD(audio_bgs_stop) {
  return Qnil;
}

MRI_METHOD(audio_bgs_fade) {
  return Qnil;
}

MRI_METHOD(audio_bgs_pos) {
  return rb_float_new(0);
}

MRI_METHOD(audio_me_play) {
  return Qnil;
}

MRI_METHOD(audio_me_stop) {
  return Qnil;
}

MRI_METHOD(audio_me_fade) {
  return Qnil;
}

MRI_METHOD(audio_se_play) {
  return Qnil;
}

MRI_METHOD(audio_se_stop) {
  return Qnil;
}

MRI_METHOD(audio_reset) {
  return Qnil;
}

void InitAudioBinding() {
  VALUE module = rb_define_module("Audio");

  MriDefineModuleFunction(module, "setup_midi", audio_setup_midi);

  MriDefineModuleFunction(module, "bgm_play", audio_bgm_play);
  MriDefineModuleFunction(module, "bgm_stop", audio_bgm_stop);
  MriDefineModuleFunction(module, "bgm_fade", audio_bgm_fade);
  MriDefineModuleFunction(module, "bgm_pos", audio_bgm_pos);
  MriDefineModuleFunction(module, "bgs_play", audio_bgs_play);
  MriDefineModuleFunction(module, "bgs_stop", audio_bgs_stop);
  MriDefineModuleFunction(module, "bgs_fade", audio_bgs_fade);
  MriDefineModuleFunction(module, "bgs_pos", audio_bgs_pos);
  MriDefineModuleFunction(module, "me_play", audio_me_play);
  MriDefineModuleFunction(module, "me_stop", audio_me_stop);
  MriDefineModuleFunction(module, "me_fade", audio_me_fade);
  MriDefineModuleFunction(module, "se_play", audio_se_play);
  MriDefineModuleFunction(module, "se_stop", audio_se_stop);
  MriDefineModuleFunction(module, "__reset__", audio_reset);
}

}  // namespace binding

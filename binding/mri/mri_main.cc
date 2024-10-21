// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/mri_util.h"
#include "ruby/version.h"

#include "binding/mri/mri_main.h"

#include "binding/mri/init_audio.h"
#include "binding/mri/init_bitmap.h"
#include "binding/mri/init_corefile.h"
#include "binding/mri/init_font.h"
#include "binding/mri/init_graphics.h"
#include "binding/mri/init_input.h"
#include "binding/mri/init_mouse.h"
#include "binding/mri/init_plane.h"
#include "binding/mri/init_rgu.h"
#include "binding/mri/init_sprite.h"
#include "binding/mri/init_table.h"
#include "binding/mri/init_tilemap.h"
#include "binding/mri/init_tilemap2.h"
#include "binding/mri/init_touch.h"
#include "binding/mri/init_utility.h"
#include "binding/mri/init_viewport.h"
#include "binding/mri/init_window.h"
#include "binding/mri/init_window2.h"

#include "content/worker/engine_worker.h"

#include "binding/rpg/module_rpg1.rb.xxd"
#include "binding/rpg/module_rpg2.rb.xxd"
#include "binding/rpg/module_rpg3.rb.xxd"

#include "zlib.h"

extern "C" {
#if RAPI_FULL >= 300
void rb_call_builtin_inits();
#endif

void Init_zlib(void);
}

namespace binding {

scoped_refptr<content::EngineWorker> g_mri_manager;

namespace {

using EvalParamater = struct {
  VALUE string;
  VALUE filename;
};

// eval script, nil, title
VALUE EvalInternal(EvalParamater* arg) {
  VALUE argv[] = {arg->string, Qnil, arg->filename};
  return rb_funcall2(Qnil, rb_intern("eval"), 3, argv);
}

VALUE EvalString(VALUE string, VALUE filename, int* state) {
  EvalParamater arg = {string, filename};
  return rb_protect((VALUE(*)(VALUE))EvalInternal, (VALUE)&arg, state);
}

std::string InsertNewLines(const std::string& input, size_t interval) {
  std::string result;
  size_t length = input.length();

  for (size_t i = 0; i < length; i += interval) {
    result += input.substr(i, interval);
    if (i + interval < length)
      result += '\n';
  }

  return result;
}

void ParseExeceptionInfo(VALUE exc,
                         const BindingEngineMri::BacktraceData& btData) {
  VALUE exeception_name = rb_class_path(rb_obj_class(exc));
  VALUE backtrace = rb_funcall2(exc, rb_intern("backtrace"), 0, NULL);
  VALUE backtrace_front = rb_ary_entry(backtrace, 0);

  VALUE ds = rb_sprintf("%" PRIsVALUE ": %" PRIsVALUE " (%" PRIsVALUE ")",
                        backtrace_front, exc, exeception_name);

  std::string error_info(StringValueCStr(ds));
  content::Debug() << "[Binding] " << error_info;

  error_info = InsertNewLines(error_info, 128);
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RGU Error",
                           error_info.c_str(),
                           MriGetGlobalRunner()->window()->AsSDLWindow());
}

VALUE RgssMainCb(VALUE block) {
  rb_funcall2(block, rb_intern("call"), 0, 0);
  return Qnil;
}

VALUE RgssMainRescue(VALUE arg, VALUE exc) {
  VALUE* excRet = (VALUE*)arg;

  *excRet = exc;

  return Qnil;
}

void MriProcessReset() {
  scoped_refptr<content::EngineWorker> binding = MriGetGlobalRunner();

  content::Debug() << "[Binding] User trigger Content Reset.";
  binding->graphics()->Reset();
}

}  // namespace

MRI_METHOD(mri_rgssmain) {
  bool gc_required = false;

  while (true) {
    VALUE exc = Qnil;
    if (gc_required) {
      rb_gc_start();
      gc_required = false;
    }

    rb_rescue2((VALUE(*)(ANYARGS))RgssMainCb, rb_block_proc(),
               (VALUE(*)(ANYARGS))RgssMainRescue, (VALUE)&exc, rb_eException,
               (VALUE)0);

    if (NIL_P(exc))
      break;

    if (rb_obj_class(exc) == MriGetException(MriException::RGSSReset)) {
      gc_required = true;
      MriProcessReset();
    } else
      rb_exc_raise(exc);
  }

  return Qnil;
}

MRI_METHOD(mri_rgssstop) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalRunner()->graphics();

  while (true)
    screen->Update();

  return Qnil;
}

template <int id>
MRI_METHOD(mri_return_id) {
  return rb_fix_new(id);
}

BindingEngineMri::BindingEngineMri() {}

BindingEngineMri::~BindingEngineMri() {}

void BindingEngineMri::InitializeBinding(
    scoped_refptr<content::EngineWorker> binding_host) {
  binding_ = binding_host;
  g_mri_manager = binding_host;
  scoped_refptr<content::CoreConfigure> config = binding_->config();

  int argc = 0;
  char** argv = 0;
  ruby_sysinit(&argc, &argv);

  RUBY_INIT_STACK;
  ruby_init();

  ruby_init_loadpath();

#if RAPI_FULL >= 300
  rb_call_builtin_inits();
#endif  //! RAPI_FULL >= 300

  rb_enc_set_default_internal(rb_enc_from_encoding(rb_utf8_encoding()));
  rb_enc_set_default_external(rb_enc_from_encoding(rb_utf8_encoding()));

  MriInitException(config->content_version() >= content::APIVersion::RGSS3);

  InitCoreFileBinding();
  InitUtilityBinding();
  InitTableBinding();
  InitViewportBinding();
  InitFontBinding();
  InitBitmapBinding();
  InitPlaneBinding();
  InitSpriteBinding();
  InitGraphicsBinding();
  InitInputBinding();
  InitAudioBinding();
  if (config->content_version() >= content::APIVersion::RGSS2) {
    InitTilemap2Binding();
    InitWindow2Binding();
  } else {
    InitTilemapBinding();
    InitWindowBinding();
  }

  InitMouseBinding();
  InitTouchBinding();
  InitRGUBinding();
  Init_zlib();

  if (config->content_version() < content::APIVersion::RGSS3) {
    if (sizeof(void*) == 4) {
      MriDefineMethod(rb_cNilClass, "id", mri_return_id<4>);
      MriDefineMethod(rb_cTrueClass, "id", mri_return_id<2>);
    } else if (sizeof(void*) == 8) {
      MriDefineMethod(rb_cNilClass, "id", mri_return_id<8>);
      MriDefineMethod(rb_cTrueClass, "id", mri_return_id<20>);
    }

    rb_const_set(rb_cObject, rb_intern("TRUE"), Qtrue);
    rb_const_set(rb_cObject, rb_intern("FALSE"), Qfalse);
    rb_const_set(rb_cObject, rb_intern("NIL"), Qnil);
  }

  MriDefineModuleFunction(rb_mKernel, "rgss_main", mri_rgssmain);
  MriDefineModuleFunction(rb_mKernel, "rgss_stop", mri_rgssstop);

  switch (config->content_version()) {
    case content::APIVersion::RGSS1:
      content::Debug() << "[Binding] Content Version: RGSS1";
      rb_eval_string(module_rpg1);
      break;
    case content::APIVersion::RGSS2:
      content::Debug() << "[Binding] Content Version: RGSS2";
      rb_eval_string(module_rpg2);
      break;
    case content::APIVersion::RGSS3:
      content::Debug() << "[Binding] Content Version: RGSS3";
      rb_eval_string(module_rpg3);
      break;
    default:
      break;
  }

  VALUE debug = MRI_BOOL_NEW(config->game_debug());
  if (config->content_version() < content::APIVersion::RGSS2)
    rb_gv_set("DEBUG", debug);
  else if (config->content_version() >= content::APIVersion::RGSS2)
    rb_gv_set("TEST", debug);
  rb_gv_set("BTEST", MRI_BOOL_NEW(config->game_battle_test()));

  content::Debug() << "[Binding] CRuby Interpreter Version: "
                   << RUBY_API_VERSION_CODE;
  content::Debug() << "[Binding] CRuby Interpreter Platform: " << RUBY_PLATFORM;
}

void BindingEngineMri::RunBindingMain() {
  LoadPackedScripts();
}

void BindingEngineMri::QuitRequired() {
  rb_raise(rb_eSystemExit, "");
}

void BindingEngineMri::ResetRequired() {
  VALUE rb_eReset = MriGetException(MriException::RGSSReset);
  rb_raise(rb_eReset, "");
}

void BindingEngineMri::FinalizeBinding() {
  VALUE exc = rb_errinfo();
  if (!NIL_P(exc) && !rb_obj_is_kind_of(exc, rb_eSystemExit))
    ParseExeceptionInfo(exc, backtrace_);

  ruby_cleanup(0);

  g_mri_manager.reset();

  content::Debug() << "[Binding] Quit mri binding engine.";
}

void BindingEngineMri::LoadPackedScripts() {
  scoped_refptr<content::CoreConfigure> config = binding_->config();
  VALUE packed_scripts;

  try {
    packed_scripts = MriLoadData(config->game_scripts(), false);
  } catch (const base::Exception& exception) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "RGU Binding",
                             exception.GetErrorMessage().c_str(), nullptr);
    return;
  }

  if (!RB_TYPE_P(packed_scripts, RUBY_T_ARRAY)) {
    content::Debug() << "[Binding] Failed to read script data.";
    return;
  }

  rb_gv_set("$RGSS_SCRIPTS", packed_scripts);

  long scripts_count = RARRAY_LEN(packed_scripts);
  std::string zlib_decode_buffer(0x1000, 0);

  for (long i = 0; i < scripts_count; ++i) {
    VALUE script = rb_ary_entry(packed_scripts, i);

    // 0 -> ScriptID for Editor
    VALUE script_name = rb_ary_entry(script, 1);
    VALUE script_src = rb_ary_entry(script, 2);
    unsigned long buffer_size;

    int zlib_result = Z_OK;

    while (true) {
      uint8_t* buffer_ptr =
          reinterpret_cast<uint8_t*>(zlib_decode_buffer.data());
      buffer_size = zlib_decode_buffer.length();

      const uint8_t* source_ptr =
          reinterpret_cast<const uint8_t*>(RSTRING_PTR(script_src));
      const uint32_t source_size = RSTRING_LEN(script_src);

      zlib_result =
          uncompress(buffer_ptr, &buffer_size, source_ptr, source_size);

      buffer_ptr[buffer_size] = '\0';

      if (zlib_result != Z_BUF_ERROR)
        break;

      zlib_decode_buffer.resize(zlib_decode_buffer.size() * 2);
    }

    if (zlib_result != Z_OK) {
      static char buffer[256];
      snprintf(buffer, sizeof(buffer), "Error decoding script %ld: '%s'", i,
               RSTRING_PTR(script_name));

      content::Debug() << buffer;
      break;
    }

    rb_ary_store(script, 3, rb_str_new_cstr(zlib_decode_buffer.c_str()));
  }

  VALUE exc = rb_gv_get("$!");
  if (exc != Qnil)
    return;

  while (true) {
    for (long i = 0; i < scripts_count; ++i) {
      VALUE script = rb_ary_entry(packed_scripts, i);
      const char* script_name = RSTRING_PTR(rb_ary_entry(script, 1));
      VALUE script_src = rb_ary_entry(script, 3);

      VALUE utf8_string =
          MriStringUTF8(RSTRING_PTR(script_src), RSTRING_LEN(script_src));

      char filename_buffer[512] = {0};
      int len = snprintf(filename_buffer, sizeof(filename_buffer), "%03ld: %s",
                         i, script_name);

      VALUE filename = MriStringUTF8(filename_buffer, len);
      backtrace_.emplace(filename_buffer, script_name);

      int state;
      EvalString(utf8_string, filename, &state);
      if (state)
        break;
    }

    VALUE exc = rb_gv_get("$!");
    if (rb_obj_class(exc) != MriGetException(RGSSReset))
      break;

    MriProcessReset();
  }
}

}  // namespace binding

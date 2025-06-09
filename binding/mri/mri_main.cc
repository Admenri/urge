// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/mri_util.h"
#include "ruby/version.h"

#include "binding/mri/mri_main.h"

#include "SDL3/SDL_messagebox.h"
#include "zlib/zlib.h"

#include "binding/mri/binding_patch.h"
#include "binding/mri/mri_file.h"

#include "binding/mri/autogen_audio_binding.h"
#include "binding/mri/autogen_bitmap_binding.h"
#include "binding/mri/autogen_color_binding.h"
#include "binding/mri/autogen_display_binding.h"
#include "binding/mri/autogen_font_binding.h"
#include "binding/mri/autogen_graphics_binding.h"
#include "binding/mri/autogen_imageanimation_binding.h"
#include "binding/mri/autogen_input_binding.h"
#include "binding/mri/autogen_iostream_binding.h"
#include "binding/mri/autogen_keyevent_binding.h"
#include "binding/mri/autogen_mouse_binding.h"
#include "binding/mri/autogen_mouseevent_binding.h"
#include "binding/mri/autogen_plane_binding.h"
#include "binding/mri/autogen_rect_binding.h"
#include "binding/mri/autogen_spineevent_binding.h"
#include "binding/mri/autogen_spinesprite_binding.h"
#include "binding/mri/autogen_sprite_binding.h"
#include "binding/mri/autogen_surface_binding.h"
#include "binding/mri/autogen_table_binding.h"
#include "binding/mri/autogen_textinputevent_binding.h"
#include "binding/mri/autogen_tilemap2_binding.h"
#include "binding/mri/autogen_tilemap_binding.h"
#include "binding/mri/autogen_tilemapautotile_binding.h"
#include "binding/mri/autogen_tilemapbitmap_binding.h"
#include "binding/mri/autogen_tone_binding.h"
#include "binding/mri/autogen_touchevent_binding.h"
#include "binding/mri/autogen_urge_binding.h"
#include "binding/mri/autogen_videodecoder_binding.h"
#include "binding/mri/autogen_viewport_binding.h"
#include "binding/mri/autogen_window2_binding.h"
#include "binding/mri/autogen_window_binding.h"

#ifdef HAVE_GET_MACHINE_HASH
#include "admenri/machineid/machineid.h"
#endif

extern "C" {
void Init_zlib(void);
void Init_ruby_prof(void);
void rb_call_builtin_inits();
}

namespace binding {

content::ExecutionContext* g_current_execution_context = nullptr;
extern filesystem::IOService* g_io_service;

namespace {

VALUE EvalString(VALUE string, VALUE filename, int32_t* state) {
  using EvaluteContext = struct {
    VALUE string;
    VALUE filename;
  };

  auto evaluate = [](VALUE parameter) -> VALUE {
    EvaluteContext* context = reinterpret_cast<EvaluteContext*>(parameter);
    VALUE argv[] = {context->string, Qnil, context->filename};
    return rb_funcall2(Qnil, rb_intern("eval"), std::size(argv), argv);
  };

  EvaluteContext context = {string, filename};
  return rb_protect(evaluate, reinterpret_cast<VALUE>(&context), state);
}

base::String InsertNewLines(const base::String& input, size_t interval) {
  base::String result;
  size_t length = input.length();

  for (size_t i = 0; i < length; i += interval) {
    result += input.substr(i, interval);
    if (i + interval < length)
      result += '\n';
  }

  return result;
}

base::String ParseExeceptionInfo(VALUE exception) {
  VALUE exeception_name = rb_class_path(rb_obj_class(exception));
  VALUE exception_message = rb_obj_as_string(exception);
  VALUE backtrace = rb_funcall2(exception, rb_intern("backtrace"), 0, NULL);
  VALUE backtrace_front = rb_str_new2("Unknown Location");
  if (backtrace != Qnil)
    backtrace_front = rb_ary_entry(backtrace, 0);

  VALUE format_errors =
      rb_sprintf("%" PRIsVALUE " (%" PRIsVALUE ") :\n%" PRIsVALUE,
                 backtrace_front, exeception_name, exception_message);

  base::String error_info = StringValueCStr(format_errors);
  return InsertNewLines(error_info, 128);
}

void MriProcessReset() {
  content::ExceptionState exception_state;
  MriGetGlobalModules()->Graphics->Reset(exception_state);
  MriGetGlobalModules()->Audio->Reset(exception_state);
  MriProcessException(exception_state);
}

static VALUE RescueCallBlock(VALUE block) {
  return rb_funcall2(block, rb_intern("call"), 0, nullptr);
};

static VALUE RescueException(VALUE param, VALUE exception) {
  return *reinterpret_cast<VALUE*>(param) = exception;
};

MRI_METHOD(MRI_RGSSMain) {
  bool gc_required = false;

  while (true) {
    VALUE exception = Qnil;
    if (gc_required) {
      rb_gc_start();
      gc_required = false;
    }

    rb_rescue2(reinterpret_cast<VALUE (*)(ANYARGS)>(RescueCallBlock),
               rb_block_proc(),
               reinterpret_cast<VALUE (*)(ANYARGS)>(RescueException),
               (VALUE)&exception, rb_eException, nullptr);

    if (NIL_P(exception))
      break;

    if (rb_obj_class(exception) ==
        MriGetException(content::ExceptionCode::CODE_NUMS)) {
      gc_required = true;
      MriProcessReset();
    } else {
      gc_required = false;
      rb_exc_raise(exception);
    }
  }

  return Qnil;
}

MRI_METHOD(MRI_RGSSStop) {
  scoped_refptr<content::Graphics> screen = MriGetGlobalModules()->Graphics;

  content::ExceptionState exception_state;
  while (true)
    screen->Update(exception_state);

  return Qnil;
}

#ifdef HAVE_GET_MACHINE_HASH
MRI_METHOD(ADMENRI_getMachineHash) {
  auto hash = admenri::GetMachineHash();
  return rb_str_new(hash.data(), hash.size());
}
#endif

}  // namespace

BindingEngineMri::BindingEngineMri() : profile_(nullptr) {}

BindingEngineMri::~BindingEngineMri() = default;

void BindingEngineMri::PreEarlyInitialization(
    content::ContentProfile* profile,
    filesystem::IOService* io_service) {
  profile_ = profile;
  g_io_service = io_service;

  int32_t argc = 0;
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

  MriInitException(profile->api_version >=
                   content::ContentProfile::APIVersion::RGSS3);

  InitCoreFileBinding();
  InitAudioBinding();
  InitBitmapBinding();
  InitColorBinding();
  InitDisplayBinding();
  InitFontBinding();
  InitGraphicsBinding();
  InitImageAnimationBinding();
  InitInputBinding();
  InitIOStreamBinding();
  InitKeyEventBinding();
  InitMouseBinding();
  InitMouseEventBinding();
  InitPlaneBinding();
  InitRectBinding();
  InitSpineEventBinding();
  InitSpineSpriteBinding();
  InitSpriteBinding();
  InitSurfaceBinding();
  InitTableBinding();
  InitTextInputEventBinding();
  InitTilemapBinding();
  InitTilemapAutotileBinding();
  InitTilemapBitmapBinding();
  InitTilemap2Binding();
  InitToneBinding();
  InitTouchEventBinding();
  InitVideoDecoderBinding();
  InitViewportBinding();
  InitWindowBinding();
  InitWindow2Binding();
  InitURGEBinding();

  Init_zlib();
  Init_ruby_prof();

  MriApplyBindingPatch();

  if (profile->api_version < content::ContentProfile::APIVersion::RGSS3) {
    if constexpr (sizeof(void*) == 4) {
      MriDefineMethod(rb_cNilClass, "id", MriReturnInt<4>);
      MriDefineMethod(rb_cTrueClass, "id", MriReturnInt<2>);
    } else if constexpr (sizeof(void*) == 8) {
      MriDefineMethod(rb_cNilClass, "id", MriReturnInt<8>);
      MriDefineMethod(rb_cTrueClass, "id", MriReturnInt<20>);
    } else {
      NOTREACHED();
    }

    rb_const_set(rb_cObject, rb_intern("TRUE"), Qtrue);
    rb_const_set(rb_cObject, rb_intern("FALSE"), Qfalse);
    rb_const_set(rb_cObject, rb_intern("NIL"), Qnil);
  }

  MriDefineModuleFunction(rb_mKernel, "rgss_main", MRI_RGSSMain);
  MriDefineModuleFunction(rb_mKernel, "rgss_stop", MRI_RGSSStop);

#ifdef HAVE_GET_MACHINE_HASH
  VALUE class_admenri = rb_define_module("Admenri");
  MriDefineModuleFunction(class_admenri, "get_machine_hash",
                          ADMENRI_getMachineHash);
#endif

  LOG(INFO) << "[Binding] CRuby Interpreter Version: " << RUBY_API_VERSION_CODE;
  LOG(INFO) << "[Binding] CRuby Interpreter Platform: " << RUBY_PLATFORM;

  VALUE debug = MRI_BOOL_VALUE(profile->game_debug);
  if (profile->api_version < content::ContentProfile::APIVersion::RGSS2)
    rb_gv_set("DEBUG", debug);
  else
    rb_gv_set("TEST", debug);
  rb_gv_set("BTEST", MRI_BOOL_VALUE(profile->game_battle_test));
}

void BindingEngineMri::OnMainMessageLoopRun(
    content::ExecutionContext* execution,
    ScopedModuleContext* module_context) {
  // Set global execution context
  g_current_execution_context = execution;

  // Define running modules
  MriGetGlobalModules()->Graphics = module_context->graphics;
  MriGetGlobalModules()->Input = module_context->input;
  MriGetGlobalModules()->Audio = module_context->audio;
  MriGetGlobalModules()->Mouse = module_context->mouse;
  MriGetGlobalModules()->URGE = module_context->engine;

  // Run packed scripts
  content::ExceptionState exception_state;
  LoadPackedScripts(profile_, exception_state);
  if (exception_state.HadException()) {
    base::String error_message;
    exception_state.FetchException(error_message);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Scripts Error",
                             error_message.c_str(), nullptr);
  }
}

void BindingEngineMri::PostMainLoopRunning() {
  // Show exception info
  VALUE exception = rb_errinfo();
  base::String exception_message;
  if (!NIL_P(exception) && !rb_obj_is_kind_of(exception, rb_eSystemExit))
    exception_message = ParseExeceptionInfo(exception);

  // Clean up ruby vm
  ruby_cleanup(0);

  // Release binding reference
  g_current_execution_context = nullptr;
  GlobalModules empty_modules;
  std::swap(*MriGetGlobalModules(), empty_modules);

  // Show message box for errors
  if (!exception_message.empty())
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Scripts Error",
                             exception_message.c_str(), nullptr);

  LOG(INFO) << "[Binding] Quit CRuby binding engine.";
}

void BindingEngineMri::ExitSignalRequired() {
  rb_raise(rb_eSystemExit, "");
}

void BindingEngineMri::ResetSignalRequired() {
  VALUE rb_eReset = MriGetException(content::ExceptionCode::CODE_NUMS);
  rb_raise(rb_eReset, "");
}

void BindingEngineMri::LoadPackedScripts(
    content::ContentProfile* profile,
    content::ExceptionState& exception_state) {
  VALUE packed_scripts = MriLoadData(profile->script_path, exception_state);
  if (exception_state.HadException())
    return;

  if (!RB_TYPE_P(packed_scripts, RUBY_T_ARRAY)) {
    LOG(INFO) << "[Binding] Failed to read script data.";
    return;
  }

  rb_gv_set("$RGSS_SCRIPTS", packed_scripts);

  int32_t scripts_count = RARRAY_LEN(packed_scripts);
  base::String zlib_decode_buffer(0x1000, 0);

  for (int32_t i = 0; i < scripts_count; ++i) {
    VALUE script = rb_ary_entry(packed_scripts, i);

    // 0 -> ScriptID for Editor
    VALUE script_name = rb_ary_entry(script, 1);
    VALUE script_src = rb_ary_entry(script, 2);

    unsigned long buffer_size;

    int32_t zlib_result = Z_OK;

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
      LOG(INFO) << "Error when decoding: " << StringValueCStr(script_name);
      break;
    }

    rb_ary_store(script, 3, rb_str_new_cstr(zlib_decode_buffer.c_str()));
  }

  for (;;) {
    for (int32_t i = 0; i < scripts_count; ++i) {
      VALUE script = rb_ary_entry(packed_scripts, i);
      VALUE script_name = rb_ary_entry(script, 1);
      VALUE script_source = rb_ary_entry(script, 3);

      std::stringstream format_filename;
      format_filename << std::to_string(i + 1) << " - "
                      << StringValueCStr(script_name);

      int32_t state = 0;
      base::String filename = format_filename.str().c_str();
      EvalString(
          MriStringUTF8(RSTRING_PTR(script_source), RSTRING_LEN(script_source)),
          MriStringUTF8(filename.data(), filename.size()), &state);
      if (state)
        break;
    }

    VALUE exception = rb_errinfo();
    if (rb_obj_class(exception) !=
        MriGetException(content::ExceptionCode::CODE_NUMS))
      break;

    MriProcessReset();
    rb_gc_start();
  }
}

}  // namespace binding

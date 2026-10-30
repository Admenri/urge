// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "bind/cruby_main.h"

#include "bind/autogen/binding_init.h"
#include "bind/cruby_utils.h"
#include "bind/script_loop.h"
#include "core/common/logger.h"
#include "core/graphics/graphics.h"
#include "core/system/config.h"
#include "core/system/filesystem.h"

#include "bind/rpg_rgss1.h"
#include "bind/rpg_rgss2.h"
#include "bind/rpg_rgss3.h"

#include "zlib.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <vector>

#include "SDL3/SDL_timer.h"

extern "C" void rb_call_builtin_inits(void);
extern "C" void Init_enc(void);
extern "C" void Init_ext(void);
extern "C" void Init_zlib(void);

namespace binding {

namespace {

void SetupRuntimeSwitches(const urge::Config& config) {
  if (config.xp() && config.HasCommandLine("debug"))
    rb_gv_set("DEBUG", Qtrue);
  if (!config.xp() && config.HasCommandLine("test"))
    rb_gv_set("TEST", Qtrue);
  if (config.HasCommandLine("btest"))
    rb_gv_set("BTEST", Qtrue);
}

static VALUE RGSSLoadData(urge::ExceptionState& exception_state,
                          const char* filename) {
  auto stream = urge::IOService::Get().OpenReadRaw(exception_state, filename);
  if (exception_state.HadException())
    return Qnil;
  size_t data_size = 0;
  auto data_ptr = SDL_LoadFile_IO(stream, &data_size, true);

  VALUE marshal_klass = rb_const_get(rb_cObject, rb_intern("Marshal"));
  VALUE data = rb_str_new(reinterpret_cast<char*>(data_ptr), data_size);
  VALUE result = rb_funcall(marshal_klass, rb_intern("load"), 1, data);

  SDL_free(data_ptr);

  return result;
}

RB_FUNC(marshal_load_utf8) {
  VALUE port, proc = Qnil;
  ParseArgs(argc, argv, "o|o", &port, &proc);

  auto string_utf8 = [](VALUE yielded, VALUE self, int argc, const VALUE* argv,
                        VALUE blockarg) -> VALUE {
    if (RB_TYPE_P(yielded, RUBY_T_STRING))
      rb_enc_associate(yielded, rb_utf8_encoding());
    return yielded;
  };

  auto utf8_conv = [](VALUE yielded, VALUE self, int argc, const VALUE* argv,
                      VALUE blockarg) -> VALUE {
    if (RB_TYPE_P(yielded, RUBY_T_STRING))
      rb_enc_associate(yielded, rb_utf8_encoding());
    return rb_funcall(self, rb_intern("call"), 1, yielded);
  };

  VALUE utf8Proc;
  if (NIL_P(proc))
    utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(+string_utf8), Qnil);
  else
    utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(+utf8_conv), proc);

  VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));
  return rb_funcall(marsh, rb_intern("_load_utf8_"), 2, port, utf8Proc);
}

RB_FUNC(rgss_main) {
  return rb_funcall(rb_block_proc(), rb_intern("call"), 0);
}

RB_FUNC(rgss_stop) {
  urge::ExceptionState exception_state;
  for (;;) {
    urge::Graphics::Get().Update(exception_state);
    RaiseException(exception_state);
  }
  return Qnil;
}

RB_FUNC(load_data) {
  const char* filename = nullptr;
  ParseArgs(argc, argv, "z", &filename);

  urge::ExceptionState exception_state;
  VALUE result = RGSSLoadData(exception_state, filename);
  RaiseException(exception_state);

  return result;
}

RB_FUNC(save_data) {
  VALUE data = Qnil;
  const char* filename = nullptr;
  ParseArgs(argc, argv, "oz", &data, &filename);

  VALUE dumped = rb_marshal_dump(data, Qnil);
  const char* bytes = RSTRING_PTR(dumped);
  long length = RSTRING_LEN(dumped);

  {
    urge::ExceptionState exception_state;
    auto stream = urge::IOService::Get().OpenWrite(exception_state, filename);
    if (stream)
      SDL_SaveFile_IO(stream, bytes, length, true);
    RaiseException(exception_state);
  }

  return Qnil;
}

std::string RubyExceptionMessage(VALUE exception) {
  VALUE message = rb_obj_as_string(exception);
  std::string result = rb_class2name(CLASS_OF(exception));
  result += ": ";
  result.append(RSTRING_PTR(message), RSTRING_LEN(message));
  VALUE backtrace = rb_funcall(exception, rb_intern("backtrace"), 0);
  if (!NIL_P(backtrace)) {
    VALUE joined =
        rb_funcall(backtrace, rb_intern("join"), 1, rb_str_new2("\n"));
    result += "\n";
    result.append(RSTRING_PTR(joined), RSTRING_LEN(joined));
  }
  return result;
}

VALUE EvalString(VALUE string, VALUE filename, int32_t* state) {
  using EvaluteContext = struct {
    VALUE string;
    VALUE filename;
  };

  auto evaluate = [](VALUE parameter) -> VALUE {
    EvaluteContext* context = reinterpret_cast<EvaluteContext*>(parameter);
    VALUE argv[] = {context->string, context->filename};
    return rb_funcall2(Qnil, rb_intern("__urge_eval_script"), std::size(argv),
                       argv);
  };

  EvaluteContext context = {string, filename};
  return rb_protect(evaluate, reinterpret_cast<VALUE>(&context), state);
}

struct LoopState {
  enum class Command { kNone, kRun, kCleanup };
  Command command = Command::kNone;

  bool initialized = false;
  bool finished = false;
  bool cleaned_up = false;

  bool failed = false;
  bool exited = false;
  std::string message;

  double smooth = 1.0;
  double elapsed = 0.0;
  uint64_t last_tick = 0;
};

LoopState g_loop;

urge::ExceptionState* g_init_exception = nullptr;

int32_t FramesDue(double delta_rate) {
  g_loop.smooth = g_loop.smooth * 0.8 + std::min(delta_rate, 2.0) * 0.2;

  if (g_loop.smooth >= 0.9) {
    g_loop.elapsed = 0.0;
    return static_cast<int32_t>(std::lround(g_loop.smooth));
  }

  g_loop.elapsed += delta_rate;
  if (g_loop.elapsed >= 1.0) {
    g_loop.elapsed -= 1.0;
    return 1;
  }

  return 0;
}

#if !defined(URGE_RUNNER_POLL)

void PresentPacer() {
  script_loop::YieldToHost();
}

#endif

void RecordFailure(VALUE exception) {
  g_loop.failed = true;
  g_loop.exited = rb_obj_class(exception) == rb_eSystemExit;
  g_loop.message = RubyExceptionMessage(exception);
  rb_set_errinfo(Qnil);
}

VALUE EvaluateScripts(VALUE scripts) {
  for (long i = 0; i < RARRAY_LEN(scripts); ++i) {
    VALUE script = rb_ary_entry(scripts, i);
    if (!RB_TYPE_P(script, T_ARRAY) || RARRAY_LEN(script) < 3)
      continue;

    int state = 0;
    EvalString(rb_ary_entry(script, 2), rb_ary_entry(script, 1), &state);
    if (state) {
      RecordFailure(rb_errinfo());
      break;
    }
  }

  return Qnil;
}

void RunScripts() {
  VALUE scripts = rb_gv_get("$RGSS_SCRIPTS");
  if (!RB_TYPE_P(scripts, T_ARRAY)) {
    g_loop.failed = true;
    g_loop.message = "invalid scripts file.";
    return;
  }

  int state = 0;
  rb_protect(EvaluateScripts, scripts, &state);
  if (state && !g_loop.failed)
    RecordFailure(rb_errinfo());
}

}

#if defined(OS_EMSCRIPTEN)
namespace {

constexpr int kCollectInterval = 60;

void CollectGarbage() {
  static int frames = 0;
  if (++frames < kCollectInterval)
    return;
  frames = 0;

  rb_gc_enable();
  rb_gc_start();
  rb_gc_disable();
}

}
#endif

void BindingMain::InitializeInterpreter(urge::ExceptionState& exception_state) {
  auto& config = urge::Config::Get();

  int argc = 0;
  char** argv = 0;
  ruby_sysinit(&argc, &argv);

#if defined(OS_EMSCRIPTEN)
  setenv("RUBY_FIBER_MACHINE_STACK_SIZE", "4194304", 1);
#endif

  LOGGER_INFO("[Ruby] starting the interpreter");
  RUBY_INIT_STACK;
  ruby_init();
  Init_enc();
  Init_ext();
  rb_call_builtin_inits();
  Init_zlib();
  rb_provide("zlib.so");
  LOGGER_INFO("[Ruby] VM ready");

#if defined(OS_EMSCRIPTEN)
  rb_gc_disable();
  urge::Graphics::Get().SetFrameCallback(&CollectGarbage);
#endif

  rb_enc_set_default_internal(rb_enc_from_encoding(rb_utf8_encoding()));
  rb_enc_set_default_external(rb_enc_from_encoding(rb_utf8_encoding()));

  SetupRuntimeSwitches(config);

  g_reset_exception = rb_define_class("RGSSReset", rb_eStandardError);
  g_rgss_exception = rb_define_class("RGSSError", rb_eStandardError);

  VALUE marshal_klass = rb_const_get(rb_cObject, rb_intern("Marshal"));
  rb_define_alias(rb_singleton_class(marshal_klass), "_load_utf8_", "load");
  DefineModuleFunction(marshal_klass, "load", marshal_load_utf8);

  DefineModuleFunction(rb_mKernel, "rgss_main", rgss_main);
  DefineModuleFunction(rb_mKernel, "rgss_stop", rgss_stop);
  DefineModuleFunction(rb_mKernel, "load_data", load_data);
  DefineModuleFunction(rb_mKernel, "save_data", save_data);

  rb_eval_string_protect(
      "def __urge_eval_script(source, filename)\n"
      "  eval(source, nil, filename)\n"
      "end\n",
      &error_state_);
  if (error_state_) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "failed to define the script evaluator.");
    return;
  }

  InitBindings();

  auto tilemap_klass = rb_const_get(
      rb_cObject, rb_intern(config.xp() ? "TilemapXP" : "TilemapVX"));
  auto window_klass = rb_const_get(
      rb_cObject, rb_intern(config.xp() ? "WindowXP" : "WindowVX"));

  rb_const_set(rb_cObject, rb_intern("Tilemap"), tilemap_klass);
  rb_const_set(rb_cObject, rb_intern("Window"), window_klass);

  rb_const_set(rb_cObject, rb_intern("RGSS_VERSION"),
               LONG2NUM(config.game.rgss));

  const char* rpg_source = config.xp()   ? rpg_rgss1
                           : config.vx() ? rpg_rgss2
                                         : rpg_rgss3;
  rb_eval_string_protect(rpg_source, &error_state_);
  if (error_state_) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "failed to load RPG database.");
    return;
  }

  VALUE scripts = RGSSLoadData(exception_state, config.game.scripts.c_str());
  if (exception_state.HadException())
    return;
  if (!RB_TYPE_P(scripts, T_ARRAY)) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "invalid scripts file.");
    return;
  }

  std::vector<uint8_t> buffer(1024);
  for (long i = 0; i < RARRAY_LEN(scripts); ++i) {
    VALUE script = rb_ary_entry(scripts, i);
    if (!RB_TYPE_P(script, T_ARRAY) || RARRAY_LEN(script) < 3)
      continue;

    VALUE compressed = rb_ary_entry(script, 2);
    if (!RB_TYPE_P(compressed, T_STRING))
      continue;

    int result = 0;
    uLongf input_size = RSTRING_LEN(compressed);
    uLongf output_size;
    do {
      output_size = static_cast<uLongf>(buffer.size());
      result = ::uncompress(
          buffer.data(), &output_size,
          reinterpret_cast<const Bytef*>(RSTRING_PTR(compressed)), input_size);
      if (result == Z_BUF_ERROR)
        buffer.resize(buffer.size() + 1024);
    } while (result == Z_BUF_ERROR);
    if (result != Z_OK) {
      exception_state.Throw(urge::ExceptionState::kRGSSError,
                            "failed to decompress script");
      return;
    }

    VALUE script_str =
        rb_str_new(reinterpret_cast<const char*>(buffer.data()), output_size);
    script_str = TagUTF8IfValid(script_str);
    rb_ary_store(script, 2, script_str);
  }

  rb_gv_set("$RGSS_SCRIPTS", scripts);
}

#if !defined(URGE_RUNNER_POLL)

void BindingMain::RunOnLoopStack() {
  InitializeInterpreter(*g_init_exception);
  g_init_exception = nullptr;
  g_loop.initialized = true;
  script_loop::YieldToHost();

  if (g_loop.command == LoopState::Command::kRun) {
    g_loop.command = LoopState::Command::kNone;
    RunScripts();
    g_loop.finished = true;
    script_loop::YieldToHost();
  }

  if (g_loop.command == LoopState::Command::kCleanup) {
    g_loop.command = LoopState::Command::kNone;
    ruby_cleanup(error_state_);
    g_loop.cleaned_up = true;
    script_loop::YieldToHost();
  }
}

void BindingMain::LoopEntry(void* arg) {
  static_cast<BindingMain*>(arg)->RunOnLoopStack();
}

#endif

#if defined(URGE_RUNNER_POLL)

BindingMain::BindingMain(urge::ExceptionState& exception_state) {
  InitializeInterpreter(exception_state);
  if (!exception_state.HadException())
    g_loop.initialized = true;
}

BindingMain::Tick BindingMain::Run() {
  if (!g_loop.initialized)
    return Tick::kFailed;

  RunScripts();
  g_loop.finished = true;

  if (g_loop.failed && !g_loop.exited)
    return Tick::kFailed;
  return Tick::kFinished;
}

BindingMain::~BindingMain() {
  if (auto* graphics = urge::Graphics::TryGet())
    graphics->SetPresentCallback(nullptr);

  if (g_loop.cleaned_up)
    return;

  g_loop.cleaned_up = true;
  ruby_cleanup(error_state_);
}

#else

BindingMain::BindingMain(urge::ExceptionState& exception_state) {
  if (!script_loop::Start(&BindingMain::LoopEntry, this)) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "failed to allocate the script stack.");
    return;
  }

  g_init_exception = &exception_state;
  script_loop::Resume();
  g_init_exception = nullptr;

  if (!exception_state.HadException()) {
    g_loop.command = LoopState::Command::kRun;
    g_loop.last_tick = SDL_GetPerformanceCounter();
    urge::Graphics::Get().SetPresentCallback(&PresentPacer);
  }
}

BindingMain::~BindingMain() {
  if (auto* graphics = urge::Graphics::TryGet())
    graphics->SetPresentCallback(nullptr);

  if (!script_loop::Started())
    return;

  if (!g_loop.initialized || g_loop.finished) {
    g_loop.command = LoopState::Command::kCleanup;
    while (!g_loop.cleaned_up)
      script_loop::Resume();
  }

  script_loop::Finish();
}

#endif

#if !defined(URGE_RUNNER_POLL)

namespace {

BindingMain::Tick OutcomeOfFinishedLoop() {
  if (g_loop.failed && !g_loop.exited)
    return BindingMain::Tick::kFailed;
  return BindingMain::Tick::kFinished;
}

}

BindingMain::Tick BindingMain::IterateMainLoop() {
  if (g_loop.finished)
    return OutcomeOfFinishedLoop();

  const uint64_t now = SDL_GetPerformanceCounter();
  const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
  const double delta = static_cast<double>(now - g_loop.last_tick) / freq;
  g_loop.last_tick = now;

  const int32_t rate = std::max(urge::Graphics::Get().frame_rate(), 1);
  const double period = 1.0 / static_cast<double>(rate);

  if (urge::Graphics::Get().ConsumeFrameReset() || delta > period * 2) {
    g_loop.smooth = 1.0;
    g_loop.elapsed = 0.0;
  }

  int32_t repeat = FramesDue(delta / period);

  if (!urge::Graphics::Get().frame_skip() && repeat > 1)
    repeat = 1;

  for (int32_t i = 0; i < repeat && !g_loop.finished; ++i)
    script_loop::Resume();

  urge::Graphics::Get().RunHostTasks();

  if (g_loop.finished)
    return OutcomeOfFinishedLoop();

  return Tick::kContinue;
}

#endif

const std::string& BindingMain::ErrorMessage() const {
  return g_loop.message;
}

}

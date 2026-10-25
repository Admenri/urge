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

#include "bind/binding_init.h"
#include "bind/cruby_utils.h"
#include "core/system/config.h"
#include "core/system/filesystem.h"
#include "core/graphics/graphics.h"
#include "core/common/logger.h"

#include "bind/rpg_rgss1.h"
#include "bind/rpg_rgss2.h"
#include "bind/rpg_rgss3.h"

#include "zlib.h"

// `ruby_init()` only runs the C initialisers (`rb_call_inits`).  Two further
// steps of the normal start-up path live in `ruby_process_options()`, which the
// engine never calls because it drives the VM by hand:
//
//   * `rb_call_builtin_inits()` installs the core written in Ruby and compiled
//     into the interpreter as "builtin" iseqs (`array.rb`, `hash.rb`,
//     `kernel.rb`, ...).  Without it those methods simply do not exist and a
//     game fails with NoMethodError on calls the interpreter is expected to
//     answer.
//   * `Init_enc()` runs the encoding database (`enc/encdb.c`), which is what
//     defines `Encoding::UTF_8` and the other encoding constants.  Without it
//     `Encoding::UTF_8` is an uninitialized constant and every
//     `str.encoding == Encoding::UTF_8` check in a script raises NameError.
//
// `Init_ext()` is the third: a core-only build links the no-op stub from
// dmyext.c, but calling it keeps the sequence identical to the interpreter's
// own and leaves room for statically linked extensions later.
//
// All three are internal to the interpreter -- `inits.c`, `enc/encdb.c` and
// `dmyext.c` define them, but no public header names them -- hence the
// hand-written declarations.
extern "C" void rb_call_builtin_inits(void);
extern "C" void Init_enc(void);
extern "C" void Init_ext(void);

namespace binding {

namespace {

/*! Mirrors the RGSS runtime switches onto the globals a game reads.  RGSS1
    takes its debug mode from `debug`, RGSS2/3 take their test mode from
    `test`, and `btest` is the battle test every version understands.  A
    switch that is absent leaves its global alone, so `$TEST` / `$BTEST` stay
    `nil` -- falsy -- exactly as a script that merely tests them expects. */
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

  /* `rb_proc_new` takes a block function -- (yielded, data, argc, argv,
     blockarg) -- and calls it with all five.  A narrower C function happens to
     work on a native target because the extra arguments are ignored, but on
     wasm an indirect call is typed, so a two-argument function called through a
     five-argument slot traps with "function signature mismatch". */
  auto string_utf8 = [](VALUE yielded, VALUE self, int argc, const VALUE *argv,
                        VALUE blockarg) -> VALUE {
    if (RB_TYPE_P(yielded, RUBY_T_STRING))
      rb_enc_associate(yielded, rb_utf8_encoding());
    return yielded;
  };

  auto utf8_conv = [](VALUE yielded, VALUE self, int argc, const VALUE *argv,
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
  // `rgss_main` in this engine runs its block once; a game's own loop around
  // `Graphics.update` is what repeats.  Nothing has to be marked here -- the
  // per-frame interpreter time is the gap between two `Graphics.update` calls,
  // which Graphics::Update measures into FramePhase::kScript.
  VALUE block = rb_block_proc();
  VALUE result = rb_funcall(block, rb_intern("call"), 0);
  return result;
}

RB_FUNC(rgss_stop) {
  urge::ExceptionState exception_state;
  for (;;) {
    urge::Graphics::Get().Update(exception_state);
    RaiseException(exception_state);
  }
  return Qnil;
}

// `load_data` / `save_data` reach the engine through `IOService`, which
// reports a failure by throwing `urge::Exception` (a C++ exception).  Ruby's
// `rescue` is tag based -- it only ever sees an exception raised with
// `rb_raise` -- so a C++ exception unwinds straight through every Ruby frame,
// past the script's own `begin/rescue`, and only stops at the host's outermost
// `catch` in app.cc, which answers with an error message box.  The script
// therefore cannot handle a missing file, and a game that probes for an
// optional save file dies instead of rescuing.  `EXC_BEGIN`/`EXC_END` is the
// binding layer's single conversion point: it turns the C++ exception into the
// Ruby one `ProcessException` maps it to -- an IO error becomes `Errno::ENOENT`
// -- so `rescue` works as it does for every other binding.  The helper
// `RGSSLoadData` itself is left bare: the constructor calls it for the game's
// own scripts, where a failure is fatal and must still reach the host.
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

  // The marshal dump is a Ruby -> C step, so it belongs in phase 1 where a
  // raise is still legal; phase 2 only writes bytes out.
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
    VALUE argv[] = {context->string, Qnil, context->filename};
    return rb_funcall2(Qnil, rb_intern("eval"), std::size(argv), argv);
  };

  EvaluteContext context = {string, filename};
  return rb_protect(evaluate, reinterpret_cast<VALUE>(&context), state);
}

}  // namespace

#if defined(OS_EMSCRIPTEN)
namespace {

/* A full mark and sweep every frame would cost more than the frame itself, and
   the interpreter allocates little enough between frames that a second of them
   fits comfortably in the heap. */
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

BindingMain::BindingMain(urge::ExceptionState& exception_state) {
  auto& config = urge::Config::Get();

  int argc = 0;
  char** argv = 0;
  ruby_sysinit(&argc, &argv);

  RUBY_INIT_STACK;
  ruby_init();
  Init_enc();
  Init_ext();
  rb_call_builtin_inits();

#if defined(OS_EMSCRIPTEN)
  /* The interpreter's garbage collector is conservative: it finds the objects
     C code is holding by scanning the machine stack.  On wasm a local variable
     lives in a wasm *local*, not in linear memory, so a VALUE that only sits in
     a local across an allocation is invisible to that scan and is collected
     while still in use -- which is what makes the heap rot at -O1 and above
     (`class Foo; end` under `GC.stress` is enough to see it).

     The fix is to collect only where that cannot happen: automatic collection
     is switched off here, and `CollectGarbage` runs a full collection from the
     frame callback, at the point where the script has returned from
     `Graphics.update` and everything alive is reachable from the interpreter's
     own stacks and globals.  See Graphics::SetFrameCallback. */
  rb_gc_disable();
  urge::Graphics::Get().SetFrameCallback(&CollectGarbage);
#endif

  rb_enc_set_default_internal(rb_enc_from_encoding(rb_utf8_encoding()));
  rb_enc_set_default_external(rb_enc_from_encoding(rb_utf8_encoding()));

  // The switches a game reads are in place before any script is evaluated.
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

  InitBindings();

  auto tilemap_klass = rb_const_get(
      rb_cObject, rb_intern(config.xp() ? "TilemapXP" : "TilemapVX"));
  auto window_klass = rb_const_get(
      rb_cObject, rb_intern(config.xp() ? "WindowXP" : "WindowVX"));

  /* The aliases have to live in `Object`, not in `Kernel`.  Reading a constant
     finds one in `Kernel` just as well -- it is an ancestor of `Object` -- but
     *defining* a class does not: `class Foo` resolves the name with
     `rb_const_defined_at(Object, :Foo)`, which never looks at ancestors.  With
     the alias parked in `Kernel`, a game that reopens the class the way every
     RGSS script does -- `class Window`, to add a method -- silently creates a
     second, empty `Object::Window` that shadows the engine's one, and the next
     `class Window_Base < Window` compares the fresh class against the
     `WindowVX` superclass the earlier definition got and raises
     "TypeError: superclass mismatch for class Window_Base". */
  rb_const_set(rb_cObject, rb_intern("Tilemap"), tilemap_klass);
  rb_const_set(rb_cObject, rb_intern("Window"), window_klass);

  rb_const_set(rb_cObject, rb_intern("RGSS_VERSION"),
               LONG2NUM(config.game.rgss));

  // RPG database
  const char* rpg_source = config.xp()   ? rpg_rgss1
                           : config.vx() ? rpg_rgss2
                                         : rpg_rgss3;
  rb_eval_string_protect(rpg_source, &error_state_);
  if (error_state_) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "failed to load RPG database.");
    return;
  }

  // Marshal decode
  VALUE scripts = RGSSLoadData(exception_state, config.game.scripts.c_str());
  if (exception_state.HadException())
    return;
  if (!RB_TYPE_P(scripts, T_ARRAY)) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "invalid scripts file.");
    return;
  }

  // Zlib decode
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
    // Script sources are text: tag them UTF-8 when the decompressed bytes form
    // valid UTF-8, so Chinese/Japanese literals eval correctly instead of
    // triggering "invalid byte sequence" against UTF-8 inputs.
    script_str = TagUTF8IfValid(script_str);
    rb_ary_store(script, 2, script_str);
  }

  // RGSS behavior?
  rb_gv_set("$RGSS_SCRIPTS", scripts);

  // Execute
  for (long i = 0; i < RARRAY_LEN(scripts); ++i) {
    VALUE script = rb_ary_entry(scripts, i);
    if (!RB_TYPE_P(script, T_ARRAY) || RARRAY_LEN(script) < 3)
      continue;

    VALUE filename = rb_ary_entry(script, 1);
    VALUE source = rb_ary_entry(script, 2);
    EvalString(source, filename, &error_state_);
    if (error_state_)
      break;
  }

  if (error_state_) {
    VALUE exc = rb_errinfo();
    if (rb_obj_class(exc) != rb_eSystemExit)
      exception_state.Throw(urge::ExceptionState::kRGSSError,
                            RubyExceptionMessage(exc));
  }
}

BindingMain::~BindingMain() {
  ruby_cleanup(error_state_);
}

}  // namespace binding

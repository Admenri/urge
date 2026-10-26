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
#include "bind/script_loop.h"
#include "core/system/config.h"
#include "core/system/filesystem.h"
#include "core/graphics/graphics.h"

#include "bind/rpg_rgss1.h"
#include "bind/rpg_rgss2.h"
#include "bind/rpg_rgss3.h"

#include "zlib.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_timer.h"

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

  /* `Kernel#eval` refuses to run when there is no Ruby-level frame beneath it
     -- "Can't eval on top of Fiber or Thread" -- and the loop's fiber entry is
     a C function, so calling `eval` directly from here would fail on the
     fiber.  `__urge_eval_script` is one Ruby frame in between, defined at the
     top level so that its cref is `Object`: `class Game_Map` in a script still
     defines a top-level class, exactly as it did when the scripts were
     evaluated from `main`. */
  auto evaluate = [](VALUE parameter) -> VALUE {
    EvaluteContext* context = reinterpret_cast<EvaluteContext*>(parameter);
    VALUE argv[] = {context->string, context->filename};
    return rb_funcall2(Qnil, rb_intern("__urge_eval_script"), std::size(argv),
                       argv);
  };

  EvaluteContext context = {string, filename};
  return rb_protect(evaluate, reinterpret_cast<VALUE>(&context), state);
}

}  // namespace

namespace {

/* ==========================================================================
   The frame loop.

   `Graphics` no longer paces itself.  A frame is one frame of work, and it
   ends by calling the pacer installed on `Graphics`, which returns only when
   the next frame may begin.  The loop itself belongs to the host -- SDL's
   callback main loop, one `SDL_AppIterate` per tick -- and there is nothing
   platform-specific left in it:

     * a tick is one animation frame on the web, and one pass of a loop SDL
       paces at the game's own frame rate on the desktop.  The host provides the
       clock either way; the engine only says how much game time belongs in a
       tick;

     * a tick may run zero frames or several.  That is the only way to pace a
       40 fps game on a 60 Hz display -- one present per tick cannot express it
       -- and it is also how a frame that overran catches up instead of halving
       the frame rate;

   From the script's side it is one loop: `loop { Graphics.update }` runs to
   completion, and nothing in the engine knows which host is ticking it.
   ========================================================================== */

/* One engine per process, and a pacer installed as a plain function pointer
   has no `this` to reach a member through -- hence a file-static. */
struct LoopState {
  /* What the loop's stack should do the next time it is resumed.  The host
     drives it: start the game, or shut the interpreter down. */
  enum class Command { kNone, kRun, kCleanup };
  Command command = Command::kNone;

  /* Where the loop's stack is in its life. */
  bool initialized = false;
  bool finished = false;
  bool cleaned_up = false;

  /* Set on the loop's stack, where the exception object is still alive: the
     message is built there so that nothing VALUE-shaped has to outlive it. */
  bool failed = false;
  bool exited = false;
  std::string message;

  /* Pacer.  `smooth` is the smoothed ratio of one host tick to one game frame;
     `elapsed` carries the fraction until a whole frame is due. */
  double smooth = 1.0;
  double elapsed = 0.0;
  uint64_t last_tick = 0;
};

LoopState g_loop;

/* The exception state of the constructor, for the initialisation that runs on
   the loop's stack while the constructor is still on the host's. */
urge::ExceptionState* g_init_exception = nullptr;

/* How many engine frames are due now, when the last display tick was
   `delta_rate` frame periods long.

   165 Hz against a 60 fps game is the case this exists for: the tick is
   0.36 of a frame, so no single tick is ever worth a frame, and only carrying
   the remainder across ticks gets the average to 60.  Rounding instead would
   give 0 forever and stall the game. */
int32_t FramesDue(double delta_rate) {
  /* Smoothed, so one late tick does not make the loop run a burst and one
     early tick does not make it stall. */
  g_loop.smooth = g_loop.smooth * 0.8 + std::min(delta_rate, 2.0) * 0.2;

  if (g_loop.smooth >= 0.9) {
    /* The display is at or below the game's rate: one frame per tick, and the
       fraction left over is dropped rather than carried -- there is nowhere to
       put a second frame. */
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

/* The frame pacer: hand the thread back to the host's loop, and return when it
   ticks again.

   This is the whole contract between the game and the host.  A frame is one
   frame of work and it ends here; what the script does between two calls to
   `Graphics.update` -- loop, recurse, spawn Fibers of its own -- is the
   script's business and the engine takes no interest in it.  All that matters
   is that the frame got here, because getting here is what returns the thread
   to `SDL_AppIterate`: SDL presents the frame after that call returns on the
   web, dispatches its events between ticks on both, and decides whether to keep
   looping.  A script that never reaches a `Present` simply stops the loop being
   ticked, which is the same thing as saying it stopped running the game.

   The switch is a plain context switch on the coroutine backend, not a Ruby
   Fiber, so it lands on the host no matter what the script has done with the
   interpreter's own fibers.  See bind/script_loop.h. */
void PresentPacer() {
  script_loop::YieldToHost();
}

/* The scripts, evaluated on the loop's stack.  A script error is caught here
   rather than left to propagate: it would otherwise longjmp out of the loop
   with the exception in hand, through C++ frames on the host's stack that are
   not prepared for it. */
VALUE EvaluateScripts(VALUE scripts) {
  for (long i = 0; i < RARRAY_LEN(scripts); ++i) {
    VALUE script = rb_ary_entry(scripts, i);
    if (!RB_TYPE_P(script, T_ARRAY) || RARRAY_LEN(script) < 3)
      continue;

    int state = 0;
    EvalString(rb_ary_entry(script, 2), rb_ary_entry(script, 1), &state);
    if (state) {
      VALUE exception = rb_errinfo();
      g_loop.failed = true;
      g_loop.exited = rb_obj_class(exception) == rb_eSystemExit;
      g_loop.message = RubyExceptionMessage(exception);
      rb_set_errinfo(Qnil);
      break;
    }
  }

  return Qnil;
}

/* Runs the script list and records how it ended.  `rb_protect` is what keeps a
   script error from longjmp-ing out of the loop. */
void RunScripts() {
  VALUE scripts = rb_gv_get("$RGSS_SCRIPTS");
  if (!RB_TYPE_P(scripts, T_ARRAY)) {
    g_loop.failed = true;
    g_loop.message = "invalid scripts file.";
    return;
  }

  int state = 0;
  rb_protect(EvaluateScripts, scripts, &state);
  if (state && !g_loop.failed) {
    VALUE exception = rb_errinfo();
    g_loop.failed = true;
    g_loop.exited = rb_obj_class(exception) == rb_eSystemExit;
    g_loop.message = RubyExceptionMessage(exception);
    rb_set_errinfo(Qnil);
  }
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

/* Brings the interpreter up on the loop's stack: the VM, the bindings, the RPG
   database, and the game's scripts, in that order.  Nothing is evaluated here
   -- see RunOnLoopStack. */
void BindingMain::InitializeInterpreter(urge::ExceptionState& exception_state) {
  auto& config = urge::Config::Get();

  int argc = 0;
  char** argv = 0;
  ruby_sysinit(&argc, &argv);

#if defined(OS_EMSCRIPTEN)
  /* Scripts still get the interpreter's Fibers, and on the web a fiber switch
     *is* an Asyncify unwind: the fiber's C stack is spilled into the other half
     of the same allocation (coroutine/emscripten/Context.h splits it down the
     middle).  The default is 256 KiB, which leaves 128 KiB of each -- thin for
     a script that runs a frame from inside a Fiber of its own.

     4 MiB gives 2 MiB of each, and the pool it feeds is 8 stacks on wasm
     (`COROUTINE_LIMITED_ADDRESS_SPACE`), so the cost is bounded at ~33 MiB.
     The desktop keeps the interpreter's default: there is no spill there, and
     the pool is 32 stacks.  The engine's own loop stack is separate and has
     nothing to do with this; see bind/script_loop.h. */
  setenv("RUBY_FIBER_MACHINE_STACK_SIZE", "4194304", 1);
#endif

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
     own stacks and globals.  See Graphics::SetFrameCallback.

     That callback now runs on the loop's fiber rather than on the module's own
     stack, which is fine and needs no patch to the interpreter:
     `emscripten_scan_stack` scans from the current stack pointer up to
     `emscripten_stack_get_base()`, and `emscripten_fiber_swap` calls
     `emscripten_stack_set_limits` on every switch -- so the range it reports is
     the fiber's own, not the heap between the fiber and the top of the module
     stack. */
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

  /* The one Ruby-level frame `Kernel#eval` needs under it, for scripts that are
     evaluated on the loop's fiber rather than from `main`; see EvalString.  It
     is defined at the top level on purpose -- that is what makes its cref
     `Object`, so the scripts keep defining top-level classes. */
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

  /* The scripts are not evaluated here.  They are the game, and the game is
     what `LoopEntry` runs after the host starts ticking.  The array stays alive
     through `$RGSS_SCRIPTS`, which the collector marks, so nothing
     VALUE-shaped has to be kept in a C++ member for the loop to find it. */
}

/* The body of the loop's stack.  The interpreter is created here, the game runs
   here, and the interpreter is shut down here -- all on the same stack, because
   the interpreter records the machine stack it was initialised on and every
   stack-overflow check and collector scan is measured against it.

   Control alternates with the host: each `YieldToHost` returns the thread to
   whoever called `IterateMainLoop`, and the next call resumes right here. */
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

  /* The host is done with the game.  Tearing the interpreter down has to happen
     on this stack too, for the same reason it was built here. */
  if (g_loop.command == LoopState::Command::kCleanup) {
    g_loop.command = LoopState::Command::kNone;
    ruby_cleanup(error_state_);
    g_loop.cleaned_up = true;
    script_loop::YieldToHost();
  }
}

/* Entry point of the loop's stack; see script_loop.h. */
void BindingMain::LoopEntry(void* arg) {
  static_cast<BindingMain*>(arg)->RunOnLoopStack();
}

BindingMain::BindingMain(urge::ExceptionState& exception_state) {
  if (!script_loop::Start(&BindingMain::LoopEntry, this)) {
    exception_state.Throw(urge::ExceptionState::kRGSSError,
                          "failed to allocate the script stack.");
    return;
  }

  /* The initialisation runs on the loop's stack and comes back here when it is
     done -- or when it has failed, which it reports through the state the
     constructor was given. */
  g_init_exception = &exception_state;
  script_loop::Resume();
  g_init_exception = nullptr;

  if (!exception_state.HadException()) {
    /* The first tick is what starts the game, and the pacer is what every frame
       calls, so both are in place before the host gets control. */
    g_loop.command = LoopState::Command::kRun;
    g_loop.last_tick = SDL_GetPerformanceCounter();
    urge::Graphics::Get().SetPresentCallback(&PresentPacer);
  }
}

BindingMain::~BindingMain() {
  /* Nothing may call into the loop once the interpreter is gone: a stray
     `Graphics.update` during shutdown would find a pacer pointing at a stack
     that no longer exists. */
  if (auto* graphics = urge::Graphics::TryGet())
    graphics->SetPresentCallback(nullptr);

  if (!script_loop::Started())
    return;

  /* Shut the interpreter down where it was built.  A game still in flight is
     not resumed -- that would run it rather than end it -- so it is left to the
     process, which is about to go away anyway. */
  if (!g_loop.initialized || g_loop.finished) {
    g_loop.command = LoopState::Command::kCleanup;
    while (!g_loop.cleaned_up)
      script_loop::Resume();
  }

  script_loop::Finish();
}

namespace {

/* How the loop ended, once it has.  `exit` and a closed window are not
   failures: the first is the script saying it is done, the second is the
   player. */
BindingMain::Tick OutcomeOfFinishedLoop() {
  if (g_loop.failed && !g_loop.exited)
    return BindingMain::Tick::kFailed;
  return BindingMain::Tick::kFinished;
}

}  // namespace

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
    /* A tick that was paused -- the tab was hidden, the window was dragged,
       the machine was suspended -- is not a tick that fell behind.  Catching up
       on it would run a burst of updates the game never asked for. */
    g_loop.smooth = 1.0;
    g_loop.elapsed = 0.0;
  }

  int32_t repeat = FramesDue(delta / period);

  /* `Graphics.frame_skip = false` is a script saying it would rather slow
     down than jump: at most one frame per tick, however far behind. */
  if (!urge::Graphics::Get().frame_skip() && repeat > 1)
    repeat = 1;

  for (int32_t i = 0; i < repeat && !g_loop.finished; ++i)
    script_loop::Resume();

  /* The last resume may have been the one that ran the script to its end,
     which is the quit signal. */
  if (g_loop.finished)
    return OutcomeOfFinishedLoop();

  return Tick::kContinue;
}

const std::string& BindingMain::ErrorMessage() const {
  return g_loop.message;
}

}  // namespace binding

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
#include "core/config.h"
#include "core/filesystem.h"
#include "core/graphics.h"

#include "bind/rpg_rgss1.h"
#include "bind/rpg_rgss2.h"
#include "bind/rpg_rgss3.h"

#include "zlib.h"

namespace binding {

using urge::Exception;

namespace {

static VALUE RGSSLoadData(const char* filename) {
  auto stream = urge::IOService::Get().OpenReadRaw(filename);
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

  auto string_utf8 = [](VALUE arg) -> VALUE {
    if (RB_TYPE_P(arg, RUBY_T_STRING))
      rb_enc_associate(arg, rb_utf8_encoding());
    return arg;
  };

  auto utf8_conv = [](VALUE arg, VALUE proc) -> VALUE {
    if (RB_TYPE_P(arg, RUBY_T_STRING))
      rb_enc_associate(arg, rb_utf8_encoding());
    arg = rb_funcall(proc, rb_intern("call"), 1, arg);
    return arg;
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
  for (;;)
    urge::Graphics::Get().Update();
  return Qnil;
}

RB_FUNC(load_data) {
  const char* filename;
  ParseArgs(argc, argv, "z", &filename);
  return RGSSLoadData(filename);
}

RB_FUNC(save_data) {
  VALUE data;
  const char* filename;
  ParseArgs(argc, argv, "oz", &data, &filename);
  auto stream = urge::IOService::Get().OpenWrite(filename);
  VALUE dumped = rb_marshal_dump(data, Qnil);
  SDL_SaveFile_IO(stream, RSTRING_PTR(dumped), RSTRING_LEN(dumped), true);
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

BindingMain::BindingMain() {
  auto& config = urge::Config::Get();

  int argc = 0;
  char** argv = 0;
  ruby_sysinit(&argc, &argv);

  RUBY_INIT_STACK;
  ruby_init();

  rb_enc_set_default_internal(rb_enc_from_encoding(rb_utf8_encoding()));
  rb_enc_set_default_external(rb_enc_from_encoding(rb_utf8_encoding()));

  VALUE marshal_klass = rb_const_get(rb_cObject, rb_intern("Marshal"));
  rb_define_alias(rb_singleton_class(marshal_klass), "_load_utf8_", "load");
  DefineModuleFunction(marshal_klass, "load", marshal_load_utf8);

  DefineModuleFunction(rb_mKernel, "rgss_main", rgss_main);
  DefineModuleFunction(rb_mKernel, "rgss_stop", rgss_stop);
  DefineModuleFunction(rb_mKernel, "load_data", load_data);
  DefineModuleFunction(rb_mKernel, "save_data", save_data);

  InitBindings();

  rb_const_set(rb_mKernel, rb_intern("RGSS_VERSION"),
               LONG2NUM(config.rgss_version));

  // RPG database
  const char* rpg_source = config.xp()   ? rpg_rgss1
                           : config.vx() ? rpg_rgss2
                                         : rpg_rgss3;
  rb_eval_string_protect(rpg_source, &error_state_);
  if (error_state_)
    throw Exception(Exception::kRGSSError, "failed to load RPG database.");

  // Marshal decode
  VALUE scripts = RGSSLoadData(config.scripts.c_str());
  if (!RB_TYPE_P(scripts, T_ARRAY))
    throw Exception(Exception::kRGSSError, "invalid scripts file.");

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
    if (result != Z_OK)
      throw Exception(Exception::kRGSSError, "failed to decompress script");

    VALUE script_str =
        rb_str_new(reinterpret_cast<const char*>(buffer.data()), output_size);
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
      throw Exception(Exception::kRGSSError, RubyExceptionMessage(exc));
  }
}

BindingMain::~BindingMain() {
  ruby_cleanup(error_state_);
}

}  // namespace binding

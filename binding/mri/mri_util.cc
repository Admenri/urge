// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "binding/mri/mri_util.h"

namespace binding {

static VALUE g_exception_list[content::ExceptionCode::CODE_NUMS + 1];
static GlobalModules g_global_modules;
extern content::ExecutionContext* g_current_execution_context;

GlobalModules* MriGetGlobalModules() {
  return &g_global_modules;
}

content::ExecutionContext* MriGetCurrentContext() {
  return g_current_execution_context;
}

int MriParseArgsTo(int argc, VALUE* argv, const char* fmt, ...) {
  va_list args_iter;
  bool is_arg_optional = false;
  int count = 0;
  std::string format(fmt);
  auto ch = format.begin();

  va_start(args_iter, fmt);

  while (ch != format.end()) {
    if (*ch != '|' && argc <= count) {
      if (!is_arg_optional)
        rb_raise(rb_eArgError, "wrong number of arguments");
      break;
    }

    /*
     * o -> VALUE
     * i -> int32
     * u -> uint32
     * l -> int64
     * p -> uint64
     * s -> std::string
     * f -> double
     * b -> bool
     * n -> std::string
     * r -> void*
     */

    VALUE arg_element = argv[count];
    switch (*ch) {
      case 'o': {
        VALUE* ptr = va_arg(args_iter, VALUE*);
        *ptr = arg_element;
      }
        ++count;
        break;
      case 'i': {
        int32_t* ptr = va_arg(args_iter, int32_t*);
        switch (rb_type(arg_element)) {
          case RUBY_T_FLOAT:
          case RUBY_T_BIGNUM:
          case RUBY_T_FIXNUM:
            *ptr = NUM2INT(arg_element);
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected int32 fixnum",
                     count);
        }
      }
        ++count;
        break;
      case 'u': {
        uint32_t* ptr = va_arg(args_iter, uint32_t*);
        switch (rb_type(arg_element)) {
          case RUBY_T_FLOAT:
          case RUBY_T_BIGNUM:
          case RUBY_T_FIXNUM:
            *ptr = NUM2UINT(arg_element);
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected uint32 fixnum",
                     count);
        }
      }
        ++count;
        break;
      case 'l': {
        int64_t* ptr = va_arg(args_iter, int64_t*);
        switch (rb_type(arg_element)) {
          case RUBY_T_FLOAT:
          case RUBY_T_BIGNUM:
          case RUBY_T_FIXNUM:
            *ptr = NUM2LL(arg_element);
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected int64 fixnum",
                     count);
        }
      }
        ++count;
        break;
      case 'p': {
        uint64_t* ptr = va_arg(args_iter, uint64_t*);
        switch (rb_type(arg_element)) {
          case RUBY_T_FLOAT:
          case RUBY_T_BIGNUM:
          case RUBY_T_FIXNUM:
            *ptr = NUM2ULL(arg_element);
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected uint64 fixnum",
                     count);
        }
      }
        ++count;
        break;
      case 's': {
        VALUE str = rb_obj_as_string(arg_element);

        std::string* ptr = va_arg(args_iter, std::string*);
        *ptr = std::string(RSTRING_PTR(str), RSTRING_LEN(str));
      }
        ++count;
        break;
      case 'f': {
        double* ptr = va_arg(args_iter, double*);

        switch (rb_type(arg_element)) {
          case RUBY_T_FLOAT:
            *ptr = RFLOAT_VALUE(arg_element);
            break;
          case RUBY_T_FIXNUM:
            *ptr = FIX2INT(arg_element);
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected float", count);
        }
      }
        ++count;
        break;
      case 'b': {
        bool* ptr = va_arg(args_iter, bool*);
        switch (rb_type(arg_element)) {
          case RUBY_T_TRUE:
            *ptr = true;
            break;
          case RUBY_T_FALSE:
          case RUBY_T_NIL:
            *ptr = false;
            break;
          case RUBY_T_FIXNUM:
            *ptr = FIX2INT(arg_element);
            break;
          default:
            rb_warning("Warning: Argument %d: Expected bool", count);
            *ptr = true;
            break;
        }
      }
        ++count;
        break;
      case 'n': {
        std::string* ptr = va_arg(args_iter, std::string*);
        switch (rb_type(arg_element)) {
          case RUBY_T_SYMBOL:
            *ptr = std::string(rb_id2name(SYM2ID(arg_element)));
            break;
          case RUBY_T_STRING:
            *ptr = std::string(RSTRING_PTR(arg_element),
                                RSTRING_LEN(arg_element));
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected symbol", count);
        }
      }
        ++count;
        break;
      case 'r': {
        void** ptr = va_arg(args_iter, void**);
        switch (rb_type(arg_element)) {
          case RUBY_T_STRING:
            *ptr = RSTRING_PTR(arg_element);
            break;
          default:
            rb_raise(rb_eTypeError, "Argument %d: Expected raw string buffer",
                     count);
        }
      }
        ++count;
        break;
      case '|':
        is_arg_optional = true;
        break;
      default:
        rb_raise(rb_eFatal, "Invalid argument specifier %c", *ch);
    }

    ch++;
  }

  va_end(args_iter);

  /* Real args caller provide */
  return count;
}

void MriInitException(bool rgss3) {
  g_exception_list[content::ExceptionCode::NO_EXCEPTION] = Qnil;
  g_exception_list[content::ExceptionCode::CONTENT_ERROR] =
      rb_define_class("RGSSError", rb_eStandardError);
  g_exception_list[content::ExceptionCode::GPU_ERROR] =
      rb_define_class("GPUError", rb_eStandardError);
  g_exception_list[content::ExceptionCode::IO_ERROR] = rb_const_get(
      rb_const_get(rb_cObject, rb_intern("Errno")), rb_intern("ENOENT"));

  g_exception_list[content::ExceptionCode::CODE_NUMS] =
      rb_define_class(rgss3 ? "RGSSReset" : "Reset", rb_eException);
}

void MriProcessException(const content::ExceptionState& exception) {
  if (exception.HadException()) {
    std::string error_message;
    content::ExceptionCode error_code = exception.FetchException(error_message);
    VALUE rb_eCustom = g_exception_list[error_code];
    rb_raise(rb_eCustom, "%s", error_message.c_str());
  }
}

void MriCheckArgc(int actual, int expected) {
  if (actual != expected)
    rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", actual,
             expected);
}

VALUE MriGetException(content::ExceptionCode exception) {
  return g_exception_list[exception];
}

}  // namespace binding

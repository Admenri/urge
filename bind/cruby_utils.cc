#include "cruby_utils.h"

namespace binding {

VALUE g_reset_exception = Qnil;
VALUE g_rgss_exception = Qnil;

void ArgcError(int argc) {
  rb_raise(rb_eArgError, "wrong number of arguments (given %d)", argc);
}

bool IsValidUTF8(VALUE str) {
  const auto* p = reinterpret_cast<const unsigned char*>(RSTRING_PTR(str));
  const long len = RSTRING_LEN(str);
  long i = 0;
  while (i < len) {
    const unsigned char c = p[i];
    long n;
    if (c < 0x80) {
      n = 1;
    } else if ((c & 0xE0) == 0xC0) {
      n = 2;
    } else if ((c & 0xF0) == 0xE0) {
      n = 3;
    } else if ((c & 0xF8) == 0xF0) {
      n = 4;
    } else {
      return false;
    }
    if (i + n > len)
      return false;
    for (long j = 1; j < n; ++j)
      if ((p[i + j] & 0xC0) != 0x80)
        return false;
    if (n == 2 && p[i] < 0xC2)
      return false;
    if (n == 3 && p[i] == 0xE0 && p[i + 1] < 0xA0)
      return false;
    if (n == 3 && p[i] == 0xED && p[i + 1] >= 0xA0)
      return false;
    if (n == 4 && p[i] == 0xF0 && p[i + 1] < 0x90)
      return false;
    if (n == 4 && p[i] > 0xF4)
      return false;
    if (n == 4 && p[i] == 0xF4 && p[i + 1] >= 0x90)
      return false;
    i += n;
  }
  return true;
}

VALUE TagUTF8IfValid(VALUE str) {
  if (RB_TYPE_P(str, T_STRING) && !OBJ_FROZEN(str) &&
      rb_enc_get_index(str) != rb_utf8_encindex() && IsValidUTF8(str))
    rb_enc_associate_index(str, rb_utf8_encindex());
  return str;
}

int ParseArgs(int argc, VALUE* argv, const char* fmt, ...) {
  va_list args_iter;
  bool is_arg_optional = false;
  int count = 0;
  const char* ch = fmt;

  va_start(args_iter, fmt);

  while (*ch) {
    if (*ch != '|' && argc <= count) {
      if (!is_arg_optional)
        rb_raise(rb_eArgError, "wrong number of arguments");
      break;
    }

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
      case 'z': {
        const char** ptr = va_arg(args_iter, const char**);
        *ptr = StringValueCStr(arg_element);
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
            *ptr =
                std::string(RSTRING_PTR(arg_element), RSTRING_LEN(arg_element));
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

  return count;
}

}

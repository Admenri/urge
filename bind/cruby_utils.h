#pragma once

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "ruby.h"
#include "ruby/encoding.h"
#include "ruby/intern.h"
#include "ruby/version.h"

#ifdef RUBY_API_VERSION_MAJOR
#define RAPI_MAJOR RUBY_API_VERSION_MAJOR
#define RAPI_MINOR RUBY_API_VERSION_MINOR
#define RAPI_TEENY RUBY_API_VERSION_TEENY
#else
#define RAPI_MAJOR RUBY_VERSION_MAJOR
#define RAPI_MINOR RUBY_VERSION_MINOR
#define RAPI_TEENY RUBY_VERSION_TEENY
#endif
#define RAPI_FULL ((RAPI_MAJOR * 100) + (RAPI_MINOR * 10) + RAPI_TEENY)

#if RAPI_FULL >= 270
#include "ruby/internal/arithmetic/int.h"
#include "ruby/internal/arithmetic/long_long.h"
#endif

// <windows.h> reaches every glue TU from inside ruby.h -- on Win32
// third_party/cruby/thread_win32.h includes it -- and it #defines a few short
// names as macros.  A function-like macro silently rewrites the engine's own
// declarations, because core/*.h are parsed *after* this header:
//
//   WinUser.h  #define DrawText DrawTextA
//     => core/bitmap.h's `void Bitmap::DrawText(...)` became
//        `void Bitmap::DrawTextA(...)`, a member the engine never defines, and
//        every glue TU linked against `urge::Bitmap::DrawTextA` (LNK2019).
//
//   wingdi.h   #define ERROR 0
//     => today the only `ERROR` in core/ is inside a string literal
//        ("[URGE] [ERROR] " in core/logger.h), which a macro cannot touch, but
//        any future enumerator spelled ERROR would be rewritten without a word.
//
// Verified by compiling a probe that includes this header and core/bitmap.h:
// DrawText -> DrawTextA and ERROR -> 0 are both live, `max`/`min` are not (the
// build already neutralises minwindef.h).  Dropping the two here, after every
// ruby header and before the first core/ header, is what keeps the engine's
// spelling; nothing between the two points depends on either macro.
#undef DrawText
#undef ERROR

#include "core/exception.h"
#include "core/object.h"
#include "core/refptr.h"

namespace binding {

struct BindingDataType {
  const char* struct_name;
  void* reserved;
};

#if RAPI_FULL >= 270
#define DEF_TYPE_RESERVED 0,
#else
#define DEF_TYPE_RESERVED
#endif

#if RAPI_FULL >= 210
#define DEF_TYPE_FLAGS 0
#else
#define DEF_TYPE_FLAGS
#endif

#define RB_DATATYPE(Klass, Name, Free)               \
  const rb_data_type_t k##Klass##DataType = {        \
      Name,                                          \
      {nullptr, Free, nullptr, DEF_TYPE_RESERVED{}}, \
      nullptr,                                       \
      nullptr,                                       \
      DEF_TYPE_FLAGS}

#define RB_DECL_TYPE(Klass) extern const rb_data_type_t k##Klass##DataType;
#define RB_DEF_TYPE(Klass) \
  RB_DATATYPE(Klass, #Klass, ReleaseDataType<urge::Klass>)

#define RB_FUNC(name) static VALUE name(int argc, VALUE* argv, VALUE self)

template <typename Ty>
inline void ReleaseDataType(void* ptr) {
  if (ptr)
    static_cast<Ty*>(ptr)->Release();
}

// RGSS exception classes. The binding library owns them so that it is
// self-sufficient: InitBindings() defines them when they are still nil, and a
// host entry point which wants a specific parent class can define them first.
extern VALUE g_reset_exception;
extern VALUE g_rgss_exception;

// Helper
using RubyMethod = VALUE (*)(int argc, VALUE* argv, VALUE self);
inline void DefineMethod(VALUE klass, const char* name, RubyMethod func) {
  rb_define_method(klass, name, RUBY_METHOD_FUNC(func), -1);
}
inline void DefineClassMethod(VALUE klass, const char* name, RubyMethod func) {
  rb_define_singleton_method(klass, name, RUBY_METHOD_FUNC(func), -1);
}
inline void DefineModuleFunction(VALUE mod, const char* name, RubyMethod func) {
  rb_define_module_function(mod, name, RUBY_METHOD_FUNC(func), -1);
}

// ARGS parsing
int ParseArgs(int argc, VALUE* argv, const char* fmt, ...);

inline void CheckArgc(int ac, int ex) {
  if (ac != ex)
    rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", ac, ex);
}

template <const rb_data_type_t* DataType>
inline VALUE ClassAllocate(VALUE klass) {
#if RAPI_FULL >= 230
  return rb_data_typed_object_wrap(klass, nullptr, DataType);
#else
  return rb_data_typed_object_alloc(klass, nullptr, DataType);
#endif
}

template <typename Ty>
inline VALUE SetupSelfData(VALUE self, Ty* data) {
  RTYPEDDATA_DATA(self) = data;
  if (data)
    data->AddRef();
  return self;
}

template <typename Ty>
inline VALUE SetupSelfData(VALUE self, Ty* data, const rb_data_type_t&) {
  return SetupSelfData(self, data);
}

template <typename Ty>
inline VALUE SetupSelfData(VALUE self, Ty* data, const BindingDataType&) {
  return SetupSelfData(self, data);
}

inline std::string RubyStringValue(VALUE value) {
  VALUE string = rb_obj_as_string(value);
  return std::string(RSTRING_PTR(string), RSTRING_LEN(string));
}

template <typename Ty>
inline Ty* GetSelfData(VALUE self) {
  auto* ptr = static_cast<Ty*>(RTYPEDDATA_DATA(self));
  if (!ptr)
    rb_raise(rb_eRuntimeError, "invalid instance data: missing call to super?");
  return ptr;
}

template <typename Ty>
inline urge::RefPtr<Ty> GetObject(VALUE value, const rb_data_type_t& type) {
  if (NIL_P(value))
    return nullptr;
  auto* ptr = static_cast<Ty*>(rb_check_typeddata(value, &type));
  return urge::RefPtr<Ty>(ptr);
}

template <typename Ty>
inline VALUE WrapObject(Ty* ptr, const rb_data_type_t& type) {
  if (!ptr)
    return Qnil;
  // NOTE: `wrap_struct_name` is the C++ class name (RB_DEF_TYPE passes #Klass),
  // so the Ruby class must be registered under that very name at the top level.
  VALUE klass = rb_const_get(rb_cObject, rb_intern(type.wrap_struct_name));
  VALUE object = rb_obj_alloc(klass);
  return SetupSelfData(object, ptr);
}

inline void ProcessException(const urge::Exception& exception) {
  VALUE klass = rb_eStandardError;
  switch (exception.type()) {
    case urge::Exception::kExitError:
      klass = rb_eSystemExit;
      break;
    case urge::Exception::kResetError:
      klass = g_reset_exception;
      break;
    case urge::Exception::kRGSSError:
      // GPU failures are engine side, but a game must still be able to catch
      // them with the same rescue clauses it uses for RGSS errors.
    case urge::Exception::kGPUError:
      klass = g_rgss_exception;
      break;
    case urge::Exception::kIOError:
      klass = rb_const_get(rb_const_get(rb_cObject, rb_intern("Errno")),
                           rb_intern("ENOENT"));
      break;
    default:
      break;
  }
  rb_raise(klass, "%s", exception.message().c_str());
}

// Exception process
#define EXC_BEGIN try
#define EXC_END                              \
  catch (const urge::Exception& exception) { \
    ProcessException(exception);             \
  }

// Utils
inline std::vector<std::string> GetStringVector(VALUE value) {
  std::vector<std::string> result;
  if (NIL_P(value))
    return result;
  if (RB_TYPE_P(value, T_STRING)) {
    result.emplace_back(StringValueCStr(value));
    return result;
  }
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array or String");
  for (long i = 0; i < RARRAY_LEN(value); ++i) {
    VALUE item = rb_ary_entry(value, i);
    if (!RB_TYPE_P(item, T_STRING))
      rb_raise(rb_eTypeError, "%s", "expected String elements");
    result.emplace_back(StringValueCStr(item));
  }
  return result;
}

inline VALUE WrapStringVector(const std::vector<std::string>& values) {
  VALUE array = rb_ary_new2(static_cast<long>(values.size()));
  for (const auto& value : values)
    rb_ary_push(array,
                rb_enc_str_new(value.c_str(), static_cast<long>(value.size()),
                               rb_utf8_encoding()));
  return array;
}

// Returns true if every byte of `str` forms a valid UTF-8 sequence. Used to
// decide whether an untagged (ASCII-8BIT) string is really text that should be
// re-tagged as UTF-8, as opposed to binary data that must be left alone.
bool IsValidUTF8(VALUE str);

// Recursively force every String in an object graph to UTF-8. Strings whose
// bytes are not valid UTF-8 (binary data) and frozen strings are left as-is.
// Used on Marshal-loaded game data so script-side text is always UTF-8.
void ForceEncodingUTF8(VALUE obj);

// Tag `str` as UTF-8 if its bytes form valid UTF-8, otherwise leave its
// current encoding untouched. Returns the (possibly re-tagged) string. Use on
// byte buffers that are expected to hold text (e.g. decompressed script
// sources) without blindly corrupting genuinely binary data.
VALUE TagUTF8IfValid(VALUE str);

// ---------------------------------------------------------------------------
// Attribute helpers
//
// Every macro expands to the getter `<Class>_<CppName>` and the setter
// `<Class>_<CppName>Equal`; the generator registers them under the Ruby names
// recorded in the IR. All of them read and write through `Attr_<CppName>()`,
// which takes and returns `std::optional` for the field itself.
// ---------------------------------------------------------------------------

// Instance attribute, `std::optional<ty>`
#define BINDING_ATTR_INT(ty, selfty, cap)       \
  RB_FUNC(ty##_##cap) {                         \
    auto* self_obj = GetSelfData<selfty>(self); \
    EXC_BEGIN {                                 \
      return INT2NUM(*self_obj->Attr_##cap());  \
    }                                           \
    EXC_END                                     \
    return Qnil;                                \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    auto* self_obj = GetSelfData<selfty>(self); \
    int value;                                  \
    ParseArgs(argc, argv, "i", &value);         \
    EXC_BEGIN {                                 \
      self_obj->Attr_##cap(value);              \
    }                                           \
    EXC_END;                                    \
    return Qnil;                                \
  }

#define BINDING_ATTR_FLOAT(ty, selfty, cap)            \
  RB_FUNC(ty##_##cap) {                                \
    auto* self_obj = GetSelfData<selfty>(self);        \
    EXC_BEGIN {                                        \
      return rb_float_new(*self_obj->Attr_##cap());    \
    }                                                  \
    EXC_END;                                           \
    return Qnil;                                       \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    auto* self_obj = GetSelfData<selfty>(self);        \
    double value;                                      \
    ParseArgs(argc, argv, "f", &value);                \
    EXC_BEGIN {                                        \
      self_obj->Attr_##cap(static_cast<float>(value)); \
    }                                                  \
    EXC_END;                                           \
    return Qnil;                                       \
  }

#define BINDING_ATTR_BOOL(ty, selfty, cap)             \
  RB_FUNC(ty##_##cap) {                                \
    auto* self_obj = GetSelfData<selfty>(self);        \
    EXC_BEGIN {                                        \
      return *self_obj->Attr_##cap() ? Qtrue : Qfalse; \
    }                                                  \
    EXC_END;                                           \
    return Qnil;                                       \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    auto* self_obj = GetSelfData<selfty>(self);        \
    bool value;                                        \
    ParseArgs(argc, argv, "b", &value);                \
    EXC_BEGIN {                                        \
      self_obj->Attr_##cap(value != 0);                \
    }                                                  \
    EXC_END;                                           \
    return Qnil;                                       \
  }

#define BINDING_ATTR_STRINGVECTOR(ty, selfty, cap)          \
  RB_FUNC(ty##_##cap) {                                     \
    auto* self_obj = GetSelfData<selfty>(self);             \
    EXC_BEGIN {                                             \
      return WrapStringVector(*self_obj->Attr_##cap());      \
    }                                                       \
    EXC_END;                                                \
    return Qnil;                                            \
  }                                                         \
  RB_FUNC(ty##_##cap##Equal) {                              \
    auto* self_obj = GetSelfData<selfty>(self);             \
    VALUE value;                                            \
    ParseArgs(argc, argv, "o", &value);                     \
    std::vector<std::string> strings = GetStringVector(value); \
    EXC_BEGIN {                                             \
      self_obj->Attr_##cap(strings);                         \
    }                                                       \
    EXC_END;                                                \
    return Qnil;                                            \
  }

// Object attribute holding a value the class itself owns: the wrapper is a
// fresh Ruby object every read, because the C++ side may hand out a different
// pointer (a copy-on-write value class, for instance).
#define BINDING_ATTR_OBJECT(ty, selfty, cap, objty, dataty)            \
  RB_FUNC(ty##_##cap) {                                                \
    auto* self_obj = GetSelfData<selfty>(self);                        \
    EXC_BEGIN {                                                        \
      return WrapObject<objty>(self_obj->Attr_##cap()->get(), dataty); \
    }                                                                  \
    EXC_END;                                                           \
    return Qnil;                                                       \
  }                                                                    \
  RB_FUNC(ty##_##cap##Equal) {                                         \
    auto* self_obj = GetSelfData<selfty>(self);                        \
    VALUE value;                                                       \
    ParseArgs(argc, argv, "o", &value);                                \
    auto object = GetObject<objty>(value, dataty);                     \
    EXC_BEGIN {                                                        \
      self_obj->Attr_##cap(object);                                    \
    }                                                                  \
    EXC_END;                                                           \
    return Qnil;                                                       \
  }

// Object attribute holding a reference the engine owns (Bitmap, Viewport,
// Font...). The wrapper is cached in an instance variable so that repeated
// reads answer the very same Ruby object and `equal?` keeps working.
#define BINDING_ATTR_OBJECT_REF(ty, selfty, cap, objty, dataty)                \
  RB_FUNC(ty##_##cap) {                                                        \
    auto* self_obj = GetSelfData<selfty>(self);                                \
    EXC_BEGIN {                                                                \
      VALUE cached = rb_iv_get(self, "_" #cap);                                \
      if (!NIL_P(cached))                                                      \
        return cached;                                                         \
      VALUE object = WrapObject<objty>(self_obj->Attr_##cap()->get(), dataty); \
      rb_iv_set(self, "_" #cap, object);                                       \
      return object;                                                           \
    }                                                                          \
    EXC_END;                                                                   \
    return Qnil;                                                               \
  }                                                                            \
  RB_FUNC(ty##_##cap##Equal) {                                                 \
    auto* self_obj = GetSelfData<selfty>(self);                                \
    VALUE value;                                                               \
    ParseArgs(argc, argv, "o", &value);                                        \
    auto object = GetObject<objty>(value, dataty);                             \
    EXC_BEGIN {                                                                \
      self_obj->Attr_##cap(object);                                            \
      rb_iv_set(self, "_" #cap, value);                                        \
    }                                                                          \
    EXC_END;                                                                   \
    return Qnil;                                                               \
  }

// Class (static) attribute. Same shape as the instance macros above, but the
// accessor is a class method and there is no `self` to read the object from.
#define BINDING_CLASS_ATTR_INT(ty, selfty, cap) \
  RB_FUNC(ty##_##cap) {                         \
    EXC_BEGIN {                                 \
      return INT2NUM(*selfty::Attr_##cap());    \
    }                                           \
    EXC_END;                                    \
    return Qnil;                                \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    int value;                                  \
    ParseArgs(argc, argv, "i", &value);         \
    EXC_BEGIN {                                 \
      selfty::Attr_##cap(value);                \
    }                                           \
    EXC_END;                                    \
    return Qnil;                                \
  }

#define BINDING_CLASS_ATTR_FLOAT(ty, selfty, cap)   \
  RB_FUNC(ty##_##cap) {                             \
    EXC_BEGIN {                                     \
      return rb_float_new(*selfty::Attr_##cap());   \
    }                                               \
    EXC_END;                                        \
    return Qnil;                                    \
  }                                                 \
  RB_FUNC(ty##_##cap##Equal) {                      \
    double value;                                   \
    ParseArgs(argc, argv, "f", &value);             \
    EXC_BEGIN {                                     \
      selfty::Attr_##cap(static_cast<float>(value)); \
    }                                               \
    EXC_END;                                        \
    return Qnil;                                    \
  }

#define BINDING_CLASS_ATTR_BOOL(ty, selfty, cap)        \
  RB_FUNC(ty##_##cap) {                                 \
    EXC_BEGIN {                                         \
      return *selfty::Attr_##cap() ? Qtrue : Qfalse;    \
    }                                                   \
    EXC_END;                                            \
    return Qnil;                                        \
  }                                                     \
  RB_FUNC(ty##_##cap##Equal) {                          \
    bool value;                                         \
    ParseArgs(argc, argv, "b", &value);                 \
    EXC_BEGIN {                                         \
      selfty::Attr_##cap(value != 0);                   \
    }                                                   \
    EXC_END;                                            \
    return Qnil;                                        \
  }

#define BINDING_CLASS_ATTR_STRINGVECTOR(ty, selfty, cap)      \
  RB_FUNC(ty##_##cap) {                                       \
    EXC_BEGIN {                                               \
      return WrapStringVector(*selfty::Attr_##cap());          \
    }                                                         \
    EXC_END;                                                  \
    return Qnil;                                              \
  }                                                           \
  RB_FUNC(ty##_##cap##Equal) {                                \
    VALUE value;                                              \
    ParseArgs(argc, argv, "o", &value);                       \
    std::vector<std::string> strings = GetStringVector(value); \
    EXC_BEGIN {                                               \
      selfty::Attr_##cap(strings);                             \
    }                                                         \
    EXC_END;                                                  \
    return Qnil;                                              \
  }

#define BINDING_CLASS_ATTR_OBJECT(ty, selfty, cap, objty, dataty)     \
  RB_FUNC(ty##_##cap) {                                               \
    EXC_BEGIN {                                                       \
      auto result = selfty::Attr_##cap();                             \
      return WrapObject<objty>(result->get(), dataty);                 \
    }                                                                 \
    EXC_END;                                                          \
    return Qnil;                                                      \
  }                                                                   \
  RB_FUNC(ty##_##cap##Equal) {                                        \
    VALUE value;                                                      \
    ParseArgs(argc, argv, "o", &value);                               \
    auto object = GetObject<objty>(value, dataty);                    \
    EXC_BEGIN {                                                       \
      selfty::Attr_##cap(object);                                     \
    }                                                                 \
    EXC_END;                                                          \
    return Qnil;                                                      \
  }

}  // namespace binding

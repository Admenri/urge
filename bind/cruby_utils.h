#pragma once

#include <cctype>
#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "ruby.h"
#include "ruby/encoding.h"
#include "ruby/intern.h"
#include "ruby/internal/arithmetic/int.h"
#include "ruby/internal/arithmetic/long_long.h"

#undef DrawText
#undef ERROR

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/common/object.h"
#include "core/common/refptr.h"

namespace binding {

#define RB_DATATYPE_PARENT(Klass, Name, Free, Parent) \
  const rb_data_type_t k##Klass##DataType = {         \
      Name, {nullptr, Free, nullptr, 0, {}}, Parent, nullptr, 0}

#define RB_DATATYPE(Klass, Name, Free) \
  RB_DATATYPE_PARENT(Klass, Name, Free, nullptr)

#define RB_DECL_TYPE(Klass) extern const rb_data_type_t k##Klass##DataType;

#define RB_DEF_TYPE(Klass) \
  RB_DATATYPE(Klass, #Klass, ReleaseDataType<urge::Klass>)

#define RB_DEF_TYPE_INHERIT(Klass, Parent)                        \
  RB_DATATYPE_PARENT(Klass, #Klass, ReleaseDataType<urge::Klass>, \
                     &k##Parent##DataType)

#define RB_FUNC(name) static VALUE name(int argc, VALUE* argv, VALUE self)

template <typename Ty>
inline void ReleaseDataType(void* ptr) {
  if (ptr)
    static_cast<Ty*>(ptr)->Release();
}

extern VALUE g_reset_exception;
extern VALUE g_rgss_exception;

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

int ParseArgs(int argc, VALUE* argv, const char* fmt, ...);

inline void CheckArgc(int ac, int ex) {
  if (ac != ex)
    rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", ac, ex);
}

template <const rb_data_type_t* DataType>
inline VALUE ClassAllocate(VALUE klass) {
  return rb_data_typed_object_wrap(klass, nullptr, DataType);
}

template <typename Ty>
inline VALUE SetupSelfData(VALUE self, Ty* data) {
  RTYPEDDATA_DATA(self) = data;
  if (data)
    data->AddRef();
  return self;
}

template <typename Ty>
inline Ty* GetSelfData(VALUE self) {
  auto* ptr = static_cast<Ty*>(RTYPEDDATA_DATA(self));
  if (!ptr)
    rb_raise(rb_eRuntimeError, "invalid instance data: missing call to super?");
  return ptr;
}

template <typename Ty>
inline VALUE WrapObject(Ty* ptr, const rb_data_type_t& type) {
  if (!ptr)
    return Qnil;
  VALUE klass = rb_const_get(rb_cObject, rb_intern(type.wrap_struct_name));
  VALUE object = rb_obj_alloc(klass);
  return SetupSelfData(object, ptr);
}

inline void RaiseException(const urge::ExceptionState& exception_state) {
  if (!exception_state.HadException())
    return;
  VALUE klass = rb_eStandardError;
  switch (exception_state.type()) {
    case urge::ExceptionState::kExitError:
      klass = rb_eSystemExit;
      break;
    case urge::ExceptionState::kResetError:
      klass = g_reset_exception;
      break;
    case urge::ExceptionState::kRGSSError:
    case urge::ExceptionState::kGPUError:
      klass = g_rgss_exception;
      break;
    case urge::ExceptionState::kIOError:
      klass = rb_const_get(rb_const_get(rb_cObject, rb_intern("Errno")),
                           rb_intern("ENOENT"));
      break;
    default:
      break;
  }
  const char* message = exception_state.message();
  rb_raise(klass, "%s", message);
}

[[noreturn]] void ArgcError(int argc);

template <typename Ty, bool kNullable = false>
inline Ty* CheckObject(VALUE value, const rb_data_type_t& type) {
  if (!NIL_P(value))
    return static_cast<Ty*>(rb_check_typeddata(value, &type));
  if constexpr (!kNullable)
    rb_raise(rb_eTypeError, "wrong argument type nil (expected %s)",
             type.wrap_struct_name);
  return nullptr;
}

template <typename Ty>
inline Ty* CheckMaybeObject(VALUE value, const rb_data_type_t& type) {
  return CheckObject<Ty, true>(value, type);
}

inline void CheckString(VALUE value, const char** data, size_t* size) {
  VALUE string = rb_obj_as_string(value);
  *data = RSTRING_PTR(string);
  *size = static_cast<size_t>(RSTRING_LEN(string));
}

template <typename Ty>
struct VectorCodec;

template <typename Ty>
inline VALUE WrapVectorOf(const std::vector<Ty>& values) {
  VALUE array = rb_ary_new2(static_cast<long>(values.size()));
  for (const Ty& value : values)
    rb_ary_push(array, VectorCodec<Ty>::To(value));
  return array;
}

template <typename Ty>
inline void CheckNumericElement(VALUE value) {
  if (RB_FLOAT_TYPE_P(value)) {
    if constexpr (std::is_integral<Ty>::value) {
      const double number = RFLOAT_VALUE(value);
      if (number < static_cast<double>((std::numeric_limits<Ty>::min)()) ||
          number > static_cast<double>((std::numeric_limits<Ty>::max)()))
        rb_raise(rb_eRangeError, "vector element out of range");
    }
    return;
  }
  if (!RB_INTEGER_TYPE_P(value))
    rb_raise(rb_eTypeError, "%s", "expected numeric vector elements");
  if constexpr (std::is_integral<Ty>::value) {
    const long long number = NUM2LL(value);
    if (number < static_cast<long long>((std::numeric_limits<Ty>::min)()) ||
        number > static_cast<long long>((std::numeric_limits<Ty>::max)()))
      rb_raise(rb_eRangeError, "vector element out of range");
  }
}

#define DEFINE_VECTOR_CODEC(Ty, ToRuby)                                \
  template <>                                                          \
  struct VectorCodec<Ty> {                                             \
    static void Check(VALUE value) { CheckNumericElement<Ty>(value); } \
    static Ty FromUnchecked(VALUE value) {                             \
      if (RB_FLOAT_TYPE_P(value))                                      \
        return static_cast<Ty>(RFLOAT_VALUE(value));                   \
      return static_cast<Ty>(NUM2LL(value));                           \
    }                                                                  \
    static VALUE To(Ty value) { return (ToRuby); }                     \
  }

DEFINE_VECTOR_CODEC(int8_t, INT2NUM(value));
DEFINE_VECTOR_CODEC(int16_t, INT2NUM(value));
DEFINE_VECTOR_CODEC(int32_t, INT2NUM(value));
DEFINE_VECTOR_CODEC(uint8_t, UINT2NUM(value));
DEFINE_VECTOR_CODEC(uint16_t, UINT2NUM(value));
DEFINE_VECTOR_CODEC(uint32_t, UINT2NUM(value));
DEFINE_VECTOR_CODEC(int64_t, LL2NUM(value));
DEFINE_VECTOR_CODEC(uint64_t, ULL2NUM(value));
DEFINE_VECTOR_CODEC(float, rb_float_new(value));
DEFINE_VECTOR_CODEC(double, rb_float_new(value));
template <>
struct VectorCodec<std::string> {
  static void Check(VALUE value) {
    if (!RB_TYPE_P(value, T_STRING))
      rb_raise(rb_eTypeError, "%s", "expected String elements");
  }
  static std::string FromUnchecked(VALUE value) {
    return std::string(RSTRING_PTR(value),
                       static_cast<size_t>(RSTRING_LEN(value)));
  }
  static VALUE To(const std::string& value) {
    return rb_enc_str_new(value.c_str(), static_cast<long>(value.size()),
                          rb_utf8_encoding());
  }
};

template <typename Ty>
inline void CheckVectorOf(VALUE value) {
  if (NIL_P(value))
    return;
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array");
  for (long i = 0; i < RARRAY_LEN(value); ++i)
    VectorCodec<Ty>::Check(rb_ary_entry(value, i));
}

template <>
inline void CheckVectorOf<std::string>(VALUE value) {
  if (NIL_P(value) || RB_TYPE_P(value, T_STRING))
    return;
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array or String");
  for (long i = 0; i < RARRAY_LEN(value); ++i) {
    if (!RB_TYPE_P(rb_ary_entry(value, i), T_STRING))
      rb_raise(rb_eTypeError, "%s", "expected String elements");
  }
}

template <typename Ty>
inline std::vector<Ty> TakeVectorOf(VALUE value) {
  std::vector<Ty> result;
  if (NIL_P(value))
    return result;
  result.reserve(static_cast<std::size_t>(RARRAY_LEN(value)));
  for (long i = 0; i < RARRAY_LEN(value); ++i)
    result.push_back(VectorCodec<Ty>::FromUnchecked(rb_ary_entry(value, i)));
  return result;
}

template <>
inline std::vector<std::string> TakeVectorOf<std::string>(VALUE value) {
  std::vector<std::string> result;
  if (NIL_P(value))
    return result;
  if (RB_TYPE_P(value, T_STRING)) {
    result.push_back(VectorCodec<std::string>::FromUnchecked(value));
    return result;
  }
  result.reserve(static_cast<std::size_t>(RARRAY_LEN(value)));
  for (long i = 0; i < RARRAY_LEN(value); ++i)
    result.push_back(
        VectorCodec<std::string>::FromUnchecked(rb_ary_entry(value, i)));
  return result;
}

bool IsValidUTF8(VALUE str);

VALUE TagUTF8IfValid(VALUE str);

#define BINDING_ATTR_INT(ty, selfty, cap)       \
  RB_FUNC(ty##_##cap) {                         \
    auto* self_obj = GetSelfData<selfty>(self); \
    CheckArgc(argc, 0);                         \
    int32_t result = 0;                         \
    {                                           \
      result = *self_obj->Attr_##cap();         \
    }                                           \
    return INT2NUM(result);                     \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    auto* self_obj = GetSelfData<selfty>(self); \
    CheckArgc(argc, 1);                         \
    int value = 0;                              \
    ParseArgs(argc, argv, "i", &value);         \
    {                                           \
      self_obj->Attr_##cap(value);              \
    }                                           \
    return Qnil;                                \
  }

#define BINDING_ATTR_FLOAT(ty, selfty, cap)            \
  RB_FUNC(ty##_##cap) {                                \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 0);                                \
    double result = 0.0;                               \
    {                                                  \
      result = *self_obj->Attr_##cap();                \
    }                                                  \
    return rb_float_new(result);                       \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 1);                                \
    double value = 0.0;                                \
    ParseArgs(argc, argv, "f", &value);                \
    {                                                  \
      self_obj->Attr_##cap(static_cast<float>(value)); \
    }                                                  \
    return Qnil;                                       \
  }

#define BINDING_ATTR_BOOL(ty, selfty, cap)      \
  RB_FUNC(ty##_##cap) {                         \
    auto* self_obj = GetSelfData<selfty>(self); \
    CheckArgc(argc, 0);                         \
    bool result = false;                        \
    {                                           \
      result = *self_obj->Attr_##cap();         \
    }                                           \
    return result ? Qtrue : Qfalse;             \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    auto* self_obj = GetSelfData<selfty>(self); \
    CheckArgc(argc, 1);                         \
    bool value = false;                         \
    ParseArgs(argc, argv, "b", &value);         \
    {                                           \
      self_obj->Attr_##cap(value != 0);         \
    }                                           \
    return Qnil;                                \
  }

#define BINDING_ATTR_VECTOR(ty, selfty, cap, elem)             \
  RB_FUNC(ty##_##cap) {                                        \
    auto* self_obj = GetSelfData<selfty>(self);                \
    CheckArgc(argc, 0);                                        \
    std::vector<elem>* result = nullptr;                       \
    {                                                          \
      result = new std::vector<elem>(*self_obj->Attr_##cap()); \
    }                                                          \
    VALUE value = WrapVectorOf<elem>(*result);                 \
    delete result;                                             \
    return value;                                              \
  }                                                            \
  RB_FUNC(ty##_##cap##Equal) {                                 \
    auto* self_obj = GetSelfData<selfty>(self);                \
    CheckArgc(argc, 1);                                        \
    VALUE value = Qnil;                                        \
    ParseArgs(argc, argv, "o", &value);                        \
    CheckVectorOf<elem>(value);                                \
    {                                                          \
      std::vector<elem> items = TakeVectorOf<elem>(value);     \
      self_obj->Attr_##cap(items);                             \
    }                                                          \
    return Qnil;                                               \
  }

#define BINDING_ATTR_OBJECT(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                            \
    auto* self_obj = GetSelfData<selfty>(self);                    \
    CheckArgc(argc, 0);                                            \
    objty* result = nullptr;                                       \
    {                                                              \
      urge::RefPtr<objty> ptr = *self_obj->Attr_##cap();           \
      result = ptr.get();                                          \
      if (result)                                                  \
        result->AddRef();                                          \
    }                                                              \
    VALUE value = WrapObject(result, dataty);                      \
    if (result)                                                    \
      result->Release();                                           \
    return value;                                                  \
  }                                                                \
  RB_FUNC(ty##_##cap##Equal) {                                     \
    auto* self_obj = GetSelfData<selfty>(self);                    \
    CheckArgc(argc, 1);                                            \
    VALUE value = Qnil;                                            \
    ParseArgs(argc, argv, "o", &value);                            \
    objty* object = check<objty>(value, dataty);                   \
    {                                                              \
      self_obj->Attr_##cap(object);                                \
    }                                                              \
    return Qnil;                                                   \
  }

#define BINDING_ATTR_OBJECT_REF(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                                \
    auto* self_obj = GetSelfData<selfty>(self);                        \
    CheckArgc(argc, 0);                                                \
    objty* result = nullptr;                                           \
    {                                                                  \
      urge::RefPtr<objty> ptr = *self_obj->Attr_##cap();               \
      result = ptr.get();                                              \
      if (result)                                                      \
        result->AddRef();                                              \
    }                                                                  \
    VALUE cached = rb_iv_get(self, "_" #cap);                          \
    if (!NIL_P(cached)) {                                              \
      if (result)                                                      \
        result->Release();                                             \
      return cached;                                                   \
    }                                                                  \
    VALUE object = WrapObject(result, dataty);                         \
    if (result)                                                        \
      result->Release();                                               \
    rb_iv_set(self, "_" #cap, object);                                 \
    return object;                                                     \
  }                                                                    \
  RB_FUNC(ty##_##cap##Equal) {                                         \
    auto* self_obj = GetSelfData<selfty>(self);                        \
    CheckArgc(argc, 1);                                                \
    VALUE value = Qnil;                                                \
    ParseArgs(argc, argv, "o", &value);                                \
    objty* object = check<objty>(value, dataty);                       \
    {                                                                  \
      self_obj->Attr_##cap(object);                                    \
    }                                                                  \
    rb_iv_set(self, "_" #cap, value);                                  \
    return Qnil;                                                       \
  }

#define BINDING_CLASS_ATTR_INT(ty, selfty, cap) \
  RB_FUNC(ty##_##cap) {                         \
    CheckArgc(argc, 0);                         \
    int32_t result = 0;                         \
    {                                           \
      result = *selfty::Attr_##cap();           \
    }                                           \
    return INT2NUM(result);                     \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    CheckArgc(argc, 1);                         \
    int value = 0;                              \
    ParseArgs(argc, argv, "i", &value);         \
    {                                           \
      selfty::Attr_##cap(value);                \
    }                                           \
    return Qnil;                                \
  }

#define BINDING_CLASS_ATTR_FLOAT(ty, selfty, cap)    \
  RB_FUNC(ty##_##cap) {                              \
    CheckArgc(argc, 0);                              \
    double result = 0.0;                             \
    {                                                \
      result = *selfty::Attr_##cap();                \
    }                                                \
    return rb_float_new(result);                     \
  }                                                  \
  RB_FUNC(ty##_##cap##Equal) {                       \
    CheckArgc(argc, 1);                              \
    double value = 0.0;                              \
    ParseArgs(argc, argv, "f", &value);              \
    {                                                \
      selfty::Attr_##cap(static_cast<float>(value)); \
    }                                                \
    return Qnil;                                     \
  }

#define BINDING_CLASS_ATTR_BOOL(ty, selfty, cap) \
  RB_FUNC(ty##_##cap) {                          \
    CheckArgc(argc, 0);                          \
    bool result = false;                         \
    {                                            \
      result = *selfty::Attr_##cap();            \
    }                                            \
    return result ? Qtrue : Qfalse;              \
  }                                              \
  RB_FUNC(ty##_##cap##Equal) {                   \
    CheckArgc(argc, 1);                          \
    bool value = false;                          \
    ParseArgs(argc, argv, "b", &value);          \
    {                                            \
      selfty::Attr_##cap(value != 0);            \
    }                                            \
    return Qnil;                                 \
  }

#define BINDING_CLASS_ATTR_VECTOR(ty, selfty, cap, elem)     \
  RB_FUNC(ty##_##cap) {                                      \
    CheckArgc(argc, 0);                                      \
    std::vector<elem>* result = nullptr;                     \
    {                                                        \
      result = new std::vector<elem>(*selfty::Attr_##cap()); \
    }                                                        \
    VALUE value = WrapVectorOf<elem>(*result);               \
    delete result;                                           \
    return value;                                            \
  }                                                          \
  RB_FUNC(ty##_##cap##Equal) {                               \
    CheckArgc(argc, 1);                                      \
    VALUE value = Qnil;                                      \
    ParseArgs(argc, argv, "o", &value);                      \
    CheckVectorOf<elem>(value);                              \
    {                                                        \
      std::vector<elem> items = TakeVectorOf<elem>(value);   \
      selfty::Attr_##cap(items);                             \
    }                                                        \
    return Qnil;                                             \
  }

#define BINDING_CLASS_ATTR_OBJECT(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                                  \
    CheckArgc(argc, 0);                                                  \
    objty* result = nullptr;                                             \
    {                                                                    \
      urge::RefPtr<objty> ptr = *selfty::Attr_##cap();                   \
      result = ptr.get();                                                \
      if (result)                                                        \
        result->AddRef();                                                \
    }                                                                    \
    VALUE value = WrapObject(result, dataty);                            \
    if (result)                                                          \
      result->Release();                                                 \
    return value;                                                        \
  }                                                                      \
  RB_FUNC(ty##_##cap##Equal) {                                           \
    CheckArgc(argc, 1);                                                  \
    VALUE value = Qnil;                                                  \
    ParseArgs(argc, argv, "o", &value);                                  \
    objty* object = check<objty>(value, dataty);                         \
    {                                                                    \
      selfty::Attr_##cap(object);                                        \
    }                                                                    \
    return Qnil;                                                         \
  }

#define BINDING_ATTR_INT_EXC(ty, selfty, cap)        \
  RB_FUNC(ty##_##cap) {                              \
    urge::ExceptionState exception_state;            \
    auto* self_obj = GetSelfData<selfty>(self);      \
    CheckArgc(argc, 0);                              \
    int32_t result = 0;                              \
    result = *self_obj->Attr_##cap(exception_state); \
    RaiseException(exception_state);                 \
    return INT2NUM(result);                          \
  }                                                  \
  RB_FUNC(ty##_##cap##Equal) {                       \
    urge::ExceptionState exception_state;            \
    auto* self_obj = GetSelfData<selfty>(self);      \
    CheckArgc(argc, 1);                              \
    int value = 0;                                   \
    ParseArgs(argc, argv, "i", &value);              \
    self_obj->Attr_##cap(exception_state, value);    \
    RaiseException(exception_state);                 \
    return Qnil;                                     \
  }

#define BINDING_ATTR_BOOL_EXC(ty, selfty, cap)         \
  RB_FUNC(ty##_##cap) {                                \
    urge::ExceptionState exception_state;              \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 0);                                \
    bool result = false;                               \
    result = *self_obj->Attr_##cap(exception_state);   \
    RaiseException(exception_state);                   \
    return result ? Qtrue : Qfalse;                    \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    urge::ExceptionState exception_state;              \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 1);                                \
    bool value = false;                                \
    ParseArgs(argc, argv, "b", &value);                \
    self_obj->Attr_##cap(exception_state, value != 0); \
    RaiseException(exception_state);                   \
    return Qnil;                                       \
  }

#define BINDING_ATTR_OBJECT_REF_EXC(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                                    \
    urge::ExceptionState exception_state;                                  \
    auto* self_obj = GetSelfData<selfty>(self);                            \
    CheckArgc(argc, 0);                                                    \
    objty* result = nullptr;                                               \
    {                                                                      \
      urge::RefPtr<objty> ptr = *self_obj->Attr_##cap(exception_state);    \
      result = ptr.get();                                                  \
      if (result)                                                          \
        result->AddRef();                                                  \
    }                                                                      \
    RaiseException(exception_state);                                       \
    VALUE cached = rb_iv_get(self, "_" #cap);                              \
    if (!NIL_P(cached)) {                                                  \
      if (result)                                                          \
        result->Release();                                                 \
      return cached;                                                       \
    }                                                                      \
    VALUE object = WrapObject(result, dataty);                             \
    if (result)                                                            \
      result->Release();                                                   \
    rb_iv_set(self, "_" #cap, object);                                     \
    return object;                                                         \
  }                                                                        \
  RB_FUNC(ty##_##cap##Equal) {                                             \
    urge::ExceptionState exception_state;                                  \
    auto* self_obj = GetSelfData<selfty>(self);                            \
    CheckArgc(argc, 1);                                                    \
    VALUE value = Qnil;                                                    \
    ParseArgs(argc, argv, "o", &value);                                    \
    objty* object = check<objty>(value, dataty);                           \
    self_obj->Attr_##cap(exception_state, object);                         \
    RaiseException(exception_state);                                       \
    rb_iv_set(self, "_" #cap, value);                                      \
    return Qnil;                                                           \
  }

}

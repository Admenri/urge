#pragma once

#include <cctype>
#include <cstddef>
#include <cmath>
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
#include "bind/native_gc.h"

namespace binding {

// Managed datatypes deliberately remain WB-unprotected. Ruby must revisit
// their C++ children in minor GC and at the final incremental mark rescan.
#define RB_DATATYPE_PARENT(Klass, Name, Free, Parent)                 \
  const rb_data_type_t k##Klass##DataType = {                         \
      Name, {NativeWrapperMark, Free, NativeWrapperSize,             \
             NativeWrapperCompact, {}}, Parent, nullptr, 0}

#define RB_DATATYPE(Klass, Name, Free) \
  RB_DATATYPE_PARENT(Klass, Name, Free, nullptr)

#define RB_DECL_TYPE(Klass) extern const rb_data_type_t k##Klass##DataType;

#define RB_DEF_TYPE(Klass) \
  RB_DATATYPE(Klass, #Klass, NativeWrapperFree)

#define RB_DEF_TYPE_INHERIT(Klass, Parent) \
  RB_DATATYPE_PARENT(Klass, #Klass, NativeWrapperFree, &k##Parent##DataType)

// Allocation/retirement thresholds are serviced even by scripts which never
// call Graphics.update. These outer frames have only POD locals; Ruby longjmp
// never crosses the native call body's C++ owning temporaries. The return safe
// point also unfolds a script resumed from a suspended presentation at shutdown.
#define RB_FUNC(name)                                                   \
  static VALUE name##_body(int argc, VALUE* argv, VALUE self);           \
  static VALUE name(int argc, VALUE* argv, VALUE self) {                  \
    NativeGCSafePoint();                                                 \
    VALUE result = name##_body(argc, argv, self);                        \
    NativeGCSafePoint();                                                 \
    RB_GC_GUARD(result);                                                 \
    RB_GC_GUARD(self);                                                   \
    return result;                                                      \
  }                                                                     \
  static VALUE name##_body(int argc, VALUE* argv, VALUE self)

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

inline void CheckConstructorSelf(VALUE self, const rb_data_type_t& datatype) {
  rb_check_typeddata(self, &datatype);
  // Ruby subclasses inherit the same datatype and remain supported. A native
  // subclass has its own datatype and must run its own native constructor.
  if (RTYPEDDATA_TYPE(self) != &datatype)
    rb_raise(rb_eTypeError, "cannot initialize %s with %s constructor",
             RTYPEDDATA_TYPE(self)->wrap_struct_name, datatype.wrap_struct_name);
  CheckNativeSelfEmpty(self);
}

template <typename Ty>
inline VALUE SetupSelfData(VALUE self, Ty* data) {
  return SetupNativeSelf(self, data);
}

inline VALUE OwnedSetupSelf(VALUE self, urge::GarbageCollector::Root* root,
                            bool allocate_self = false) {
  return SetupOwnedNative(self, root, allocate_self);
}

inline VALUE OwnedWrapObject(urge::GarbageCollector::Root* root,
                             const rb_data_type_t& datatype) {
  return WrapOwnedNative(root, datatype);
}

[[noreturn]] void RaiseMissingInstanceData(VALUE self);

bool IsLegacyDisposable(VALUE self);
void CheckDisposedInstanceData(VALUE self);

template <typename Ty, bool kAllowDisposed = false>
inline Ty* GetSelfData(VALUE self) {
  if (!RTYPEDDATA_P(self))
    rb_raise(rb_eTypeError, "expected native instance data");
  Ty* ptr = nullptr;
  if constexpr (std::is_base_of_v<urge::GarbageCollected, Ty>)
    ptr = dynamic_cast<Ty*>(NativeInstanceData(RTYPEDDATA_DATA(self)));
  else
    ptr = static_cast<Ty*>(RTYPEDDATA_DATA(self));  // Win32API's non-GC payload
  if (!ptr)
    RaiseMissingInstanceData(self);
  if constexpr (!kAllowDisposed)
    CheckDisposedInstanceData(self);
  return ptr;
}

template <typename Ty>
inline VALUE WrapObject(Ty* ptr, const rb_data_type_t& type) {
  return WrapNativeObject(ptr, type);
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

inline VALUE RejectNativeCopy(int argc, VALUE* argv, VALUE self) {
  CheckArgc(argc, 1);
  if (self == argv[0])
    return self;
  rb_raise(rb_eTypeError, "this native type cannot be copied");
}

template <typename Ty, bool kNullable = false>
inline Ty* CheckObject(VALUE value, const rb_data_type_t& type) {
  if (!NIL_P(value)) {
    void* payload = rb_check_typeddata(value, &type);
    Ty* ptr = nullptr;
    if constexpr (std::is_base_of_v<urge::GarbageCollected, Ty>)
      ptr = dynamic_cast<Ty*>(NativeInstanceData(payload));
    else
      ptr = static_cast<Ty*>(payload);
    if (!ptr)
      RaiseMissingInstanceData(value);
    return ptr;
  }
  if constexpr (!kNullable)
    rb_raise(rb_eTypeError, "wrong argument type nil (expected %s)",
             type.wrap_struct_name);
  return nullptr;
}

template <typename Ty>
inline Ty* CheckMaybeObject(VALUE value, const rb_data_type_t& type) {
  return CheckObject<Ty, true>(value, type);
}

inline VALUE SnapshotString(VALUE string) {
  // rb_str_dup does not dispatch dup/initialize_copy. Shared storage is COW;
  // freezing our private copy keeps later coercions from changing its bytes.
  VALUE snapshot = rb_obj_freeze(rb_str_dup(string));
  RB_GC_GUARD(string);
  return snapshot;
}

inline VALUE CheckString(VALUE value) {
  return SnapshotString(rb_obj_as_string(value));
}

inline VALUE CheckCString(VALUE value) {
  StringValue(value);  // preserve the C-string parameter's to_str contract
  VALUE snapshot = SnapshotString(value);
  StringValueCStr(snapshot);  // reject embedded NUL before owning C++ locals
  RB_GC_GUARD(value);
  return snapshot;
}

// Only read buffers after ALL Ruby-capable checks, in the native-only phase.
inline std::string TakeString(VALUE snapshot) {
  return std::string(RSTRING_PTR(snapshot),
                     static_cast<size_t>(RSTRING_LEN(snapshot)));
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
inline Ty CheckNumericElement(VALUE value) {
  if (RB_FLOAT_TYPE_P(value)) {
    const double number = RFLOAT_VALUE(value);
    if constexpr (std::is_integral_v<Ty>) {
      // The upper bound is exclusive: double(UINT64_MAX/INT64_MAX) rounds
      // upwards. NaN must be rejected before any floating-to-integer cast.
      const double upper = std::ldexp(1.0, std::numeric_limits<Ty>::digits);
      const double lower = std::is_signed_v<Ty> ? -upper : 0.0;
      if (!std::isfinite(number) || number < lower || number >= upper)
        rb_raise(rb_eRangeError, "vector element out of range");
    }
    return static_cast<Ty>(number);
  }
  if (!RB_INTEGER_TYPE_P(value))
    rb_raise(rb_eTypeError, "%s", "expected numeric vector elements");
  if constexpr (std::is_integral_v<Ty>) {
    if constexpr (std::is_unsigned_v<Ty>) {
      if ((RB_FIXNUM_P(value) && FIX2LONG(value) < 0) ||
          (RB_TYPE_P(value, T_BIGNUM) && RBIGNUM_NEGATIVE_P(value)))
        rb_raise(rb_eRangeError, "vector element out of range");
      const unsigned long long number = NUM2ULL(value);
      if (number > (std::numeric_limits<Ty>::max)())
        rb_raise(rb_eRangeError, "vector element out of range");
      return static_cast<Ty>(number);
    } else {
      const long long number = NUM2LL(value);
      if (number < (std::numeric_limits<Ty>::min)() ||
          number > (std::numeric_limits<Ty>::max)())
        rb_raise(rb_eRangeError, "vector element out of range");
      return static_cast<Ty>(number);
    }
  } else {
    // rb_big2dbl can call Warning.warn, raise, throw, or reenter the engine.
    // This must run in phase 1, with only POD/Ruby-managed locals alive.
    return static_cast<Ty>(NUM2DBL(value));
  }
}

#define DEFINE_VECTOR_CODEC(Ty, ToRuby)                                \
  template <>                                                          \
  struct VectorCodec<Ty> {                                             \
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
  static VALUE To(const std::string& value) {
    return rb_enc_str_new(value.c_str(), static_cast<long>(value.size()),
                          rb_utf8_encoding());
  }
};

// Check returns a private normalized snapshot; callers MUST keep that VALUE,
// not the original input, for TakeVectorOf in the native-only phase.
template <typename Ty>
inline VALUE CheckVectorOf(VALUE value) {
  static_assert(std::is_arithmetic_v<Ty>);
  if (NIL_P(value))
    return Qnil;
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array");
  VALUE source = rb_obj_freeze(rb_ary_dup(value));
  const long count = RARRAY_LEN(source);
  if (static_cast<size_t>(count) >
      static_cast<size_t>((std::numeric_limits<long>::max)()) / sizeof(Ty))
    rb_raise(rb_eRangeError, "vector too large");
  VALUE snapshot = rb_str_buf_new(count * static_cast<long>(sizeof(Ty)));
  for (long i = 0; i < count; ++i) {
    const Ty number = CheckNumericElement<Ty>(rb_ary_entry(source, i));
    // No buffer address survives a Ruby-capable call. These native bytes are
    // internal, never exposed as a Ruby argument or converted a second time.
    rb_str_cat(snapshot, reinterpret_cast<const char*>(&number), sizeof(Ty));
  }
  rb_obj_freeze(snapshot);
  RB_GC_GUARD(source);
  RB_GC_GUARD(value);
  return snapshot;
}

template <>
inline VALUE CheckVectorOf<std::string>(VALUE value) {
  if (NIL_P(value))
    return Qnil;
  if (RB_TYPE_P(value, T_STRING))
    return SnapshotString(value);
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array or String");
  VALUE snapshot = rb_ary_dup(value);
  const long count = RARRAY_LEN(snapshot);
  for (long i = 0; i < count; ++i) {
    VALUE string = rb_ary_entry(snapshot, i);
    if (!RB_TYPE_P(string, T_STRING))
      rb_raise(rb_eTypeError, "%s", "expected String elements");
    rb_ary_store(snapshot, i, SnapshotString(string));
  }
  rb_obj_freeze(snapshot);
  RB_GC_GUARD(value);
  return snapshot;
}

template <typename Ty>
inline std::vector<Ty> TakeVectorOf(VALUE snapshot) {
  static_assert(std::is_arithmetic_v<Ty>);
  if (NIL_P(snapshot))
    return {};
  const size_t size = static_cast<size_t>(RSTRING_LEN(snapshot));
  std::vector<Ty> result(size / sizeof(Ty));
  if (size)
    std::memcpy(result.data(), RSTRING_PTR(snapshot), size);
  return result;
}

template <>
inline std::vector<std::string> TakeVectorOf<std::string>(VALUE snapshot) {
  std::vector<std::string> result;
  if (NIL_P(snapshot))
    return result;
  if (RB_TYPE_P(snapshot, T_STRING)) {
    result.push_back(TakeString(snapshot));
    return result;
  }
  const long count = RARRAY_LEN(snapshot);
  result.reserve(static_cast<size_t>(count));
  for (long i = 0; i < count; ++i)
    result.push_back(TakeString(RARRAY_AREF(snapshot, i)));
  return result;
}

// Handwritten aggregate conversions also need ensure-protected heap storage.
// The context and converter frames carry no owning C++ values.
template <typename Box>
inline VALUE OwnedConvertBox(Box* box, VALUE (*convert)(const Box&)) {
  struct Context {
    Box* box;
    VALUE (*convert)(const Box&);
  } context{box, convert};
  const VALUE opaque = reinterpret_cast<VALUE>(&context);
  return rb_ensure(
      [](VALUE data) -> VALUE {
        auto* context = reinterpret_cast<Context*>(data);
        return context->convert(*context->box);
      }, opaque,
      [](VALUE data) -> VALUE {
        delete reinterpret_cast<Context*>(data)->box;
        return Qnil;
      }, opaque);
}

// Boxed return values also use ensure: NoMemoryError and conversion hooks must
// not leak storage (in particular snapshots carrying external native roots).
template <typename Ty>
inline VALUE OwnedWrapVector(std::vector<Ty>* values) {
  const VALUE opaque = reinterpret_cast<VALUE>(values);
  return rb_ensure(
      [](VALUE data) -> VALUE {
        return WrapVectorOf<Ty>(*reinterpret_cast<std::vector<Ty>*>(data));
      }, opaque,
      [](VALUE data) -> VALUE {
        delete reinterpret_cast<std::vector<Ty>*>(data);
        return Qnil;
      }, opaque);
}
inline VALUE OwnedWrapString(std::string* text) {
  const VALUE opaque = reinterpret_cast<VALUE>(text);
  return rb_ensure(
      [](VALUE data) -> VALUE {
        auto* text = reinterpret_cast<std::string*>(data);
        return rb_enc_str_new(text->data(), static_cast<long>(text->size()),
                              rb_utf8_encoding());
      }, opaque,
      [](VALUE data) -> VALUE {
        delete reinterpret_cast<std::string*>(data);
        return Qnil;
      }, opaque);
}
template <typename Box>
inline void DeleteFailedResult(Box*& result,
                               const urge::ExceptionState& exception_state) {
  if (exception_state.HadException()) {
    delete result;
    result = nullptr;
  }
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
    return OwnedWrapVector<elem>(result);                       \
  }                                                            \
  RB_FUNC(ty##_##cap##Equal) {                                 \
    auto* self_obj = GetSelfData<selfty>(self);                \
    CheckArgc(argc, 1);                                        \
    VALUE value = Qnil;                                        \
    ParseArgs(argc, argv, "o", &value);                        \
    value = CheckVectorOf<elem>(value);                        \
    {                                                          \
      std::vector<elem> items = TakeVectorOf<elem>(value);     \
      self_obj->Attr_##cap(items);                             \
    }                                                          \
    RB_GC_GUARD(value);                                         \
    return Qnil;                                               \
  }

#define BINDING_ATTR_OBJECT_IMPL(ty, selfty, cap, objty, dataty, check, frozen_check) \
  RB_FUNC(ty##_##cap) {                                            \
    auto* self_obj = GetSelfData<selfty>(self);                    \
    CheckArgc(argc, 0);                                            \
    urge::GarbageCollector::Root* result = nullptr;                 \
    {                                                              \
      objty* value = *self_obj->Attr_##cap();                        \
      result = new urge::GarbageCollector::Root(value);             \
    }                                                              \
    return OwnedWrapObject(result, dataty);                         \
  }                                                                \
  RB_FUNC(ty##_##cap##Equal) {                                     \
    auto* self_obj = GetSelfData<selfty>(self);                    \
    CheckArgc(argc, 1);                                            \
    VALUE value = Qnil;                                            \
    ParseArgs(argc, argv, "o", &value);                            \
    objty* object = check<objty>(value, dataty);                   \
    frozen_check;                                                  \
    {                                                              \
      self_obj->Attr_##cap(object);                                \
    }                                                              \
    return Qnil;                                                   \
  }

#define BINDING_ATTR_OBJECT(ty, selfty, cap, objty, dataty, check) \
  BINDING_ATTR_OBJECT_IMPL(ty, selfty, cap, objty, dataty, check, (void)0)

// Canonical identity replaces lifecycle ivar caches. Only the old reference
// setters retain the FrozenError contract previously supplied by rb_iv_set.
#define BINDING_ATTR_OBJECT_REF(ty, selfty, cap, objty, dataty, check) \
  BINDING_ATTR_OBJECT_IMPL(ty, selfty, cap, objty, dataty, check, rb_check_frozen(self))

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
    return OwnedWrapVector<elem>(result);                     \
  }                                                          \
  RB_FUNC(ty##_##cap##Equal) {                               \
    CheckArgc(argc, 1);                                      \
    VALUE value = Qnil;                                      \
    ParseArgs(argc, argv, "o", &value);                      \
    value = CheckVectorOf<elem>(value);                      \
    {                                                        \
      std::vector<elem> items = TakeVectorOf<elem>(value);   \
      selfty::Attr_##cap(items);                             \
    }                                                        \
    RB_GC_GUARD(value);                                       \
    return Qnil;                                             \
  }

#define BINDING_CLASS_ATTR_OBJECT(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                                  \
    CheckArgc(argc, 0);                                                  \
    urge::GarbageCollector::Root* result = nullptr;                       \
    {                                                                    \
      objty* value = *selfty::Attr_##cap();                                \
      result = new urge::GarbageCollector::Root(value);                   \
    }                                                                    \
    return OwnedWrapObject(result, dataty);                               \
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
    urge::GarbageCollector::Root* result = nullptr;                         \
    {                                                                      \
      objty* value = *self_obj->Attr_##cap(exception_state);                \
      result = new urge::GarbageCollector::Root(value);                     \
    }                                                                      \
    DeleteFailedResult(result, exception_state);                          \
    RaiseException(exception_state);                                       \
    return OwnedWrapObject(result, dataty);                                 \
  }                                                                        \
  RB_FUNC(ty##_##cap##Equal) {                                             \
    urge::ExceptionState exception_state;                                  \
    auto* self_obj = GetSelfData<selfty>(self);                            \
    CheckArgc(argc, 1);                                                    \
    VALUE value = Qnil;                                                    \
    ParseArgs(argc, argv, "o", &value);                                    \
    objty* object = check<objty>(value, dataty);                           \
    rb_check_frozen(self);                                                 \
    self_obj->Attr_##cap(exception_state, object);                         \
    RaiseException(exception_state);                                       \
    return Qnil;                                                           \
  }

}

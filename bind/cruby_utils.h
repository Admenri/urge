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
// ruby/win32.h includes <winsock2.h>, which includes it -- and it #defines a
// few short names as macros.  A function-like macro silently rewrites the
// engine's own declarations, because core/*.h are parsed *after* this header:
//
//   WinUser.h  #define DrawText DrawTextA
//     => core/canvas/bitmap.h's `void Bitmap::DrawText(...)` became
//        `void Bitmap::DrawTextA(...)`, a member the engine never defines, and
//        every glue TU linked against `urge::Bitmap::DrawTextA` (LNK2019).
//
//   wingdi.h   #define ERROR 0
//     => today the only `ERROR` in core/ is inside a string literal
//        ("[URGE] [ERROR] " in core/common/logger.h), which a macro cannot touch, but
//        any future enumerator spelled ERROR would be rewritten without a word.
//
// Verified by compiling a probe that includes this header and core/canvas/bitmap.h:
// DrawText -> DrawTextA and ERROR -> 0 are both live, `max`/`min` are not (the
// build already neutralises minwindef.h).  Dropping the two here, after every
// ruby header and before the first core/ header, is what keeps the engine's
// spelling; nothing between the two points depends on either macro.
#undef DrawText
#undef ERROR

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/common/object.h"
#include "core/common/refptr.h"

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

// A derived class is *not* accepted where its base is expected on the strength
// of the C++ or the Ruby hierarchy alone.  `rb_check_typeddata` walks the
// `parent` chain of the *data type* -- `rb_typeddata_inherited_p` in error.c --
// and stops there, so `Node#parent=` (which asks for a `kNodeDataType`) rejects
// a `Viewport` VALUE with "wrong argument type Viewport (expected Node)" until
// the two types are linked.
//
// `RB_DEF_TYPE` leaves the link null for a class whose base is not exported (a
// `Disposable`, or one derived from `Singleton<T>`): nothing named it, so its
// values are only ever checked against their own type.  `RB_DEF_TYPE_INHERIT`
// sets it, and the generated glue picks whichever fits the class.  The base
// must be exported, and `k<Base>DataType` declared before the expansion --
// which is what the generated `binding_<base>.h` does (see Bindgen.md).
#define RB_DATATYPE_PARENT(Klass, Name, Free, Parent) \
  const rb_data_type_t k##Klass##DataType = {         \
      Name,                                           \
      {nullptr, Free, nullptr, DEF_TYPE_RESERVED{}},  \
      Parent,                                         \
      nullptr,                                        \
      DEF_TYPE_FLAGS}

#define RB_DATATYPE(Klass, Name, Free) \
  RB_DATATYPE_PARENT(Klass, Name, Free, nullptr)

#define RB_DECL_TYPE(Klass) extern const rb_data_type_t k##Klass##DataType;

/*! Defines `k<Klass>DataType` for a class whose base is not exported, so a
    value of it is only accepted where `Klass` itself is expected. */
#define RB_DEF_TYPE(Klass) \
  RB_DATATYPE(Klass, #Klass, ReleaseDataType<urge::Klass>)

/*! Defines `k<Klass>DataType` linked to `k<Parent>DataType`, so a value of
    `Klass` is also accepted wherever `Parent` is expected. */
#define RB_DEF_TYPE_INHERIT(Klass, Parent)                        \
  RB_DATATYPE_PARENT(Klass, #Klass, ReleaseDataType<urge::Klass>, \
                     &k##Parent##DataType)

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

// Exception process (legacy shape, hand-written glue only)
//
// `EXC_END` raises from inside the catch body, which is exactly what breaks on
// the web: a raise must not be issued from a frame that still holds a landing
// pad.  Generated glue uses `CPP_BEGIN`/`CPP_END` below instead.  The one
// remaining caller is `binding_win32api.cc`, which never builds for Emscripten.
#define EXC_BEGIN try
#define EXC_END                              \
  catch (const urge::Exception& exception) { \
    ProcessException(exception);             \
  }

// ---------------------------------------------------------------------------
// The three-phase bridge
//
// Every generated binding is one function with three phases:
//
//   RB_FUNC(Width) {
//     urge::Bitmap* self_obj = GetSelfData<urge::Bitmap>(self);  // 1. Ruby -> C
//     CheckArgc(argc, 0);                                       //    bare data,
//                                                               //    may raise
//     int32_t result = 0;                                       //    result slot
//     CPP_BEGIN {                                               // 2. pure C++
//       result = self_obj->Width();                             //    no Ruby API
//     } CPP_END;                                                //    the only
//                                                               //    try/catch
//     return LONG2NUM(result);                                  // 3. C++ -> Ruby
//   }
//
// Why the split is where it is, all measured with
// `build/web-test/frame_shape_probe.rb` against the web build:
//
//   * A frame that owns something with a destructor cannot raise.  The compiler
//     gives such a frame a landing pad for the cleanup, and the longjmp behind
//     `rb_raise` is dispatched into it.  Probe A (no try/catch anywhere, one
//     `std::string` alive) traps, so this is about the destructor alone and has
//     nothing to do with try/catch.  Hence phase 1 and phase 3 hold bare data
//     only.
//   * A raise issued inside a try region is dispatched into that region's
//     landing pad.  Probe C traps.
//   * A raise before or after a try region in the same frame is fine, as long
//     as the frame owns nothing.  Probes D, E and F pass -- which is why the
//     three phases can live in one function and nothing has to be split.
//   * Phase 2 may own whatever it likes: it is a real exception region, so
//     destructors run on both paths.
//
// The contract in one line: phase 2 contains no call that can raise or
// longjmp.  `generate_binding.py` enforces it (PHASE2_FORBIDDEN) instead of
// leaving it to review.
// ---------------------------------------------------------------------------

namespace detail {

//! What phase 2 leaves behind for phase 3 to raise.  Lives outside the frames
//! so the raise has nothing to destroy.
struct PendingError {
  bool active = false;
  VALUE klass = rb_eStandardError;
  char message[4096] = {};
};

inline PendingError& Pending() {
  static PendingError pending;
  return pending;
}

//! The Ruby class an engine error is reported as.
inline VALUE ErrorClass(urge::Exception::Type type) {
  switch (type) {
    case urge::Exception::kExitError:
      return rb_eSystemExit;
    case urge::Exception::kResetError:
      return g_reset_exception;
    case urge::Exception::kRGSSError:
      // GPU failures are engine side, but a game must still be able to catch
      // them with the same rescue clauses it uses for RGSS errors.
    case urge::Exception::kGPUError:
      return g_rgss_exception;
    case urge::Exception::kIOError:
      return rb_const_get(rb_const_get(rb_cObject, rb_intern("Errno")),
                          rb_intern("ENOENT"));
    default:
      return rb_eStandardError;
  }
}

inline std::string_view ExceptionTypeName(urge::Exception::Type type) {
  switch (type) {
    case urge::Exception::kRGSSError:
      return "RGSSError";
    case urge::Exception::kGPUError:
      return "GPUError";
    case urge::Exception::kIOError:
      return "IOError";
    case urge::Exception::kExitError:
      return "ExitError";
    case urge::Exception::kResetError:
      return "ResetError";
    default:
      return "Error";
  }
}

// Parking is also where the error is logged: on the web there is no message
// box, so this line is the only trace a failing core call leaves behind.
inline void ParkError(const urge::Exception& exception) {
  LOGGER_ERROR("[Binding] {}: {}", ExceptionTypeName(exception.type()),
               exception.message());
  auto& pending = Pending();
  pending.active = true;
  pending.klass = ErrorClass(exception.type());
  snprintf(pending.message, sizeof pending.message, "%s",
           exception.message().c_str());
}

//! A failure that is not an `urge::Exception`.
inline void ParkUnknown() {
  LOGGER_ERROR("[Binding] a non-engine C++ exception escaped a core call");
  auto& pending = Pending();
  pending.active = true;
  pending.klass = rb_eStandardError;
  snprintf(pending.message, sizeof pending.message, "%s",
           "a non-engine C++ exception escaped a core call");
}

}  // namespace detail

//! Turns what `CPP_END` parked into a Ruby exception; a no-op on the happy path.
/*!
    Called from the binding's own frame, which owns nothing by construction, so
    the raise reaches the VM `setjmp` instead of a landing pad.
*/
inline void RaisePending() {
  auto& pending = detail::Pending();
  if (!pending.active)
    return;
  const VALUE klass = pending.klass;
  const char* message = pending.message;
  pending.active = false;
  rb_raise(klass, "%s", message);
}

//! Phase 2's two ends: the only `try`/`catch` in the glue.
/*!
    The catch bodies only move data -- no raise, no allocation that could throw
    -- and `RaisePending()` runs after they have closed.
*/
#define CPP_BEGIN try
#define CPP_END                             \
  catch (const urge::Exception& exception) { \
    detail::ParkError(exception);           \
  }                                          \
  catch (...) {                              \
    detail::ParkUnknown();                   \
  }                                          \
  RaisePending()

// ---------------------------------------------------------------------------
// Phase 1 helpers: Ruby -> bare C
//
// These raise, so they may only be called from phase 1 or phase 3 -- the parts
// of the binding that run outside CPP_BEGIN.  Every one answers plain data,
// never an owning object, which is what keeps those frames destructor free.
// ---------------------------------------------------------------------------

//! Raises `ArgumentError` for a call whose valid arities are not a single value.
[[noreturn]] void ArgcError(int argc);

//! An object argument as a raw pointer.
/*!
    `RefPtr<Ty>` is constructible from `Ty*` and AddRefs, so the raw pointer
    converts at the call site -- inside phase 2, where the temporary it creates
    is destroyed by a real exception region.

    `kNullable` is the null contract of the parameter being read, and it comes
    from how the header spelled it: a plain `RefPtr<Ty>` is required, so `nil`
    is rejected here with the very `TypeError` `rb_check_typeddata` gives any
    other wrong type -- which is why the engine does not have to test for a
    null pointer on those parameters.  `MaybePtr<Ty>` accepts `nil` and reads
    it as `nullptr`; use `CheckMaybeObject` for it rather than the flag.
*/
template <typename Ty, bool kNullable = false>
inline Ty* CheckObject(VALUE value, const rb_data_type_t& type) {
  if (!NIL_P(value))
    return static_cast<Ty*>(rb_check_typeddata(value, &type));
  if constexpr (!kNullable)
    rb_raise(rb_eTypeError, "wrong argument type nil (expected %s)",
             type.wrap_struct_name);
  return nullptr;
}

//! The `MaybePtr<Ty>` form: `nil` is an argument, and reads as `nullptr`.
template <typename Ty>
inline Ty* CheckMaybeObject(VALUE value, const rb_data_type_t& type) {
  return CheckObject<Ty, true>(value, type);
}

//! A string argument as pointer plus length, without owning either.
/*!
    Same conversion ParseArgs uses for its `s` format, but the bytes stay in the
    Ruby object, so phase 1 only carries two scalars.  Phase 2 builds the
    `std::string` from the pair.
*/
inline void CheckString(VALUE value, const char** data, size_t* size) {
  VALUE string = rb_obj_as_string(value);
  *data = RSTRING_PTR(string);
  *size = static_cast<size_t>(RSTRING_LEN(string));
}

// ---------------------------------------------------------------------------
// std::vector<T>
//
// A `std::vector<T>` crosses the boundary as a Ruby Array of its elements.  The
// element codec is the only part that varies, so it is one trait with a
// `From`/`To` pair per element type, and `GetVectorOf` / `WrapVectorOf` are the
// two directions built on it: the generated glue names the type and nothing
// else (`GetVectorOf<int32_t>(value)` in, `WrapVectorOf<int32_t>(v)` out).
//
// `std::string` is the one element that is not a number, and it keeps two
// behaviours the numeric elements do not have -- a bare String stands in for a
// one element Array, and a wrong element is told apart from a wrong container
// -- so it specializes the reader as well as the trait.
//
// `bool` is deliberately absent: `std::vector<bool>` is a packed bitset whose
// element is a proxy rather than a `bool`, so it is not an interchangeable
// container.  Any element without a `VectorCodec` is not bindable at all, which
// is what keeps a `std::vector` of something unmarshalable out by accident.
// ---------------------------------------------------------------------------

/*! Converts one element of a `std::vector<Ty>` in both directions.  Only the
    specializations below exist; `gen_api_json.py` mirrors the list in
    `VECTOR_ELEMENTS`, so a new element has to be added in both places. */
template <typename Ty>
struct VectorCodec;

//! Reads `value` as `std::vector<Ty>`; `nil` reads as an empty vector.
template <typename Ty>
inline std::vector<Ty> GetVectorOf(VALUE value) {
  std::vector<Ty> result;
  if (NIL_P(value))
    return result;
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array");
  result.reserve(static_cast<std::size_t>(RARRAY_LEN(value)));
  for (long i = 0; i < RARRAY_LEN(value); ++i)
    result.push_back(VectorCodec<Ty>::From(rb_ary_entry(value, i)));
  return result;
}

//! Writes `values` as a Ruby Array of `Ty`.
template <typename Ty>
inline VALUE WrapVectorOf(const std::vector<Ty>& values) {
  VALUE array = rb_ary_new2(static_cast<long>(values.size()));
  for (const Ty& value : values)
    rb_ary_push(array, VectorCodec<Ty>::To(value));
  return array;
}

/*! Defines the element codec of one `std::vector<Ty>`.  `FromRuby` and `ToRuby`
    are spelled out with the element named `value`, so a codec whose conversion
    is not a plain `NUM2*`/`*2NUM` pair reads the same way. */
//! Validates one numeric element.  Raises, so phase 1 only.
/*!
    Phase 2 cannot afford a raise here: it converts with `FromUnchecked`, whose
    `NUM2LL`/`RFLOAT_VALUE` cannot fail once `Check` has accepted the value.  The
    range test is what makes that true -- a Bignum that does not fit is rejected
    by `NUM2LL` in phase 1, where raising is still allowed.
*/
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

/*! Defines the element codec of one numeric `std::vector<Ty>`.

    Four members, split by which phase may call them: `Check` raises and runs in
    phase 1, `FromUnchecked` cannot raise and runs in phase 2, `From`/`To` are
    the hand-written path.  `uint64_t`'s upper bound does not survive the
    `long long` comparison, so a value above `LLONG_MAX` is rejected; no
    `std::vector<uint64_t>` is exported today.
*/
#define DEFINE_VECTOR_CODEC(Ty, FromRuby, ToRuby)                      \
  template <>                                                          \
  struct VectorCodec<Ty> {                                             \
    static void Check(VALUE value) { CheckNumericElement<Ty>(value); }  \
    static Ty FromUnchecked(VALUE value) {                              \
      if (RB_FLOAT_TYPE_P(value))                                       \
        return static_cast<Ty>(RFLOAT_VALUE(value));                    \
      return static_cast<Ty>(NUM2LL(value));                            \
    }                                                                  \
    static Ty From(VALUE value) { return static_cast<Ty>(FromRuby); }  \
    static VALUE To(Ty value) { return (ToRuby); }                     \
  }

DEFINE_VECTOR_CODEC(int8_t, NUM2INT(value), INT2NUM(value));
DEFINE_VECTOR_CODEC(int16_t, NUM2INT(value), INT2NUM(value));
DEFINE_VECTOR_CODEC(int32_t, NUM2INT(value), INT2NUM(value));
DEFINE_VECTOR_CODEC(uint8_t, NUM2UINT(value), UINT2NUM(value));
DEFINE_VECTOR_CODEC(uint16_t, NUM2UINT(value), UINT2NUM(value));
DEFINE_VECTOR_CODEC(uint32_t, NUM2UINT(value), UINT2NUM(value));
DEFINE_VECTOR_CODEC(int64_t, NUM2LL(value), LL2NUM(value));
DEFINE_VECTOR_CODEC(uint64_t, NUM2ULL(value), ULL2NUM(value));
DEFINE_VECTOR_CODEC(float, NUM2DBL(value), rb_float_new(value));
DEFINE_VECTOR_CODEC(double, NUM2DBL(value), rb_float_new(value));
//! The string element, written out because it is not a numeric conversion.
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
  static std::string From(VALUE value) {
    return std::string(StringValueCStr(value));
  }
  static VALUE To(const std::string& value) {
    return rb_enc_str_new(value.c_str(), static_cast<long>(value.size()),
                          rb_utf8_encoding());
  }
};

//! The string vector, which also takes a bare String for a one element vector.
template <>
inline std::vector<std::string> GetVectorOf<std::string>(VALUE value) {
  std::vector<std::string> result;
  if (NIL_P(value))
    return result;
  if (RB_TYPE_P(value, T_STRING)) {
    result.push_back(VectorCodec<std::string>::From(value));
    return result;
  }
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array or String");
  for (long i = 0; i < RARRAY_LEN(value); ++i) {
    VALUE item = rb_ary_entry(value, i);
    if (!RB_TYPE_P(item, T_STRING))
      rb_raise(rb_eTypeError, "%s", "expected String elements");
    result.push_back(VectorCodec<std::string>::From(item));
  }
  return result;
}

// ---------------------------------------------------------------------------
// Vector arguments, split across the two phases
//
// Phase 1 cannot own the `std::vector` -- that would put a destructor in the
// raising frame -- so it only validates the Array.  Phase 2 builds the vector,
// where the element conversions are guaranteed not to raise because
// `CheckVectorOf` has already accepted every element.
// ---------------------------------------------------------------------------

//! Raises unless `value` is nil or an Array of elements `Ty` accepts.
template <typename Ty>
inline void CheckVectorOf(VALUE value) {
  if (NIL_P(value))
    return;
  if (!RB_TYPE_P(value, T_ARRAY))
    rb_raise(rb_eTypeError, "%s", "expected Array");
  for (long i = 0; i < RARRAY_LEN(value); ++i)
    VectorCodec<Ty>::Check(rb_ary_entry(value, i));
}

//! Same shape, but a bare String also stands in for a one element Array.
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

//! Builds the vector; only valid after `CheckVectorOf<Ty>` accepted `value`.
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
    CheckArgc(argc, 0);                         \
    int32_t result = 0;                         \
    CPP_BEGIN {                                 \
      result = *self_obj->Attr_##cap();         \
    } CPP_END;                                  \
    return INT2NUM(result);                     \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    auto* self_obj = GetSelfData<selfty>(self); \
    CheckArgc(argc, 1);                         \
    int value = 0;                              \
    ParseArgs(argc, argv, "i", &value);         \
    CPP_BEGIN {                                 \
      self_obj->Attr_##cap(value);              \
    } CPP_END;                                  \
    return Qnil;                                \
  }

#define BINDING_ATTR_FLOAT(ty, selfty, cap)            \
  RB_FUNC(ty##_##cap) {                                \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 0);                                \
    double result = 0.0;                               \
    CPP_BEGIN {                                        \
      result = *self_obj->Attr_##cap();                \
    } CPP_END;                                         \
    return rb_float_new(result);                       \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 1);                                \
    double value = 0.0;                                \
    ParseArgs(argc, argv, "f", &value);                \
    CPP_BEGIN {                                        \
      self_obj->Attr_##cap(static_cast<float>(value)); \
    } CPP_END;                                         \
    return Qnil;                                       \
  }

#define BINDING_ATTR_BOOL(ty, selfty, cap)             \
  RB_FUNC(ty##_##cap) {                                \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 0);                                \
    bool result = false;                               \
    CPP_BEGIN {                                        \
      result = *self_obj->Attr_##cap();                \
    } CPP_END;                                         \
    return result ? Qtrue : Qfalse;                    \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    auto* self_obj = GetSelfData<selfty>(self);        \
    CheckArgc(argc, 1);                                \
    bool value = false;                                \
    ParseArgs(argc, argv, "b", &value);                \
    CPP_BEGIN {                                        \
      self_obj->Attr_##cap(value != 0);                \
    } CPP_END;                                         \
    return Qnil;                                       \
  }

// The vector attribute hands phase 2 a boxed vector: the container cannot live
// in the phase-1/3 frame, and phase 3 needs it after the try has closed.  The
// box leaks only if the Ruby conversion raises, which is NoMemoryError.
#define BINDING_ATTR_VECTOR(ty, selfty, cap, elem)             \
  RB_FUNC(ty##_##cap) {                                        \
    auto* self_obj = GetSelfData<selfty>(self);                \
    CheckArgc(argc, 0);                                        \
    std::vector<elem>* result = nullptr;                       \
    CPP_BEGIN {                                                \
      result = new std::vector<elem>(*self_obj->Attr_##cap()); \
    } CPP_END;                                                 \
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
    CPP_BEGIN {                                                \
      std::vector<elem> items = TakeVectorOf<elem>(value);     \
      self_obj->Attr_##cap(items);                             \
    } CPP_END;                                                 \
    return Qnil;                                               \
  }

// The getter keeps a guard reference across the boundary: `result` is a raw
// pointer in a frame that must own nothing, so phase 2 AddRefs and phase 3
// Releases after `WrapObject` has taken its own.
//
// `check` is `CheckObject` or `CheckMaybeObject`, i.e. whether the attribute
// accepts `nil`; it is the setter's null contract and mirrors the `ATTR(ty,..)`
// spelling in the header.
#define BINDING_ATTR_OBJECT(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                         \
    auto* self_obj = GetSelfData<selfty>(self);                 \
    CheckArgc(argc, 0);                                         \
    objty* result = nullptr;                                    \
    CPP_BEGIN {                                                 \
      urge::RefPtr<objty> ptr = *self_obj->Attr_##cap();         \
      result = ptr.get();                                       \
      if (result) result->AddRef();                             \
    } CPP_END;                                                  \
    VALUE value = WrapObject(result, dataty);                   \
    if (result) result->Release();                              \
    return value;                                               \
  }                                                             \
  RB_FUNC(ty##_##cap##Equal) {                                  \
    auto* self_obj = GetSelfData<selfty>(self);                 \
    CheckArgc(argc, 1);                                         \
    VALUE value = Qnil;                                         \
    ParseArgs(argc, argv, "o", &value);                         \
    objty* object = check<objty>(value, dataty);                \
    CPP_BEGIN {                                                 \
      self_obj->Attr_##cap(object);                             \
    } CPP_END;                                                  \
    return Qnil;                                                \
  }

// Object attribute holding a reference the engine owns (Bitmap, Viewport,
// Font...). The wrapper is cached in an instance variable so that repeated
// reads answer the very same Ruby object and `equal?` keeps working.
#define BINDING_ATTR_OBJECT_REF(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                         \
    auto* self_obj = GetSelfData<selfty>(self);                 \
    CheckArgc(argc, 0);                                         \
    objty* result = nullptr;                                    \
    CPP_BEGIN {                                                 \
      urge::RefPtr<objty> ptr = *self_obj->Attr_##cap();         \
      result = ptr.get();                                       \
      if (result) result->AddRef();                             \
    } CPP_END;                                                  \
    VALUE cached = rb_iv_get(self, "_" #cap);                   \
    if (!NIL_P(cached)) {                                       \
      if (result) result->Release();                            \
      return cached;                                            \
    }                                                           \
    VALUE object = WrapObject(result, dataty);                  \
    if (result) result->Release();                              \
    rb_iv_set(self, "_" #cap, object);                          \
    return object;                                              \
  }                                                             \
  RB_FUNC(ty##_##cap##Equal) {                                  \
    auto* self_obj = GetSelfData<selfty>(self);                 \
    CheckArgc(argc, 1);                                         \
    VALUE value = Qnil;                                         \
    ParseArgs(argc, argv, "o", &value);                         \
    objty* object = check<objty>(value, dataty);                \
    CPP_BEGIN {                                                 \
      self_obj->Attr_##cap(object);                             \
    } CPP_END;                                                  \
    rb_iv_set(self, "_" #cap, value);                           \
    return Qnil;                                                \
  }

// Class (static) attribute. Same shape as the instance macros above, but the
// accessor is a class method and there is no `self` to read the object from.
#define BINDING_CLASS_ATTR_INT(ty, selfty, cap) \
  RB_FUNC(ty##_##cap) {                         \
    CheckArgc(argc, 0);                         \
    int32_t result = 0;                         \
    CPP_BEGIN {                                 \
      result = *selfty::Attr_##cap();           \
    } CPP_END;                                  \
    return INT2NUM(result);                     \
  }                                             \
  RB_FUNC(ty##_##cap##Equal) {                  \
    CheckArgc(argc, 1);                         \
    int value = 0;                              \
    ParseArgs(argc, argv, "i", &value);         \
    CPP_BEGIN {                                 \
      selfty::Attr_##cap(value);                \
    } CPP_END;                                  \
    return Qnil;                                \
  }

#define BINDING_CLASS_ATTR_FLOAT(ty, selfty, cap)      \
  RB_FUNC(ty##_##cap) {                                \
    CheckArgc(argc, 0);                                \
    double result = 0.0;                               \
    CPP_BEGIN {                                        \
      result = *selfty::Attr_##cap();                  \
    } CPP_END;                                         \
    return rb_float_new(result);                       \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    CheckArgc(argc, 1);                                \
    double value = 0.0;                                \
    ParseArgs(argc, argv, "f", &value);                \
    CPP_BEGIN {                                        \
      selfty::Attr_##cap(static_cast<float>(value));   \
    } CPP_END;                                         \
    return Qnil;                                       \
  }

#define BINDING_CLASS_ATTR_BOOL(ty, selfty, cap)       \
  RB_FUNC(ty##_##cap) {                                \
    CheckArgc(argc, 0);                                \
    bool result = false;                               \
    CPP_BEGIN {                                        \
      result = *selfty::Attr_##cap();                  \
    } CPP_END;                                         \
    return result ? Qtrue : Qfalse;                    \
  }                                                    \
  RB_FUNC(ty##_##cap##Equal) {                         \
    CheckArgc(argc, 1);                                \
    bool value = false;                                \
    ParseArgs(argc, argv, "b", &value);                \
    CPP_BEGIN {                                        \
      selfty::Attr_##cap(value != 0);                  \
    } CPP_END;                                         \
    return Qnil;                                       \
  }

#define BINDING_CLASS_ATTR_VECTOR(ty, selfty, cap, elem)         \
  RB_FUNC(ty##_##cap) {                                          \
    CheckArgc(argc, 0);                                          \
    std::vector<elem>* result = nullptr;                         \
    CPP_BEGIN {                                                  \
      result = new std::vector<elem>(*selfty::Attr_##cap());     \
    } CPP_END;                                                   \
    VALUE value = WrapVectorOf<elem>(*result);                   \
    delete result;                                               \
    return value;                                                \
  }                                                              \
  RB_FUNC(ty##_##cap##Equal) {                                   \
    CheckArgc(argc, 1);                                          \
    VALUE value = Qnil;                                          \
    ParseArgs(argc, argv, "o", &value);                          \
    CheckVectorOf<elem>(value);                                  \
    CPP_BEGIN {                                                  \
      std::vector<elem> items = TakeVectorOf<elem>(value);       \
      selfty::Attr_##cap(items);                                 \
    } CPP_END;                                                   \
    return Qnil;                                                 \
  }

#define BINDING_CLASS_ATTR_OBJECT(ty, selfty, cap, objty, dataty, check) \
  RB_FUNC(ty##_##cap) {                                           \
    CheckArgc(argc, 0);                                           \
    objty* result = nullptr;                                      \
    CPP_BEGIN {                                                   \
      urge::RefPtr<objty> ptr = *selfty::Attr_##cap();             \
      result = ptr.get();                                         \
      if (result) result->AddRef();                               \
    } CPP_END;                                                    \
    VALUE value = WrapObject(result, dataty);                     \
    if (result) result->Release();                                \
    return value;                                                 \
  }                                                               \
  RB_FUNC(ty##_##cap##Equal) {                                    \
    CheckArgc(argc, 1);                                           \
    VALUE value = Qnil;                                           \
    ParseArgs(argc, argv, "o", &value);                           \
    objty* object = check<objty>(value, dataty);                  \
    CPP_BEGIN {                                                   \
      selfty::Attr_##cap(object);                                 \
    } CPP_END;                                                    \
    return Qnil;                                                  \
  }

}  // namespace binding

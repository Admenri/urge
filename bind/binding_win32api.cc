// Hand-written binding -- see binding_win32api.h.
//
// `Win32API` is the classic Ruby extension an RGSS script uses to reach the
// Win32 API directly:
//
//     api = Win32API.new('kernel32', 'GetPrivateProfileStringA',
//                        %w[p p p p i p], 'i')
//     api.call(section, key, '', buffer, buffer.size, 'Game.ini')
//
// The extension the engine ships scripts against (Ruby 1.8's `ext/Win32API`,
// kept as a `dl`-backed shim in 1.9) built the call by pushing the arguments
// onto the stack itself, through a pointer to an array of 32-bit words.  That
// only works on 32-bit x86: on x64 the first four arguments travel in
// registers, a pointer is 64 bits wide, and there is no such array to point
// at.  libffi is what replaces the trick here -- it takes the signature and
// performs the ABI-specific call, so the Ruby-visible behaviour of the classic
// extension can be reproduced on x64.
//
// What is reproduced, and what is deliberately not:
//
//   * the four type kinds, with the classic numeric values and the classic
//     spellings (`N n L l` -> long, `I i` -> int, `P p` -> pointer, `V v` ->
//     void); a character that is none of them is skipped, as before;
//   * `import` as either a String of type characters or an Array of Strings
//     whose first character is one;
//   * the `LoadLibrary` / `GetProcAddress` pair, including the retry with an
//     "A" suffix when the bare export name is not found;
//   * the argument conversion: `nil` -> NULL, an Integer -> the pointer value,
//     anything else -> the address of its String buffer;
//   * the return conversion: `V` -> 0, `N`/`I` -> Integer, `P` -> the C string
//     at the returned address.
//
// The one thing that has to change with the word size is how wide a type code
// is.  The classic extension pushed one machine word per argument but narrowed
// the result to 32 bits, and both halves of that are kept: on x64 a `N`/`L`
// argument is 64 bits wide -- so a handle or a WPARAM survives being passed
// through one -- while a numeric *result* is still read as 32 bits, which is
// the only safe reading given that the Win64 ABI leaves the upper half of RAX
// undefined for a 32-bit return.  See ArgumentType / ResultType below.
//
// Three more places it knowingly differs:
//
//   * the classic refused more than 16 arguments, a limit of the fixed array it
//     passed by pointer.  libffi has no such limit, so neither does this;
//   * a `P` return of NULL is answered with an empty String rather than with
//     the crash `rb_str_new2((char *)0)` would have caused;
//   * a String handed to a `P` argument is made private with rb_str_modify
//     before its storage is passed on, so a frozen one raises where the classic
//     would have written into it anyway.  A buffer the API writes through has
//     to be the script's own, and the classic's silent corruption of a shared
//     String was a bug rather than a behaviour worth reproducing.
//
// `Win32API` has no meaning off Windows, so the whole implementation is behind
// `_WIN32` and the file compiles to a no-op initialiser everywhere else.

#include "binding_win32api.h"

#ifdef _WIN32

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// cruby_utils.h pulls in <windows.h> through ruby.h (thread_win32.h) and then
// neutralises the macros it defines; including it first keeps that ordering.
#include <windows.h>

#include "ffi.h"

namespace binding {
namespace {

// ---------------------------------------------------------------------------
// Type codes
//
// The four kinds carry the classic numeric values (`_T_VOID`/`_T_NUMBER`/
// `_T_POINTER`/`_T_INTEGER`) so the classification reads the same as the
// extension it replaces.
// ---------------------------------------------------------------------------

enum TypeKind {
  kVoid = 0,
  kNumber = 1,
  kPointer = 2,
  kInteger = 3,
};

//! Maps one type character onto a kind, or -1 when the character is not a type
//! code at all.  Case is not significant, exactly as in the classic extension.
int ClassifyType(char code) {
  switch (code) {
    case 'N':
    case 'n':
    case 'L':
    case 'l':
      return kNumber;
    case 'P':
    case 'p':
      return kPointer;
    case 'I':
    case 'i':
      return kInteger;
    case 'V':
    case 'v':
      return kVoid;
    default:
      return -1;
  }
}

//! The libffi type a kind occupies in an *argument* slot.
//!
//! The classic extension pushed one `unsigned long` per argument -- a machine
//! word -- and passed the array by pointer, so an argument's width was whatever
//! a word was on the host.  On x64 that is 64 bits, which is what keeps a
//! handle, a WPARAM or a buffer address intact when a script passes one through
//! `l`/`N`.  `I`/`i` stays 32 bits, because `int` is 32 bits on Windows in both
//! the ILP32 and the LLP64 model.
ffi_type* ArgumentType(int kind) {
  return kind == kInteger ? &ffi_type_sint32 : &ffi_type_pointer;
}

//! The libffi type a kind occupies in the *result* slot.
//!
//! The classic extension read the result as an `unsigned long` and handed it
//! straight to `INT2NUM`, so a numeric result was a 32-bit signed value even on
//! a machine where a word was wider.  That is kept, and it is also the only
//! safe reading: for a 32-bit return the Win64 ABI leaves the upper half of RAX
//! undefined, so widening the read would pick up garbage -- and would turn a
//! `-1` failure result into 4294967295.
ffi_type* ResultType(int kind) {
  return kind == kPointer ? &ffi_type_pointer : &ffi_type_sint32;
}

//! The calling convention named by `Win32API.new`'s fifth argument.
ffi_abi SelectAbi(VALUE calltype) {
#if defined(X86_WIN32)
  // The two conventions really do differ on 32-bit x86: a Win32 export is
  // __stdcall (the callee pops) while the C runtime's own are __cdecl.  The
  // default is :stdcall, which is what Win32API has always meant.
  if (!NIL_P(calltype)) {
    VALUE name = rb_obj_as_string(calltype);
    if (RSTRING_LEN(name) == 5 &&
        std::memcmp(RSTRING_PTR(name), "cdecl", 5) == 0)
      return FFI_MS_CDECL;
  }
  return FFI_STDCALL;
#else
  // On x64 there is one convention, so `:stdcall` and `:cdecl` name the same
  // thing and the argument is accepted only for compatibility.
  (void)calltype;
  return FFI_DEFAULT_ABI;
#endif
}

// ---------------------------------------------------------------------------
// Instance state
// ---------------------------------------------------------------------------

struct Win32APIObject {
  HMODULE module = nullptr;
  void* proc = nullptr;
  ffi_cif cif{};
  // Owned because ffi_prep_cif keeps the pointer, and the kinds are kept
  // alongside because `call` needs them per argument.
  std::vector<ffi_type*> arg_types;
  std::vector<int> arg_kinds;
  int result_kind = kVoid;
};

void Win32API_Free(void* pointer) {
  auto* object = static_cast<Win32APIObject*>(pointer);
  if (!object)
    return;
  if (object->module)
    ::FreeLibrary(object->module);
  delete object;
}

// Ruby 1.9.3's rb_data_type_t has no `flags` member, so the initialiser stops
// at `data`; the reserved slots inside `function` zero-fill.
const rb_data_type_t kWin32APIDataType = {
    "Win32API",
    {nullptr, Win32API_Free, nullptr},
    nullptr,
    nullptr,
};

// ---------------------------------------------------------------------------
// Win32API#initialize
// ---------------------------------------------------------------------------

RB_FUNC(Win32API_initialize) {
  if (argc < 3 || argc > 5)
    rb_raise(rb_eArgError, "wrong number of arguments (%d for 3..5)", argc);

  const char* dll_name = StringValueCStr(argv[0]);
  const char* proc_name = StringValueCStr(argv[1]);
  VALUE import = argv[2];
  VALUE export_type = argc >= 4 ? argv[3] : Qnil;
  VALUE calltype = argc >= 5 ? argv[4] : Qnil;

  auto* object = new Win32APIObject();

  object->module = ::LoadLibraryA(dll_name);
  if (!object->module) {
    delete object;
    rb_raise(rb_eRuntimeError, "LoadLibrary: %s", dll_name);
  }

  // The classic extension asks for the bare name first and retries with an "A"
  // suffix, so a caller that named the export without its W/A suffix still
  // reaches the ANSI entry point.
  object->proc =
      reinterpret_cast<void*>(::GetProcAddress(object->module, proc_name));
  if (!object->proc) {
    std::string ansi(proc_name);
    ansi += "A";
    object->proc =
        reinterpret_cast<void*>(::GetProcAddress(object->module, ansi.c_str()));
  }
  if (!object->proc) {
    Win32API_Free(object);
    rb_raise(rb_eRuntimeError, "GetProcAddress: %s", proc_name);
  }

  // `import` is a String whose every character is a type code, or an Array of
  // Strings whose first character is one.  A character that is not a type code
  // contributes no argument, which is how the classic extension behaved.
  if (!NIL_P(import)) {
    if (RB_TYPE_P(import, T_ARRAY)) {
      for (long i = 0; i < RARRAY_LEN(import); ++i) {
        // StringValueCStr is `rb_string_value_cstr(&(v))`, so its argument has
        // to be an lvalue -- the array element is read into one first.
        VALUE item = rb_ary_entry(import, i);
        const char* codes = StringValueCStr(item);
        int kind = ClassifyType(codes[0]);
        if (kind > kVoid)
          object->arg_kinds.push_back(kind);
      }
    } else {
      const char* codes = StringValueCStr(import);
      for (const char* cursor = codes; *cursor; ++cursor) {
        int kind = ClassifyType(*cursor);
        if (kind > kVoid)
          object->arg_kinds.push_back(kind);
      }
    }
  }

  // `export` is classified from its first character; anything unrecognised --
  // including an empty String, or the "0" the classic default spells -- means
  // void.
  object->result_kind = kVoid;
  if (!NIL_P(export_type)) {
    const char* codes = StringValueCStr(export_type);
    int kind = ClassifyType(codes[0]);
    object->result_kind = kind > kVoid ? kind : kVoid;
  }

  object->arg_types.reserve(object->arg_kinds.size());
  for (int kind : object->arg_kinds)
    object->arg_types.push_back(ArgumentType(kind));

  ffi_type* result_type =
      object->result_kind == kVoid ? &ffi_type_void : ResultType(object->result_kind);

  if (ffi_prep_cif(&object->cif, SelectAbi(calltype),
                   static_cast<unsigned int>(object->arg_types.size()),
                   result_type,
                   object->arg_types.empty() ? nullptr
                                             : object->arg_types.data()) !=
      FFI_OK) {
    Win32API_Free(object);
    rb_raise(rb_eRuntimeError, "ffi_prep_cif failed for %s", proc_name);
  }

  // `new` runs `allocate` first, so a re-`initialize` finds the previous state
  // still attached; release it rather than leaking the module handle.
  Win32API_Free(RTYPEDDATA_DATA(self));
  RTYPEDDATA_DATA(self) = object;
  return self;
}

// ---------------------------------------------------------------------------
// Win32API#call
// ---------------------------------------------------------------------------

RB_FUNC(Win32API_call) {
  auto* object = GetSelfData<Win32APIObject>(self);

  int expected = static_cast<int>(object->arg_kinds.size());
  if (argc != expected)
    rb_raise(rb_eRuntimeError,
             "wrong number of parameters: expected %d, got %d", expected, argc);

  // libffi wants an address per argument, so the values live in `slots` and
  // `arguments` points into it.  A pointer is 8 bytes and a number 4; storing
  // both in a 64-bit slot and letting libffi read the width the cif declares
  // is what keeps the two in one array.
  std::vector<uint64_t> slots(static_cast<std::size_t>(argc));
  std::vector<void*> arguments(static_cast<std::size_t>(argc));

  for (int i = 0; i < argc; ++i) {
    VALUE value = argv[i];
    if (object->arg_kinds[i] == kPointer) {
      if (NIL_P(value)) {
        slots[i] = 0;
      } else if (FIXNUM_P(value) || RB_TYPE_P(value, T_BIGNUM)) {
        // An Integer argument is the pointer value itself.
        slots[i] = static_cast<uint64_t>(NUM2LL(value));
      } else {
        // Anything else is treated as a buffer: the API may write through the
        // pointer, so it is handed the String's own storage and the caller
        // reads the result back out of the same String.
        //
        // The accessor is RSTRING_PTR, not StringValueCStr, and that is the
        // whole point of this branch: an out-parameter is a buffer full of
        // NULs -- `"\0" * 260` is the idiom -- and rb_string_value_cstr
        // rejects exactly those strings with "string contains null byte".
        // The classic extension reached for the raw pointer for the same
        // reason.  rb_str_modify first, so the storage is private and a write
        // through it cannot corrupt a String the buffer was sliced out of.
        StringValue(value);
        rb_str_modify(value);
        slots[i] = reinterpret_cast<uint64_t>(RSTRING_PTR(value));
      }
    } else {
      // A numeric argument is stored as a full machine word; libffi reads
      // exactly the width the cif declares, so `i` takes the low 32 bits while
      // `l` takes the whole word.  Storing the value whole rather than
      // range-checking is what lets -1 mean 0xFFFFFFFF for a DWORD parameter
      // and 0xFFFFFFFFFFFFFFFF for a word-sized one.
      slots[i] = static_cast<uint64_t>(NUM2LL(value));
    }
    arguments[i] = &slots[i];
  }

  // The return value is written into a variable of the kind the signature
  // declares; a void call gets no destination at all.
  int32_t number_result = 0;
  void* pointer_result = nullptr;
  void* result_address = nullptr;
  if (object->result_kind == kPointer)
    result_address = &pointer_result;
  else if (object->result_kind != kVoid)
    result_address = &number_result;

  ffi_call(&object->cif, FFI_FN(object->proc), result_address,
           arguments.empty() ? nullptr : arguments.data());

  switch (object->result_kind) {
    case kPointer:
      // The classic extension answered `rb_str_new2((char *)ret)`: the C
      // string the API left at the returned address.  A NULL return crashed it;
      // an empty String is the useful reading of the same case.
      if (!pointer_result)
        return rb_str_new("", 0);
      return rb_str_new_cstr(static_cast<const char*>(pointer_result));
    case kNumber:
    case kInteger:
      return INT2NUM(number_result);
    case kVoid:
    default:
      return INT2NUM(0);
  }
}

}  // namespace

void InitWin32APIBinding() {
  auto klass = rb_define_class("Win32API", rb_cObject);
  rb_define_alloc_func(klass, ClassAllocate<&kWin32APIDataType>);

  DefineMethod(klass, "initialize", Win32API_initialize);
  DefineMethod(klass, "call", Win32API_call);
  // The classic extension registered the alias explicitly.
  rb_define_alias(klass, "Call", "call");
}

}  // namespace binding

#else  // !_WIN32

namespace binding {

void InitWin32APIBinding() {
  // `Win32API` is the Windows API import facility; there is nothing for it to
  // import anywhere else, and no script can be relying on it there.
}

}  // namespace binding

#endif  // _WIN32

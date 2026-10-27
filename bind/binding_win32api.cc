#include "binding_win32api.h"

#ifdef _WIN32

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

#include "ffi.h"

namespace binding {
namespace {

enum TypeKind {
  kVoid = 0,
  kNumber = 1,
  kPointer = 2,
  kInteger = 3,
};

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

ffi_type* ArgumentType(int kind) {
  return kind == kInteger ? &ffi_type_sint32 : &ffi_type_pointer;
}

ffi_type* ResultType(int kind) {
  return kind == kPointer ? &ffi_type_pointer : &ffi_type_sint32;
}

ffi_abi SelectAbi(VALUE calltype) {
#if defined(X86_WIN32)
  if (!NIL_P(calltype)) {
    VALUE name = rb_obj_as_string(calltype);
    if (RSTRING_LEN(name) == 5 &&
        std::memcmp(RSTRING_PTR(name), "cdecl", 5) == 0)
      return FFI_MS_CDECL;
  }
  return FFI_STDCALL;
#else
  (void)calltype;
  return FFI_DEFAULT_ABI;
#endif
}

struct Win32APIObject {
  HMODULE module = nullptr;
  void* proc = nullptr;
  ffi_cif cif{};
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

const rb_data_type_t kWin32APIDataType = {
    "Win32API",
    {nullptr, Win32API_Free, nullptr},
    nullptr,
    nullptr,
};

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

  if (!NIL_P(import)) {
    if (RB_TYPE_P(import, T_ARRAY)) {
      for (long i = 0; i < RARRAY_LEN(import); ++i) {
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

  object->result_kind = kVoid;
  if (!NIL_P(export_type)) {
    const char* codes = StringValueCStr(export_type);
    int kind = ClassifyType(codes[0]);
    object->result_kind = kind > kVoid ? kind : kVoid;
  }

  object->arg_types.reserve(object->arg_kinds.size());
  for (int kind : object->arg_kinds)
    object->arg_types.push_back(ArgumentType(kind));

  ffi_type* result_type = object->result_kind == kVoid
                              ? &ffi_type_void
                              : ResultType(object->result_kind);

  if (ffi_prep_cif(
          &object->cif, SelectAbi(calltype),
          static_cast<unsigned int>(object->arg_types.size()), result_type,
          object->arg_types.empty() ? nullptr : object->arg_types.data()) !=
      FFI_OK) {
    Win32API_Free(object);
    rb_raise(rb_eRuntimeError, "ffi_prep_cif failed for %s", proc_name);
  }

  Win32API_Free(RTYPEDDATA_DATA(self));
  RTYPEDDATA_DATA(self) = object;
  return self;
}

RB_FUNC(Win32API_call) {
  auto* object = GetSelfData<Win32APIObject>(self);

  int expected = static_cast<int>(object->arg_kinds.size());
  if (argc != expected)
    rb_raise(rb_eRuntimeError,
             "wrong number of parameters: expected %d, got %d", expected, argc);

  std::vector<uint64_t> slots(static_cast<std::size_t>(argc));
  std::vector<void*> arguments(static_cast<std::size_t>(argc));

  for (int i = 0; i < argc; ++i) {
    VALUE value = argv[i];
    if (object->arg_kinds[i] == kPointer) {
      if (NIL_P(value)) {
        slots[i] = 0;
      } else if (FIXNUM_P(value) || RB_TYPE_P(value, T_BIGNUM)) {
        slots[i] = static_cast<uint64_t>(NUM2LL(value));
      } else {
        StringValue(value);
        rb_str_modify(value);
        slots[i] = reinterpret_cast<uint64_t>(RSTRING_PTR(value));
      }
    } else {
      slots[i] = static_cast<uint64_t>(NUM2LL(value));
    }
    arguments[i] = &slots[i];
  }

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

}

void InitWin32APIBinding() {
  auto klass = rb_define_class("Win32API", rb_cObject);
  rb_define_alloc_func(klass, ClassAllocate<&kWin32APIDataType>);

  DefineMethod(klass, "initialize", Win32API_initialize);
  DefineMethod(klass, "call", Win32API_call);
  rb_define_alias(klass, "Call", "call");
}

}

#else

namespace binding {

void InitWin32APIBinding() {}

}

#endif

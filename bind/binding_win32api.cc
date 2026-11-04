#include "binding_win32api.h"
#include "bind/win32api_common.h"

#ifdef _WIN32

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <windows.h>

#include "ffi.h"

namespace binding {
namespace {

using win32api::TypeKind;

ffi_type* ArgumentType(TypeKind kind) {
  return kind == TypeKind::kInteger ? &ffi_type_sint32 : &ffi_type_pointer;
}

ffi_type* ResultType(TypeKind kind) {
  return kind == TypeKind::kPointer ? &ffi_type_pointer : &ffi_type_sint32;
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

void* ResolveProcedure(HMODULE module, const char* name) {
  auto* proc = reinterpret_cast<void*>(::GetProcAddress(module, name));
  if (!proc) {
    std::string ansi(name);
    ansi += "A";
    proc = reinterpret_cast<void*>(::GetProcAddress(module, ansi.c_str()));
  }
  return proc;
}

RB_FUNC(Win32API_supported) {
  CheckArgc(argc, 2);
  win32api::ValidateNames(argv);
  const char* library = StringValueCStr(argv[0]);
  const char* procedure = StringValueCStr(argv[1]);
  const DWORD previous_error = ::GetLastError();
  const HMODULE module = ::LoadLibraryA(library);
  const bool found = module && ResolveProcedure(module, procedure);
  if (module)
    ::FreeLibrary(module);
  ::SetLastError(previous_error);
  return found ? Qtrue : Qfalse;
}

struct Win32APIObject {
  HMODULE module = nullptr;
  void* proc = nullptr;
  ffi_cif cif{};
  std::vector<ffi_type*> arg_types;
  win32api::Signature signature;
  std::string library;
  std::string procedure;
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

struct Initialization {
  int argc;
  VALUE* argv;
  VALUE self;
  Win32APIObject* object;
};

VALUE InitializeBody(VALUE opaque) {
  auto& context = *reinterpret_cast<Initialization*>(opaque);
  auto* object = context.object;
  VALUE* argv = context.argv;
  win32api::ValidateNames(argv);
  object->library = StringValueCStr(argv[0]);
  object->procedure = StringValueCStr(argv[1]);
  object->module = ::LoadLibraryA(object->library.c_str());
  if (!object->module)
    rb_raise(rb_eRuntimeError, "LoadLibrary: %s", object->library.c_str());
  object->proc = ResolveProcedure(object->module, object->procedure.c_str());
  if (!object->proc)
    rb_raise(rb_eRuntimeError, "GetProcAddress: %s", object->procedure.c_str());

  object->signature.ParseImports(argv[2], win32api::SignaturePolicy::kNative);
  object->signature.ParseResult(context.argc >= 4 ? argv[3] : Qnil,
                               win32api::SignaturePolicy::kNative);
  object->arg_types.reserve(object->signature.arguments.size());
  for (TypeKind kind : object->signature.arguments)
    object->arg_types.push_back(ArgumentType(kind));

  const auto result_kind = object->signature.result;
  ffi_type* result_type = result_kind == TypeKind::kVoid
                              ? &ffi_type_void
                              : ResultType(result_kind);
  if (ffi_prep_cif(
          &object->cif, SelectAbi(context.argc >= 5 ? argv[4] : Qnil),
          static_cast<unsigned int>(object->arg_types.size()), result_type,
          object->arg_types.empty() ? nullptr : object->arg_types.data()) !=
      FFI_OK)
    rb_raise(rb_eRuntimeError, "ffi_prep_cif failed for %s",
             object->procedure.c_str());
  return context.self;
}

RB_FUNC(Win32API_initialize) {
  win32api::CheckInitializeArgumentCount(argc);
  auto* object = new Win32APIObject;
  Initialization context{argc, argv, self, object};
  int state = 0;
  const VALUE result = rb_protect(InitializeBody,
                                 reinterpret_cast<VALUE>(&context), &state);
  // A signature's StringValue/to_str can raise after loading the DLL. Keep
  // parser storage and the temporary DLL reference owned across Ruby longjmp.
  if (state) {
    Win32API_Free(object);
    rb_jump_tag(state);
  }
  Win32API_Free(RTYPEDDATA_DATA(self));
  RTYPEDDATA_DATA(self) = object;
  return result;
}

RB_FUNC(Win32API_call) {
  auto* object = GetSelfData<Win32APIObject>(self);

  int expected = static_cast<int>(object->signature.arguments.size());
  if (argc != expected)
    rb_raise(rb_eRuntimeError,
             "wrong number of parameters: expected %d, got %d", expected, argc);

  std::vector<uint64_t> slots(static_cast<std::size_t>(argc));
  std::vector<void*> arguments(static_cast<std::size_t>(argc));

  for (int i = 0; i < argc; ++i) {
    VALUE value = argv[i];
    if (object->signature.arguments[i] == TypeKind::kPointer) {
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

#ifdef _WIN64
  int64_t number_result = 0;
#else
  int32_t number_result = 0;
#endif
  void* pointer_result = nullptr;
  void* result_address = nullptr;
  if (object->signature.result == TypeKind::kPointer)
    result_address = &pointer_result;
  else if (object->signature.result != TypeKind::kVoid)
    result_address = &number_result;

  ffi_call(&object->cif, FFI_FN(object->proc), result_address,
           arguments.empty() ? nullptr : arguments.data());

  switch (object->signature.result) {
    case TypeKind::kPointer:
      if (!pointer_result)
        return rb_str_new("", 0);
      return rb_str_new_cstr(static_cast<const char*>(pointer_result));
    case TypeKind::kNumber:
    case TypeKind::kInteger:
      return INT2NUM(static_cast<int32_t>(number_result));
    case TypeKind::kVoid:
    default:
      return INT2NUM(0);
  }
}

}

void InitWin32APIBinding() {
  win32api::DefineClass(ClassAllocate<&kWin32APIDataType>, Win32API_initialize,
                       Win32API_call, Win32API_supported);
}

}

#endif

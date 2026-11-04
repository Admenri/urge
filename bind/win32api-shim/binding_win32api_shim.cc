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

#include "bind/binding_win32api.h"
#include "bind/win32api-shim/win32api_memory.h"
#include "bind/win32api-shim/win32api_shim.h"
#include "bind/win32api_common.h"

#if !defined(_WIN32)
#include <array>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <utility>

namespace binding {
namespace {
using Kind = win32api::TypeKind;
constexpr size_t kMaxArguments = win32api::kMaxShimArguments;
struct ShimObject {
  const win32api::Function* function = nullptr;
  win32api::Function fallback{};
  std::string library;
  std::string procedure;
  win32api::Signature signature;
  bool ready = false;
  bool busy = false;
  std::string label;
  // Ruby raises with longjmp: own C++ result storage in the Ruby object so
  // error reporting and allocating a Ruby return value cannot leak it.
  win32api::Result result;
};
void Free(void* pointer) {
  delete static_cast<ShimObject*>(pointer);
}
const rb_data_type_t kShimDataType = {"Win32API",
                                      {nullptr, Free, nullptr},
                                      nullptr,
                                      nullptr};

std::string DisplayName(const char* name) {
  constexpr char hex[] = "0123456789ABCDEF";
  std::string result;
  for (size_t i = 0; name[i] && i < 256; ++i) {
    const auto ch = static_cast<unsigned char>(name[i]);
    if (ch >= 32 && ch < 127)
      result.push_back(static_cast<char>(ch));
    else {
      result += "\\x";
      result.push_back(hex[ch >> 4]);
      result.push_back(hex[ch & 15]);
    }
  }
  return result;
}

void WarnOnce(const ShimObject& object, bool memory) {
  static std::mutex mutex;
  static std::unordered_set<std::string> warned;
  // Use the full normalized identity for deduplication; display names are
  // escaped and bounded separately so script text cannot inject log lines.
  std::string key = win32api::NormalizeLibraryName(object.library);
  key.push_back('\0');
  key += object.procedure;
  key.push_back(memory ? 'M' : 'U');
  bool first;
  {
    std::lock_guard lock(mutex);
    first = warned.insert(std::move(key)).second;
  }
  if (first) {
    if (memory)
      LOGGER_WARN("[Win32API] {}: {}", object.label, object.result.warning);
    else
      LOGGER_WARN("[Win32API] Unsupported API {}; using no-op (error 120)",
                  object.label);
  }
}

struct Invocation {
  int argc;
  VALUE* argv;
  VALUE self;
  ShimObject* object;
};
VALUE ReleaseBusy(VALUE opaque) {
  auto* context = reinterpret_cast<Invocation*>(opaque);
  context->object->busy = false;
  return Qnil;
}
VALUE InitializeBody(VALUE opaque) {
  auto& context = *reinterpret_cast<Invocation*>(opaque);
  const int argc = context.argc;
  VALUE* argv = context.argv;
  const VALUE self = context.self;
  auto* object = context.object;
  win32api::CheckInitializeArgumentCount(argc);
  win32api::ValidateNames(argv);
  const char* library = StringValueCStr(argv[0]);
  const char* procedure = StringValueCStr(argv[1]);
  object->library = library;
  object->procedure = procedure;
  object->label = DisplayName(library) + "!" + DisplayName(procedure);
  object->function = win32api::GetRegistry().Find(library, procedure);
  object->signature.ParseImports(argv[2], win32api::SignaturePolicy::kShim);
  if (object->function &&
      object->signature.arguments.size() != object->function->arity)
    rb_raise(rb_eArgError,
             "Win32API shim: %s expects %d arguments, signature declares %d",
             object->label.c_str(), static_cast<int>(object->function->arity),
             static_cast<int>(object->signature.arguments.size()));
  object->signature.ParseResult(argc >= 4 ? argv[3] : Qnil,
                                win32api::SignaturePolicy::kShim);
  // Unknown exports use the caller's validated signature without mutating the
  // registry. Each instance owns its fallback so registry pointers stay stable.
  if (!object->function) {
    object->fallback = win32api::MakeNoOp(object->signature.arguments.size());
    object->function = &object->fallback;
  }
  if (object->function->noop)
    WarnOnce(*object, false);
  // The optional calling convention is meaningful only for a native DLL.
  object->ready = true;
  return self;
}

RB_FUNC(ShimInitialize) {
  win32api::CheckInitializeArgumentCount(argc);
  rb_check_frozen(self);
  auto* previous = static_cast<ShimObject*>(RTYPEDDATA_DATA(self));
  if (previous && previous->busy)
    rb_raise(rb_eRuntimeError,
             "Win32API shim: cannot reinitialize an active call");
  auto* object = new ShimObject;
  Free(previous);
  RTYPEDDATA_DATA(self) = object;
  object->busy = true;
  Invocation context{argc, argv, self, object};
  const VALUE opaque = reinterpret_cast<VALUE>(&context);
  return rb_ensure(InitializeBody, opaque, ReleaseBusy, opaque);
}

VALUE CallBody(VALUE opaque) {
  auto& context = *reinterpret_cast<Invocation*>(opaque);
  const int argc = context.argc;
  VALUE* argv = context.argv;
  auto* object = context.object;
  if (!object->ready)
    rb_raise(rb_eRuntimeError, "Win32API shim: uninitialized function");
  if (static_cast<size_t>(argc) != object->signature.arguments.size())
    rb_raise(rb_eArgError, "wrong number of arguments (%d for %d)", argc,
             static_cast<int>(object->signature.arguments.size()));
  if (object->function->noop) {
    // No-op calls must not coerce values, borrow buffers or invoke callbacks.
    // Only the declared argument count and return category are relevant.
    win32api::SetLastError(120);  // ERROR_CALL_NOT_IMPLEMENTED
    return object->signature.result == Kind::kPointer ? rb_str_new("", 0)
                                                      : INT2NUM(0);
  }
  std::array<VALUE, kMaxArguments> values{};
  std::array<win32api::Argument, kMaxArguments> arguments{};
  // Finish Ruby conversions (which can allocate or move a String) before
  // borrowing any buffer pointer. In particular, arguments may alias.
  for (int i = 0; i < argc; ++i) {
    VALUE value = argv[i];
    if (object->signature.arguments[i] == Kind::kPointer && !NIL_P(value) &&
        !RB_INTEGER_TYPE_P(value)) {
      StringValue(value);
    } else if (!NIL_P(value)) {
      arguments[i].number = NUM2LL(value);
    } else if (object->signature.arguments[i] != Kind::kPointer) {
      rb_raise(rb_eTypeError, "Win32API shim: nil is only valid for a pointer");
    }
    values[i] = value;
  }
  // Check every output before detaching any String; frozen buffers are a
  // rejected memory operation, not a Ruby exception. Conversions above may
  // themselves have frozen or aliased an earlier argument.
  for (int i = 0; i < argc; ++i) {
    if ((object->function->writable & (1u << i)) &&
        RB_TYPE_P(values[i], T_STRING) && OBJ_FROZEN(values[i])) {
      object->result = win32api::memory::Rejected(
          "memory operation skipped: frozen output buffer");
      WarnOnce(*object, true);
      return object->signature.result == Kind::kPointer ? rb_str_new("", 0)
                                                        : INT2NUM(0);
    }
  }
  for (int i = 0; i < argc; ++i)
    if ((object->function->writable & (1u << i)) &&
        RB_TYPE_P(values[i], T_STRING))
      rb_str_modify(values[i]);
  for (int i = 0; i < argc; ++i) {
    if (RB_TYPE_P(values[i], T_STRING)) {
      arguments[i].buffer = true;
      arguments[i].data = RSTRING_PTR(values[i]);
      arguments[i].size = static_cast<size_t>(RSTRING_LEN(values[i]));
    }
  }
  object->result =
      object->function->handler(std::span<const win32api::Argument>(
          arguments.data(), object->signature.arguments.size()));
  if (!object->result.warning.empty())
    WarnOnce(*object, true);
  if (!object->result.error.empty())
    rb_raise(rb_eRuntimeError, "Win32API shim: %s",
             object->result.error.c_str());
  if (object->result.destination >= argc)
    rb_raise(rb_eRuntimeError,
             "Win32API shim: invalid destination returned by backend");
  switch (object->signature.result) {
    case Kind::kPointer:
      if (object->result.destination >= 0 &&
          arguments[object->result.destination].buffer) {
        const auto text = arguments[object->result.destination].text();
        const VALUE result = rb_str_new(text.data(), text.size());
        for (int i = 0; i < argc; ++i)
          RB_GC_GUARD(values[i]);
        return result;
      }
      if (object->result.number != 0 && object->result.text.empty())
        rb_raise(rb_eRuntimeError,
                 "Win32API shim: raw pointer results are unsupported");
      return rb_str_new(object->result.text.data(), object->result.text.size());
    case Kind::kNumber:
    case Kind::kInteger:
      if (object->result.destination >= 0 &&
          arguments[object->result.destination].buffer)
        rb_raise(rb_eRuntimeError,
                 "Win32API shim: a String destination requires result P or V; "
                 "use malloc with result L for numeric pointers");
      return INT2NUM(static_cast<int32_t>(object->result.number));
    default:
      return INT2NUM(0);
  }
}
RB_FUNC(ShimCall) {
  auto* object = GetSelfData<ShimObject>(self);
  if (!object->ready)
    rb_raise(rb_eRuntimeError, "Win32API shim: uninitialized function");
  if (object->busy)
    rb_raise(
        rb_eRuntimeError,
        "Win32API shim: recursive calls on the same instance are unsupported");
  object->busy = true;
  Invocation context{argc, argv, self, object};
  const VALUE opaque = reinterpret_cast<VALUE>(&context);
  return rb_ensure(CallBody, opaque, ReleaseBusy, opaque);
}

RB_FUNC(ShimCopy) {
  rb_raise(rb_eTypeError, "Win32API shim: function instances cannot be copied");
}

RB_FUNC(ShimSupported) {
  CheckArgc(argc, 2);
  win32api::ValidateNames(argv);
  const char* library = StringValueCStr(argv[0]);
  const char* procedure = StringValueCStr(argv[1]);
  return win32api::GetRegistry().Find(library, procedure) ? Qtrue : Qfalse;
}
}  // namespace

void InitWin32APIBinding() {
  win32api::memory::Reset();
  win32api::DefineClass(ClassAllocate<&kShimDataType>, ShimInitialize, ShimCall,
                        ShimSupported, ShimCopy);
  rb_provide("Win32API.rb");
  rb_provide("win32api.rb");
}
}  // namespace binding
#endif

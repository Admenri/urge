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

#include "bind/win32api_common.h"

namespace binding::win32api {
namespace {
TypeKind ClassifyType(char code) {
  switch (code) {
    case 'N':
    case 'n':
    case 'L':
    case 'l':
      return TypeKind::kNumber;
    case 'I':
    case 'i':
      return TypeKind::kInteger;
    case 'P':
    case 'p':
      return TypeKind::kPointer;
    case 'V':
    case 'v':
      return TypeKind::kVoid;
    default:
      return TypeKind::kInvalid;
  }
}
}  // namespace

void Signature::ParseImports(VALUE imports, SignaturePolicy policy) {
  arguments.clear();
  const auto add = [&](char code) {
    const TypeKind kind = ClassifyType(code);
    if (kind == TypeKind::kInvalid) {
      if (policy == SignaturePolicy::kShim)
        rb_raise(rb_eArgError,
                 "Win32API shim: invalid argument type "
                 "(expected N, L, I, P or V)");
      return;
    }
    if (kind == TypeKind::kVoid)
      return;
    if (policy == SignaturePolicy::kShim &&
        arguments.size() == kMaxShimArguments)
      rb_raise(rb_eArgError, "Win32API shim: too many arguments");
    arguments.push_back(kind);
  };
  if (NIL_P(imports))
    return;
  if (RB_TYPE_P(imports, T_ARRAY)) {
    for (long i = 0; i < RARRAY_LEN(imports); ++i) {
      VALUE item = rb_ary_entry(imports, i);
      const char* code = StringValueCStr(item);
      if (policy == SignaturePolicy::kShim && (!code[0] || code[1]))
        rb_raise(rb_eArgError,
                 "Win32API shim: array types must be single characters");
      add(code[0]);
    }
  } else {
    const char* codes = StringValueCStr(imports);
    for (; *codes; ++codes)
      add(*codes);
  }
}

void Signature::ParseResult(VALUE export_type, SignaturePolicy policy) {
  result = TypeKind::kVoid;
  if (NIL_P(export_type))
    return;
  const char* code = StringValueCStr(export_type);
  const TypeKind kind = ClassifyType(code[0]);
  if (policy == SignaturePolicy::kShim &&
      (!code[0] || code[1] || kind == TypeKind::kInvalid))
    rb_raise(rb_eArgError, "Win32API shim: invalid result type");
  if (kind != TypeKind::kInvalid)
    result = kind;
}

void CheckInitializeArgumentCount(int argc) {
  if (argc < 3 || argc > 5)
    rb_raise(rb_eArgError, "wrong number of arguments (%d for 3..5)", argc);
}

void ValidateNames(VALUE* argv) {
  StringValue(argv[0]);
  StringValue(argv[1]);
  StringValueCStr(argv[0]);
  StringValueCStr(argv[1]);
}

VALUE DefineClass(VALUE (*allocate)(VALUE),
                  RubyMethod initialize,
                  RubyMethod call,
                  RubyMethod supported,
                  RubyMethod copy) {
  VALUE klass = rb_define_class("Win32API", rb_cObject);
  rb_define_alloc_func(klass, allocate);
  DefineMethod(klass, "initialize", initialize);
  DefineMethod(klass, "call", call);
  rb_define_alias(klass, "Call", "call");
  DefineClassMethod(klass, "supported?", supported);
  if (copy)
    DefineMethod(klass, "initialize_copy", copy);
  return klass;
}
}  // namespace binding::win32api

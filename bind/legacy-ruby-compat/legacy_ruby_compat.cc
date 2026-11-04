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

#include "bind/legacy-ruby-compat/legacy_ruby_compat.h"

#include <initializer_list>

#include "bind/legacy-ruby-compat/legacy_classes.h"
#include "ruby.h"

namespace binding {
namespace {

bool g_class_recovery_enabled = false;

// Ruby 1.8.1/1.9.2 entry points removed from current Ruby. Only missing names
// are installed; existing public, protected, and private methods take
// precedence. Sources and deliberately deferred behavior changes are listed in
// README.md.
bool HasMethod(VALUE owner, const char* name) {
  VALUE symbol = ID2SYM(rb_intern(name));
  return RTEST(rb_funcall(owner, rb_intern("method_defined?"), 1, symbol)) ||
         RTEST(rb_funcall(owner, rb_intern("private_method_defined?"), 1,
                          symbol));
}

void AliasIfMissing(VALUE owner, const char* name, const char* target) {
  if (!HasMethod(owner, name))
    rb_define_alias(owner, name, target);
}

VALUE ArrayNItems(VALUE self) {
  long count = 0;
  for (long i = 0; i < RARRAY_LEN(self); ++i) {
    if (!NIL_P(rb_ary_entry(self, i)))
      ++count;
  }
  return LONG2NUM(count);
}

VALUE ArrayIndices(int argc, VALUE* argv, VALUE self) {
  VALUE result = rb_ary_new_capa(argc);
  for (int i = 0; i < argc; ++i) {
    // Ruby 1.8.1 applies Array#[] to each selector separately. A Range is
    // therefore one nested slice, unlike the flattened Array#values_at.
    rb_ary_push(result, rb_ary_aref(1, argv + i, self));
  }
  return result;
}

void ConstantIfMissing(const char* name, VALUE value) {
  if (!rb_const_defined(rb_cObject, rb_intern(name)))
    rb_define_global_const(name, value);
}

// These removed security APIs exist only for script compatibility. Never
// modify object flags or activate the old VM's safe-level restrictions.
VALUE NoOpTaint(VALUE self) {
  return self;
}

VALUE NeverTainted(VALUE) {
  return Qfalse;
}

VALUE SafeLevel(ID, VALUE*) {
  return INT2FIX(0);
}

void IgnoreSafeLevel(VALUE, ID, VALUE*) {}

void InitSafetyAPIs(bool ruby19) {
  for (const char* name : {"taint", "untaint"}) {
    if (!HasMethod(rb_mKernel, name))
      rb_define_method(rb_mKernel, name, RUBY_METHOD_FUNC(NoOpTaint), 0);
  }
  if (!HasMethod(rb_mKernel, "tainted?"))
    rb_define_method(rb_mKernel, "tainted?", RUBY_METHOD_FUNC(NeverTainted), 0);

  // The separate trust API was introduced after the RGSS1/2 Ruby baseline.
  if (ruby19) {
    for (const char* name : {"trust", "untrust"}) {
      if (!HasMethod(rb_mKernel, name))
        rb_define_method(rb_mKernel, name, RUBY_METHOD_FUNC(NoOpTaint), 0);
    }
    if (!HasMethod(rb_mKernel, "untrusted?"))
      rb_define_method(rb_mKernel, "untrusted?", RUBY_METHOD_FUNC(NeverTainted),
                       0);
  }

  // A setter is required: a null setter makes the global read-only and raises
  // on assignment. Ignore writes without retaining any Ruby object or state.
  rb_define_virtual_variable("$SAFE", SafeLevel, IgnoreSafeLevel);
}

// Present in both original Ruby baselines; also needed by RGSS3.
void InitSharedAPIs() {
  // FileTest's module function has both a public singleton method and a
  // private instance method (used by `include FileTest`). Restore both forms.
  AliasIfMissing(rb_singleton_class(rb_cFile), "exists?", "exist?");
  AliasIfMissing(rb_singleton_class(rb_mFileTest), "exists?", "exist?");
  if (!HasMethod(rb_mFileTest, "exists?")) {
    rb_define_alias(rb_mFileTest, "exists?", "exist?");
    rb_funcall(rb_mFileTest, rb_intern("private"), 1,
               ID2SYM(rb_intern("exists?")));
  }

  AliasIfMissing(rb_cHash, "index", "key");
  AliasIfMissing(rb_singleton_class(rb_const_get(rb_cObject, rb_intern("ENV"))),
                 "index", "key");
  ConstantIfMissing("TRUE", Qtrue);
  ConstantIfMissing("FALSE", Qfalse);
  ConstantIfMissing("NIL", Qnil);
  // Keep legacy integer class names usable on the unified Integer class.
  ConstantIfMissing("Fixnum", rb_cInteger);
  ConstantIfMissing("Bignum", rb_cInteger);
}

// Removed before Ruby 1.9.2: do not install these names for RGSS3.
void InitRuby18APIs() {
  AliasIfMissing(rb_mKernel, "type", "class");
  AliasIfMissing(rb_mKernel, "id", "object_id");
  AliasIfMissing(rb_cHash, "indices", "values_at");
  AliasIfMissing(rb_cHash, "indexes", "values_at");
  if (!HasMethod(rb_cArray, "nitems"))
    rb_define_method(rb_cArray, "nitems", RUBY_METHOD_FUNC(ArrayNItems), 0);
  if (!HasMethod(rb_cArray, "indices"))
    rb_define_method(rb_cArray, "indices", RUBY_METHOD_FUNC(ArrayIndices), -1);
  if (!HasMethod(rb_cArray, "indexes"))
    rb_define_method(rb_cArray, "indexes", RUBY_METHOD_FUNC(ArrayIndices), -1);
}

}  // namespace

void InitLegacyRubyCompat(int rgss_version) {
  g_class_recovery_enabled = false;
  if (rgss_version < 1 || rgss_version > 3)
    return;

  InitSharedAPIs();
  InitSafetyAPIs(rgss_version == 3);
  InitLegacyClasses(rgss_version < 3);
  g_class_recovery_enabled = true;
  if (rgss_version == 3) {
    // Dir.exist?/exists? were present in 1.9.2, but not in 1.8.1.
    AliasIfMissing(rb_singleton_class(rb_cDir), "exists?", "exist?");
    return;
  }

  InitRuby18APIs();
}

VALUE EvalRubyScript(VALUE source, VALUE filename) {
  if (g_class_recovery_enabled) {
    VALUE compatibility =
        rb_const_get(rb_cObject, rb_intern("URGELegacyRubyCompat"));
    return rb_funcall(compatibility, rb_intern("evaluate"), 2, source,
                      filename);
  }
  VALUE scope =
      rb_funcall(rb_const_get(rb_cObject, rb_intern("TOPLEVEL_BINDING")),
                 rb_intern("dup"), 0);
  VALUE args[] = {source, scope, filename};
  return rb_funcallv(rb_mKernel, rb_intern("eval"), 3, args);
}
}  // namespace binding

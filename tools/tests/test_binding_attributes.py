#!/usr/bin/env python3
"""Narrow, engine-free regressions for typed/read-only ATTRs and binary returns.

Run: python -I tools/tests/test_binding_attributes.py
Only temporary fixture outputs are generated. The optional C++ test compiles
production integer helpers/macros with a tiny Ruby API stand-in, not the engine.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
API = runpy.run_path(str(ROOT / "bind/gen_api_json.py"))
GLUE = runpy.run_path(str(ROOT / "bind/generate_binding.py"))
INTEGER_TYPES = {
    "int8_t": (8, True), "uint8_t": (8, False),
    "int16_t": (16, True), "uint16_t": (16, False),
    "int32_t": (32, True), "uint32_t": (32, False),
    "int64_t": (64, True), "uint64_t": (64, False),
    "int": (32, True), "unsigned int": (32, False),
}


def parse_body(body: str, name: str = "Fixture") -> dict:
    entry = API["parse_class"](name, body, 0, 0, "fixture.h", body)
    entry.update(cpp_name=name, kind="class", name=name, cpp_parent=None,
                 ruby_superclass="Object", header="fixture.h")
    return entry


def emitted(entry: dict) -> str:
    model = GLUE["Model"]({"classes": [entry], "modules": []})
    return "\n".join(GLUE["emit_class"](model, entry))


def fixture_body() -> str:
    lines = ["public:"]
    for i, cpp_type in enumerate(INTEGER_TYPES):
        for static in (False, True):
            for exc in (False, True):
                for readonly in (False, True):
                    name = f"Value{i}{int(static)}{int(exc)}{int(readonly)}"
                    annotation = "ReadOnly: true" if readonly else ""
                    lines.append(f"URGE_BINDING({annotation}) "
                                 f"{'static ' if static else ''}"
                                 f"ATTR{'_EXC' if exc else ''}({cpp_type}, {name});")
    return "\n".join(lines)


class AttributeGenerationTests(unittest.TestCase):
    def test_annotation_ir_and_non_sticky_defaults(self) -> None:
        entry = parse_body('''public:
          URGE_BINDING(Name: "usage", ReadOnly: true) ATTR(uint32_t, Flags);
          URGE_BINDING(ReadOnly: true) static ATTR_EXC(uint64_t, MapState);
          URGE_BINDING(ReadOnly: false) ATTR(int16_t, Editable);
          URGE_BINDING() ATTR(uint64_t, Plain);
          URGE_BINDING(Name: "bytes", Binary: true) std::string Read(URGE_EXCEPT);
          URGE_BINDING(Binary: false) std::string Text();
        ''')
        usage, editable, plain = entry["attributes"]
        self.assertEqual((usage["name"], usage["setter"], usage["readonly"]),
                         ("usage", None, True))
        state = entry["class_attributes"][0]
        self.assertTrue(state["has_exception"])
        self.assertTrue(state["readonly"])
        self.assertIsNone(state["setter"])
        for attr in (editable, plain):
            self.assertEqual(attr["setter"], attr["name"] + "=")
            self.assertNotIn("readonly", attr)
        read, text = entry["instance_methods"]
        self.assertEqual(read["ruby_name"], "bytes")
        self.assertTrue(read["binary"])
        self.assertNotIn("binary", text)

    def test_every_integer_width_static_exception_and_readonly_combination(self) -> None:
        entry = parse_body(fixture_body())
        text = emitted(entry)
        for attr in entry["attributes"] + entry["class_attributes"]:
            with self.subTest(attr=attr):
                prefix = "BINDING_CLASS_ATTR" if attr["static"] else "BINDING_ATTR"
                suffix = "_INT_TYPED" + ("_GET" if attr.get("readonly") else "")
                suffix += "_EXC" if attr["has_exception"] else ""
                macro = GLUE["attr_macro"](entry, attr, attr["static"], set())
                self.assertEqual(macro, f'{prefix}{suffix}(Fixture, urge::Fixture, '
                                 f'{attr["cpp_name"]}, {attr["cpp_type"]});')
                setter = f'"{attr["name"]}=", Fixture_{attr["cpp_name"]}Equal'
                self.assertEqual(setter in text, not attr.get("readonly", False))
        # Null setter alone is also a valid IR representation of ReadOnly.
        attr = dict(entry["attributes"][0], setter=None)
        self.assertIn("_INT_TYPED_GET(", GLUE["attr_macro"](entry, attr, False, set()))

    def test_readonly_non_integer_attributes_emit_no_setter_body(self) -> None:
        for cpp_type in ("float", "double", "bool", "std::string",
                         "std::vector<uint32_t>", "RefPtr<Fixture>"):
            for static in (False, True):
                for exc in (False, True):
                    with self.subTest(cpp_type=cpp_type, static=static, exc=exc):
                        entry = parse_body('public: URGE_BINDING(ReadOnly: true) '
                                           f'{"static " if static else ""}'
                                           f'ATTR{"_EXC" if exc else ""}({cpp_type}, Value);')
                        text = emitted(entry)
                        self.assertIn("RB_FUNC(Fixture_Value)", text)
                        self.assertNotIn("Fixture_ValueEqual", text)
                        self.assertNotIn('"value="', text)
                        self.assertEqual("RaiseException(exception_state)" in text, exc)

    def test_module_readonly_and_typed_exception_paths(self) -> None:
        source = '''URGE_BINDING() class Input { public:
          URGE_BINDING(ReadOnly: true) ATTR_EXC(uint64_t, Mask);
          URGE_BINDING() ATTR_EXC(uint8_t, Narrow);
          URGE_BINDING(Binary: true) std::string Read(URGE_EXCEPT);
        };'''
        with tempfile.TemporaryDirectory(prefix="urge-module-ir-", dir=ROOT) as directory:
            Path(directory, "fixture.h").write_text(source, encoding="utf-8")
            with patch.dict(API["build_document"].__globals__, SOURCE_DIR=directory):
                entry = API["build_document"]()["modules"][0]
        functions = entry["functions"]
        mask = next(fn for fn in functions if fn["ruby_name"] == "mask")
        self.assertTrue(mask["readonly"])
        self.assertIsNone(mask["setter"])
        self.assertFalse(any(fn["ruby_name"] == "mask=" for fn in functions))
        self.assertTrue(all(fn["has_exception"] for fn in functions))
        text = "\n".join(GLUE["emit_module"](entry))
        self.assertIn("uint8_t value = CheckInteger<uint8_t>(value_arg);", text)
        self.assertIn("Attr_Narrow(exception_state, value)", text)
        self.assertIn("Attr_Mask(exception_state)", text)
        self.assertIn("rb_ascii8bit_encoding()", text)

    def test_binary_returns_per_overload_default_arity_and_receiver(self) -> None:
        entry = parse_body('''public:
          URGE_BINDING(Binary: true) std::string Read(URGE_EXCEPT, uint32_t n = 3);
          URGE_BINDING(Binary: true) static std::string StaticRead();
          URGE_BINDING(Binary: true) std::string Overloaded();
          URGE_BINDING() std::string Overloaded(int32_t n);
          URGE_BINDING() std::string Text();
        ''')
        text = emitted(entry)
        self.assertEqual(text.count("rb_ascii8bit_encoding()"), 4)
        self.assertEqual(text.count("rb_utf8_encoding()"), 2)
        self.assertIn("result->data(), static_cast<long>(result->size())", text)
        module = "\n".join(GLUE["emit_module_method"]("Input", [entry["instance_methods"][0]]))
        self.assertEqual(module.count("rb_ascii8bit_encoding()"), 2)
        with self.assertRaisesRegex(ValueError, "std::string return"):
            parse_body("public: URGE_BINDING(Binary: true) uint64_t Read();")
        with self.assertRaisesRegex(ValueError, "std::string return"):
            GLUE["emit_convert"]("uint64_t", "", True)

    def test_failed_string_return_releases_box_before_ruby_exception(self) -> None:
        entry = parse_body('''public:
          URGE_BINDING(Binary: true) std::string Read(URGE_EXCEPT, uint32_t n = 3);
          URGE_BINDING(Binary: true) static std::string StaticRead(URGE_EXCEPT);
          URGE_BINDING() std::string Plain();
        ''')
        cleanup = (r"if \(exception_state\.HadException\(\)\) \{\s*"
                   r"delete result;\s*result = nullptr;\s*\}\s*"
                   r"RaiseException\(exception_state\);")
        text = emitted(entry)
        self.assertEqual(len(re.findall(cleanup, text)), 3)
        module = "\n".join(GLUE["emit_module_method"]("Input", [entry["instance_methods"][0]]))
        self.assertEqual(len(re.findall(cleanup, module)), 2)
        plain = "\n".join(GLUE["emit_method"](entry, [entry["instance_methods"][1]], False))
        self.assertNotIn("exception_state", plain)
        self.assertIn("delete result;", plain)  # normal conversion cleanup remains

    def test_regeneration_restores_typed_getter_and_preserves_handwritten_blocks(self) -> None:
        entry = parse_body('''public:
          URGE_BINDING(ReadOnly: true) ATTR(uint64_t, Usage);
          URGE_BINDING(Binary: true) std::string Read();
          URGE_BINDING() enum class Flags : uint64_t { High = 1ULL << 63 };
        ''')
        model = GLUE["Model"]({"classes": [entry], "modules": []})
        with tempfile.TemporaryDirectory(prefix="urge-attr-generation-") as directory:
            path = Path(directory, "binding_fixture.cc")
            GLUE["generate_cc"](model, entry, str(path))
            text = path.read_text(encoding="utf-8")
            self.assertIn("_INT_TYPED_GET(Fixture, urge::Fixture, Usage, uint64_t)", text)
            self.assertIn("WrapConstant(urge::Fixture::Flags::High)", text)
            text = text.replace(GLUE["HANDWRITTEN_END"],
                                "// retain custom glue\n" + GLUE["HANDWRITTEN_END"])
            path.write_text(text, encoding="utf-8", newline="\r\n")
            before = path.read_bytes(), path.stat().st_mtime_ns
            GLUE["generate_cc"](model, entry, str(path))
            self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)
            stale = text.replace("_INT_TYPED_GET(Fixture, urge::Fixture, Usage, uint64_t)",
                                 "_INT(Fixture, urge::Fixture, Usage)")
            path.write_text(stale, encoding="utf-8", newline="\r\n")
            GLUE["generate_cc"](model, entry, str(path))
            restored = path.read_text(encoding="utf-8")
            self.assertIn("_INT_TYPED_GET(Fixture, urge::Fixture, Usage, uint64_t)", restored)
            self.assertIn("rb_ascii8bit_encoding()", restored)
            self.assertNotIn("Fixture_UsageEqual", restored)
            self.assertIn("// retain custom glue", restored)

    def test_production_typed_macros_compile_and_check_boundaries(self) -> None:
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler and os.name == "nt":
            installed = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM/bin/clang++.exe"
            if installed.is_file():
                compiler = str(installed)
        if not compiler:
            self.skipTest("a C++17 compiler is not installed")
        utils = (ROOT / "bind/cruby_utils.h").read_text(encoding="utf-8")
        wrap_start = utils.index("template <typename Ty>\ninline VALUE WrapConstant")
        wrap = utils[wrap_start:utils.index("int ParseArgs(", wrap_start)]
        typed_start = utils.index("// NUM2ULL deliberately wraps")
        typed = utils[typed_start:utils.index("#define BINDING_ATTR_FLOAT", typed_start)]

        def macro(name: str) -> str:
            lines = utils[utils.index("#define " + name + "("):].splitlines()
            end = 0
            while lines[end].endswith("\\"):
                end += 1
            return "\n".join(lines[:end + 1]) + "\n"

        typed += macro("BINDING_CLASS_ATTR_INT") + macro("BINDING_ATTR_INT_EXC")
        typed += macro("BINDING_CLASS_ATTR_INT_EXC")
        entry = parse_body(fixture_body())
        declarations, invocations, checks = [], [], []
        for attr in entry["attributes"] + entry["class_attributes"]:
            cpp_type, name = attr["cpp_type"], attr["cpp_name"]
            bits, signed = INTEGER_TYPES[cpp_type]
            params = "urge::ExceptionState& state, " if attr["has_exception"] else ""
            failure = "if (state.failed) return std::nullopt; " if attr["has_exception"] else ""
            declarations.append(f'{"static " if attr["static"] else ""}std::optional<{cpp_type}> '
                                f'Attr_{name}({params}std::optional<{cpp_type}> value = std::nullopt) '
                                f'{{ {failure}static {cpp_type} stored = 0; '
                                'if (value) { stored = *value; ++writes; } return stored; }')
            invocations.append(GLUE["attr_macro"](entry, attr, attr["static"], set()))
            maximum = (1 << (bits - int(signed))) - 1
            minimum = -(1 << (bits - 1)) if signed else 0
            symbol = f"Fixture_{name}"
            if attr.get("readonly"):
                checks.append(f"assert({symbol}(0, nullptr, nil).bits == 0);")
            else:
                for valid in (minimum, 0, maximum):
                    expression = f"negative({-valid}ULL)" if valid < 0 else f"positive({valid}ULL)"
                    checks.append(f"roundtrip({symbol}, {symbol}Equal, {expression});")
                if signed:
                    # Represent signed overflow as a Ruby bignum, not C++ overflow.
                    checks.append(f"reject({symbol}Equal, positive({maximum + 1}ULL));")
                    checks.append(f"reject({symbol}Equal, negative({-minimum + 1}ULL));")
                else:
                    checks.append(f"reject({symbol}Equal, negative(1));")
                    if bits < 64:
                        checks.append(f"reject({symbol}Equal, positive({maximum + 1}ULL));")
                checks.append(f"reject({symbol}Equal, huge);")
                if attr["has_exception"]:
                    checks.append(f"fail_next = true; expect_error({symbol});")
        # Existing three-argument generated calls also retain their C++ type.
        for i, (static, exc) in enumerate(((False, False), (True, False), (False, True), (True, True))):
            cap = f"Legacy{i}"
            params = "urge::ExceptionState&, " if exc else ""
            declarations.append(f'{"static " if static else ""}std::optional<uint64_t> Attr_{cap}'
                                f'({params}std::optional<uint64_t> v = std::nullopt) '
                                '{ static uint64_t stored = 0; if (v) stored = *v; return stored; }')
            prefix = "BINDING_CLASS_ATTR_INT" if static else "BINDING_ATTR_INT"
            invocations.append(f'{prefix}{"_EXC" if exc else ""}(Fixture, urge::Fixture, {cap});')
            checks.append(f'roundtrip(Fixture_{cap}, Fixture_{cap}Equal, positive(UINT64_MAX));')
        preamble = r'''
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>
struct VALUE { uint64_t bits; bool negative; bool huge; bool floating = false; };
VALUE positive(uint64_t n) { return {n, false, false}; }
VALUE negative(uint64_t n) { return {n, true, false}; }
VALUE nil = positive(0), Qnil = nil, huge = {0, false, true};
int writes = 0; bool fail_next = false;
struct RubyError {};
int rb_eRangeError = 0, rb_eArgError = 1;
[[noreturn]] void rb_raise(int, const char*, ...) { throw RubyError{}; }
VALUE rb_to_int(VALUE n) { return n; }
bool RB_FLOAT_TYPE_P(VALUE n) { return n.floating; }
double RFLOAT_VALUE(VALUE n) { return n.negative ? -0.5 : 0.5; }
bool RB_FIXNUM_P(VALUE n) { return !n.huge && n.bits < (1ULL << 30); }
long FIX2LONG(VALUE n) { return n.negative ? -long(n.bits) : long(n.bits); }
constexpr int T_BIGNUM = 1;
bool RB_TYPE_P(VALUE n, int type) { return type == T_BIGNUM && !RB_FIXNUM_P(n); }
bool RBIGNUM_NEGATIVE_P(VALUE n) { return n.negative; }
long long NUM2LL(VALUE n) {
  if (n.huge || n.bits > (n.negative ? (1ULL << 63) : uint64_t(INT64_MAX)))
    throw RubyError{};
  if (n.negative && n.bits == (1ULL << 63)) return INT64_MIN;
  return n.negative ? -static_cast<long long>(n.bits) : static_cast<long long>(n.bits);
}
uint64_t NUM2ULL(VALUE n) {
  if (n.huge) throw RubyError{};
  return n.negative ? uint64_t(0) - n.bits : n.bits; // Ruby's intentional wrap
}
VALUE LL2NUM(long long n) {
  return n < 0 ? negative(uint64_t(0) - static_cast<uint64_t>(n)) : positive(n);
}
VALUE ULL2NUM(uint64_t n) { return positive(n); }
void CheckArgc(int ac, int ex) { if (ac != ex) throw RubyError{}; }
void ParseArgs(int argc, VALUE* argv, const char* fmt, VALUE* out) {
  assert(argc == 1 && fmt[0] == 'o'); *out = *argv;
}
#define RB_FUNC(name) static VALUE name(int argc, VALUE* argv, VALUE self)
namespace urge {
struct ExceptionState { bool failed = fail_next; ExceptionState() { fail_next = false; } };
}
void RaiseException(const urge::ExceptionState& state) { if (state.failed) throw RubyError{}; }
template <typename Ty> Ty* GetSelfData(VALUE) { static Ty value; return &value; }
'''
        helpers = r'''
using Method = VALUE (*)(int, VALUE*, VALUE);
void roundtrip(Method get, Method set, VALUE value) {
  set(1, &value, nil); VALUE result = get(0, nullptr, nil);
  assert(result.bits == value.bits && result.negative == value.negative);
}
void reject(Method set, VALUE value) {
  int before = writes;
  try { set(1, &value, nil); assert(false); } catch (const RubyError&) {}
  assert(writes == before);
}
void expect_error(Method get) {
  try { get(0, nullptr, nil); assert(false); } catch (const RubyError&) {}
}
'''
        source = preamble + wrap + typed + "\nnamespace urge { class Fixture { public:\n"
        source += "\n".join(declarations) + "\n}; }\n" + "\n".join(invocations)
        source += helpers + "\nint main() {\n" + "\n".join(checks)
        source += '\nVALUE fraction = negative(0); fraction.floating = true; reject(Fixture_Value1000Equal, fraction);\n}\n'
        with tempfile.TemporaryDirectory(prefix="urge-typed-attr-cpp-", dir=ROOT) as directory:
            cpp = Path(directory, "attributes.cc")
            executable = Path(directory, "attributes.exe" if os.name == "nt" else "attributes")
            cpp.write_text(source, encoding="utf-8")
            compiled = subprocess.run([compiler, "-std=c++17", str(cpp), "-o", str(executable)],
                                      cwd=ROOT, capture_output=True, text=True, timeout=60)
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)

#!/usr/bin/env python3
"""Narrow, backend-free regressions for the Ruby constant generation pipeline.

Run: python -I tools/tests/test_binding_constants.py
Emit assertions for an existing Ruby VM: add --ruby-checks (no network calls).
All generated test outputs live in temporary directories, never bind/autogen.
"""

from __future__ import annotations

import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
API = runpy.run_path(str(ROOT / "bind/gen_api_json.py"))
GLUE = runpy.run_path(str(ROOT / "bind/generate_binding.py"))
WEBGPU = ROOT / "third_party/wgpu-cmake/wgpu-native/ffi/webgpu-headers/webgpu.h"

# Independent inventory of existing non-GPU constants and the new public
# selectors. Names and values must survive both cold and incremental generation.
EXPECTED = {
    "NetworkEvent": {"OPEN": 0, "MESSAGE": 1, "CLOSE": 2, "ERROR": 3},
    "WebSocket": {"CONNECTING": 0, "OPEN": 1, "CLOSING": 2, "CLOSED": 3},
    "Mouse": {"LEFT": 1, "MIDDLE": 2, "RIGHT": 3, "X1": 4, "X2": 5},
    "Input": {
        "DOWN": 2, "LEFT": 4, "RIGHT": 6, "UP": 8,
        "A": 11, "B": 12, "C": 13, "X": 14, "Y": 15, "Z": 16, "L": 17, "R": 18,
        "SHIFT": 21, "CTRL": 22, "ALT": 23,
        "F5": 25, "F6": 26, "F7": 27, "F8": 28, "F9": 29,
        "W": 31, "S": 32, "D": 33, "Q": 34, "E": 35,
    },
    "Gamepad": {
        "AXIS_LEFTX": 0, "AXIS_LEFTY": 1, "AXIS_RIGHTX": 2, "AXIS_RIGHTY": 3,
        "AXIS_LEFT_TRIGGER": 4, "AXIS_RIGHT_TRIGGER": 5,
        "BUTTON_SOUTH": 0, "BUTTON_EAST": 1, "BUTTON_WEST": 2, "BUTTON_NORTH": 3,
        "BUTTON_BACK": 4, "BUTTON_GUIDE": 5, "BUTTON_START": 6,
        "BUTTON_LEFT_STICK": 7, "BUTTON_RIGHT_STICK": 8,
        "BUTTON_LEFT_SHOULDER": 9, "BUTTON_RIGHT_SHOULDER": 10,
        "BUTTON_DPAD_UP": 11, "BUTTON_DPAD_DOWN": 12,
        "BUTTON_DPAD_LEFT": 13, "BUTTON_DPAD_RIGHT": 14,
        "BUTTON_MISC1": 15, "BUTTON_RIGHT_PADDLE1": 16, "BUTTON_LEFT_PADDLE1": 17,
        "BUTTON_RIGHT_PADDLE2": 18, "BUTTON_LEFT_PADDLE2": 19, "BUTTON_TOUCHPAD": 20,
        "BUTTON_MISC2": 21, "BUTTON_MISC3": 22, "BUTTON_MISC4": 23,
        "BUTTON_MISC5": 24, "BUTTON_MISC6": 25,
    },
    "Touch": {"MAX_FINGERS": 16},
    "URGE": {
        "MESSAGEBOX_ERROR": 16, "MESSAGEBOX_WARNING": 32, "MESSAGEBOX_INFORMATION": 64,
        "MESSAGEBOX_BUTTONS_LEFT_TO_RIGHT": 128, "MESSAGEBOX_BUTTONS_RIGHT_TO_LEFT": 256,
    },
    "Animation3D": {"TRANSLATION": 0, "ROTATION": 1, "SCALE": 2, "LINEAR": 0, "STEP": 1},
    "Light3D": {"DIRECTIONAL": 0, "POINT": 1},
    "Camera3D": {"CLEAR_NONE": 0, "CLEAR_COLOR": 1, "CLEAR_DEPTH": 2, "CLEAR_STENCIL": 4, "CLEAR_ALL": 7},
    "RenderGraphPass3D": {
        "RASTER": 0, "FULLSCREEN": 1, "COMPUTE": 2,
        "CLEAR_NONE": 0, "CLEAR_COLOR": 1, "CLEAR_DEPTH": 2, "CLEAR_STENCIL": 4, "CLEAR_ALL": 7,
    },
}


def entries(document: dict) -> dict:
    return {entry["name"]: entry for entry in document["modules"] + document["classes"]}


def parse_body(body: str) -> dict:
    return API["parse_class"]("Fixture", body, 0, 0, "fixture.h", body)


def wgpu_values() -> dict[tuple[str, str], int]:
    """Read numeric values from the vendored C API, not a build-tree header."""
    text = API["strip_comments"](WEBGPU.read_text(encoding="utf-8"))
    return {
        (kind, member): int(value, 0)
        for kind, member, value in re.findall(
            r"\bWGPU(\w+?)_(\w+)\s*=\s*(0x[\da-fA-F]+|\d+)\s*[,;]", text,
        )
    }


def gpu_expectations(entry: dict) -> dict[str, int]:
    values = wgpu_values()
    result = {}
    for constant in entry["constants"]:
        match = re.fullmatch(r"static_cast<uint32_t>\(wgpu::(\w+)::(\w+)\)", constant["value"])
        if not match:
            raise AssertionError("GPU constant is not backed by WebGPU: " + repr(constant))
        kind, member = match.groups()
        if re.match(r"e\d", member):
            member = member[1:]  # C++ e2D / C 2D
        result[constant["name"]] = values[kind, member]
    return result


def ruby_checks(document: dict) -> str:
    """Assertions only: usable by the real-engine test without any I/O API."""
    expected = dict(EXPECTED, GPU=gpu_expectations(entries(document)["GPU"]))
    lines = ["# Constant-only assertions; no network operations.", "expected_constants = {"]
    for owner, constants in expected.items():
        lines.append(f"  {owner} => {{")
        for name, value in constants.items():
            lines.append(f"    :{name} => {value},")
        lines.append("  },")
    lines.extend([
        "}",
        "expected_constants.each do |owner, constants|",
        "  constants.each do |name, expected|",
        "    actual = owner.const_get(name)",
        '    raise "#{owner}::#{name}: #{actual.inspect} != #{expected}" unless actual == expected',
        "  end",
        "end",
    ])
    return "\n".join(lines) + "\n"


class ConstantParsingTests(unittest.TestCase):
    def test_scoped_unscoped_and_anonymous_enums(self) -> None:
        result = parse_body("""
          public:
            URGE_BINDING() enum Plain { First = 7, Second };
            URGE_BINDING() enum class Scoped : unsigned long long { High = 1ULL << 63, Next };
            URGE_BINDING() enum struct Other : std::uint8_t { Small = 2 };
            URGE_BINDING() enum { Anonymous = 9 };
        """)
        self.assertEqual(result["constants"], [
            {"name": "FIRST", "cpp_name": "First", "value": "7"},
            {"name": "SECOND", "cpp_name": "Second", "value": None},
            {"name": "HIGH", "cpp_name": "Scoped::High", "value": "1ULL << 63"},
            {"name": "NEXT", "cpp_name": "Scoped::Next", "value": None},
            {"name": "SMALL", "cpp_name": "Other::Small", "value": "2"},
            {"name": "ANONYMOUS", "cpp_name": "Anonymous", "value": "9"},
        ])
        emitted = "\n".join(GLUE["emit_constants"]("Fixture", result, "klass"))
        self.assertIn("WrapConstant(urge::Fixture::Scoped::High)", emitted)
        self.assertIn("WrapConstant(urge::Fixture::Second)", emitted)
        self.assertNotIn("static_cast<long>", emitted)
        self.assertNotIn("1ULL", emitted)  # values are read from C++, never evaluated in Python

    def test_public_access_and_explicit_export_are_required(self) -> None:
        result = parse_body("""
            URGE_BINDING() enum DefaultPrivate { HiddenDefault };
          public:
            enum Unmarked { HiddenUnmarked };
            URGE_BINDING() enum Visible { Public };
          private:
            URGE_BINDING() enum Private { HiddenPrivate };
          protected:
            URGE_BINDING() enum Protected { HiddenProtected };
          public:
            URGE_BINDING() enum Restored { PublicAgain };
        """)
        self.assertEqual([c["name"] for c in result["constants"]], ["PUBLIC", "PUBLIC_AGAIN"])

    def test_names_and_sentinels(self) -> None:
        result = parse_body("""
          public:
            URGE_BINDING() enum State {
              URGE_BINDING(Name : "OPEN") OpenState = 1,
              URGE_BINDING(Name : "CLOSED") ClosedState = 3,
              kTranslation = 0, AXIS_LEFTX = 2,
              Count, kButtonCount, AXIS_COUNT, Force32 = 0x7fffffff,
              URGE_BINDING(Name : "NOT_A_SENTINEL") StateForce32 = 0x7fffffff,
            };
        """)
        self.assertEqual([c["name"] for c in result["constants"]], ["OPEN", "CLOSED", "TRANSLATION", "AXIS_LEFTX"])
        self.assertEqual(result["constants"][0]["cpp_name"], "OpenState")

    def test_explicit_integral_constexpr(self) -> None:
        result = parse_body("""
          public:
            URGE_BINDING(Name : "MAX_FINGERS") static constexpr int32_t kLimit = 16;
            URGE_BINDING() static constexpr uint64_t Wide = 0xffffffffffffffffULL;
            static constexpr int32_t Unmarked = 2;
            URGE_BINDING() static constexpr float NonInteger = 3.5f;
            URGE_BINDING() static constexpr uint32_t kCount = 4;
          private:
            URGE_BINDING() static constexpr int32_t Internal = 3;
        """)
        self.assertEqual([c["name"] for c in result["constants"]], ["MAX_FINGERS", "WIDE"])
        emitted = "\n".join(GLUE["emit_constants"]("Fixture", result, "mod"))
        self.assertIn("WrapConstant(urge::Fixture::Wide)", emitted)

    def test_generated_cpp_scope_and_integer_width(self) -> None:
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler and os.name == "nt":
            installed = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "LLVM/bin/clang++.exe"
            if installed.is_file():
                compiler = str(installed)
        if not compiler:
            self.skipTest("a C++17 compiler is not installed")
        body = """
          public:
            URGE_BINDING() enum class Wide : uint64_t { Mask = 0xffffffffffffffffULL };
            URGE_BINDING() enum class Narrow : uint32_t { High = 0x80000000u };
            URGE_BINDING() enum class Signed : int64_t { Negative = -1 };
            URGE_BINDING() enum { Plain = 7 };
        """
        emitted = "\n".join(GLUE["emit_constants"]("Fixture", parse_body(body), "klass"))
        utils = (ROOT / "bind/cruby_utils.h").read_text(encoding="utf-8")
        start = utils.index("template <typename Ty>\ninline VALUE WrapConstant")
        helper = utils[start:utils.index("int ParseArgs(", start)]
        source = """
          #include <cassert>
          #include <cstdint>
          #include <cstring>
          #include <type_traits>
          #define URGE_BINDING(...)
          struct VALUE { bool is_unsigned; uint64_t bits; };
          VALUE LL2NUM(long long n) { return {false, static_cast<uint64_t>(n)}; }
          VALUE ULL2NUM(unsigned long long n) { return {true, n}; }
          const char* rb_intern(const char* name) { return name; }
          int calls = 0;
          void rb_const_set(int, const char* name, VALUE value) {
            ++calls;
            if (std::strcmp(name, "MASK") == 0) {
              assert(value.is_unsigned && value.bits == UINT64_MAX);
            } else if (std::strcmp(name, "HIGH") == 0) {
              assert(value.is_unsigned && value.bits == 0x80000000ULL);
            } else if (std::strcmp(name, "NEGATIVE") == 0) {
              assert(!value.is_unsigned && value.bits == UINT64_MAX);
            } else {
              assert(std::strcmp(name, "PLAIN") == 0 && value.bits == 7);
            }
          }
        """ + helper + "\nnamespace urge { class Fixture {" + body + "}; }\n" + "\nint main() { int klass = 0;\n" + emitted + "\nassert(calls == 4); }\n"
        with tempfile.TemporaryDirectory(prefix="urge-constant-cpp-", dir=ROOT) as directory:
            work = Path(directory)
            cpp = work / "constants.cc"
            executable = work / ("constants.exe" if os.name == "nt" else "constants")
            cpp.write_text(source, encoding="utf-8")
            compiled = subprocess.run(
                [compiler, "-std=c++17", str(cpp), "-o", str(executable)],
                cwd=ROOT, capture_output=True, text=True, encoding="utf-8", timeout=60,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stdout + compiled.stderr)
            result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_duplicate_or_invalid_ruby_names_fail_early(self) -> None:
        for body in (
            'public: URGE_BINDING() enum E { URGE_BINDING(Name : "open") Open };',
            'public: URGE_BINDING() enum E { Open, URGE_BINDING(Name : "OPEN") Other };',
            'public: URGE_BINDING(Name : "not_a_constant") static constexpr uint32_t Limit = 3;',
        ):
            with self.subTest(body=body), self.assertRaises(ValueError):
                parse_body(body)


class ProjectConstantTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.document = API["build_document"]()
        cls.by_name = entries(cls.document)

    def test_inventory_and_existing_names(self) -> None:
        owners = {name for name, entry in self.by_name.items() if entry["constants"]}
        self.assertEqual(owners, set(EXPECTED) | {"GPU"})
        for name, expected in EXPECTED.items():
            with self.subTest(owner=name):
                self.assertEqual({c["name"] for c in self.by_name[name]["constants"]}, set(expected))
        self.assertNotIn("OPEN_STATE", {c["name"] for c in self.by_name["WebSocket"]["constants"]})
        self.assertNotIn("CLOSED_STATE", {c["name"] for c in self.by_name["WebSocket"]["constants"]})
        self.assertEqual(self.by_name["Table"]["index"]["get"]["return"]["cpp_type"], "int16_t")
        self.assertEqual(self.by_name["Table"]["index"]["set"]["return"]["cpp_type"], "void")

    def test_gpu_mirrors_every_exposed_enum_without_implementation_sentinels(self) -> None:
        gpu = self.by_name["GPU"]
        values = wgpu_values()
        mirrored = {}
        for constant in gpu["constants"]:
            kind, member = re.search(r"wgpu::(\w+)::(\w+)", constant["value"]).groups()
            if re.match(r"e\d", member):
                member = member[1:]
            mirrored.setdefault(kind, set()).add(member)
        # MAP_READ / MAP_WRITE already serve both map mode and buffer usage.
        mirrored["BufferUsage"].update(("MapRead", "MapWrite"))
        for kind, actual in mirrored.items():
            with self.subTest(enum=kind):
                expected = {member for owner, member in values if owner == kind}
                expected -= {"Force32", "BindingNotUsed"}
                self.assertEqual(actual, expected)
        self.assertEqual(len(gpu["constants"]), 276)
        expected = gpu_expectations(gpu)
        self.assertEqual(expected["TEXTURE_COPY_SRC"], 1)
        self.assertEqual(expected["TEXTURE_COPY_DST"], 2)
        self.assertEqual(expected["MAP_READ"], values["BufferUsage", "MapRead"])
        self.assertEqual(expected["MAP_WRITE"], values["BufferUsage", "MapWrite"])
        # Historical naming includes the dimension digit before D and CCW/CW.
        for legacy in ("COPY_SRC", "COPY_DST", "MAP_READ", "MAP_WRITE", "LOAD", "CLEAR",
                       "STORE", "DISCARD", "TEXTURE_DIMENSION2D", "TEXTURE_VIEW_DIMENSION2D_ARRAY",
                       "FRONT_FACE_CCW", "FRONT_FACE_CW", "TEXTURE_FORMAT_RGBA8_UNORM_SRGB"):
            self.assertIn(legacy, expected)

    def test_cold_and_incremental_generation_register_constants_once(self) -> None:
        with tempfile.TemporaryDirectory(prefix="urge-constant-generation-", dir=ROOT) as directory:
            work = Path(directory)
            ir = work / "api.json"
            out = work / "autogen"
            self.assertTrue(API["write_document"](str(ir), self.document))
            original_ir = ir.read_bytes()
            original_mtime = ir.stat().st_mtime_ns
            self.assertFalse(API["write_document"](str(ir), API["build_document"]()))
            self.assertEqual(ir.read_bytes(), original_ir)
            self.assertEqual(ir.stat().st_mtime_ns, original_mtime)

            def generate() -> None:
                result = subprocess.run(
                    [sys.executable, "-I", str(ROOT / "bind/generate_binding.py"), str(ir), str(out)],
                    cwd=ROOT, capture_output=True, text=True, encoding="utf-8", timeout=60,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

            generate()
            for name, entry in self.by_name.items():
                source = out / ("binding_" + GLUE["snake"](name) + ".cc")
                text = source.read_text(encoding="utf-8")
                actual = re.findall(r'rb_const_set\(\w+, rb_intern\("([^"]+)"\)', text)
                self.assertEqual(sorted(actual), sorted(c["name"] for c in entry["constants"]), name)
                self.assertEqual(len(actual), len(set(actual)), name)
                for constant in entry["constants"]:
                    self.assertIn(f'WrapConstant(urge::{name}::{constant["cpp_name"]})', text)

            source = out / "binding_network_event.cc"
            text = source.read_text(encoding="utf-8")
            text = text.replace(GLUE["HANDWRITTEN_END"], "// retained custom data glue\n" + GLUE["HANDWRITTEN_END"])
            source.write_text(text, encoding="utf-8", newline="\r\n")
            before = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in out.iterdir()}
            generate()
            after = {p.name: (p.read_bytes(), p.stat().st_mtime_ns) for p in out.iterdir()}
            self.assertEqual(after, before)
            self.assertIn("// retained custom data glue", source.read_text(encoding="utf-8"))
            self.assertIn('DefineMethod(klass, "data", NetworkEvent_GetData);', source.read_text(encoding="utf-8"))


if __name__ == "__main__":
    if sys.argv[1:] == ["--ruby-checks"]:
        sys.stdout.write(ruby_checks(API["build_document"]()))
    else:
        unittest.main(verbosity=2)

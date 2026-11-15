#!/usr/bin/env python3
"""Regress cold binding seeds without reading existing autogen blocks or building.

Run: python -I test/gc-integration/generation_test.py
Read current headers into an independent IR snapshot, then copy it per test.
All outputs use disposable directories under build/; never rewrite the user's IR.
"""

from __future__ import annotations

import json
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "bind" / "generate_binding.py"
BINDGEN = runpy.run_path(str(GENERATOR))
BLOCKS = (
    (BINDGEN["HANDWRITTEN_INCLUDES_BEGIN"], BINDGEN["HANDWRITTEN_INCLUDES_END"]),
    (BINDGEN["HANDWRITTEN_BEGIN"], BINDGEN["HANDWRITTEN_END"]),
    (BINDGEN["HANDWRITTEN_INIT_BEGIN"], BINDGEN["HANDWRITTEN_INIT_END"]),
)
SEEDED_FILES = sorted(
    set(BINDGEN["HANDWRITTEN_SEED"])
    | {"binding_" + BINDGEN["snake"](name) + ".cc"
       for name in BINDGEN["HANDWRITTEN_INIT_SEED"]}
    | {"binding_init.cc"}
)

# Independent inventory of public glue found in the workspace's HANDWRITTEN
# blocks. Never load autogen as an input: incremental preservation masked the
# missing cold APIs before these assertions existed.
PUBLIC_GLUE = {
    "disposable": (("dispose", "Disposable_DisposeCompat"),
                   ("disposed?", "Disposable_IsDisposedCompat")),
    "sprite": (("bitmap", "Sprite_BitmapCompat"),),
    "tilemap_xp": (("[]", "TilemapAutotile_Get"),
                   ("[]=", "TilemapAutotile_Set"),
                   ("autotiles", "TilemapXP_Autotiles")),
    "tilemap_vx": (("[]", "TilemapBitmaps_Get"),
                   ("[]=", "TilemapBitmaps_Set"),
                   ("bitmaps", "Tilemap_GetBitmaps")),
    "input": (("press?", "Input_PressedManual"),
              ("trigger?", "Input_TriggeredManual"),
              ("repeat?", "Input_RepeatedManual")),
    "table": (("[]", "Table_Get"), ("[]=", "Table_Set")),
    "node3d": (("children", "Node3D_Children"),),
    "gamepad": (("available_devices", "Gamepad_available_devices"),),
    "video": (("player_info", "Video_PlayerInfo"),),
    "fetch": (("body", "Fetch_Body"),),
    "network_event": (("data", "NetworkEvent_Data"),),
}
PUBLIC_CONSTANTS = {
    "mouse": ("LEFT", "MIDDLE", "RIGHT", "X1", "X2"),
    "network_event": ("OPEN", "TEXT", "BINARY", "CLOSE", "ERROR"),
    "web_socket": ("CONNECTING", "OPEN", "CLOSING", "CLOSED"),
}


class GenerationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        build = ROOT / "build"
        build.mkdir(exist_ok=True)
        cls.ir_temporary = tempfile.TemporaryDirectory(prefix="gc-generation-ir-", dir=build)
        cls.addClassCleanup(cls.ir_temporary.cleanup)
        cls.source_ir = Path(cls.ir_temporary.name) / "api.json"
        # api_reference.json may contain intentionally unregenerated user edits.
        # Read headers once, with an explicit isolated output, never the default IR.
        result = subprocess.run(
            [sys.executable, "-I", str(ROOT / "bind" / "gen_api_json.py"),
             str(cls.source_ir)],
            cwd=ROOT, capture_output=True, text=True, encoding="utf-8", timeout=60,
        )
        if result.returncode != 0:
            raise AssertionError(result.stdout + result.stderr)

    def setUp(self) -> None:
        build = ROOT / "build"
        build.mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(prefix="gc-generation-test-", dir=build)
        self.addCleanup(self.cleanup_directory)
        self.work = Path(self.temporary.name)
        self.ir = self.work / "ir.json"
        shutil.copyfile(self.source_ir, self.ir)
        self.out = self.work / "cold"
        self.out.mkdir()
        self.entries = BINDGEN["Model"](json.loads(self.ir.read_text(encoding="utf-8"))).all_entries()
        self.generate()

    def cleanup_directory(self) -> None:
        # Windows indexing/virus scanners can briefly open newly generated TUs
        # without delete sharing. Retry only sharing violations, never assertions
        # or other I/O errors, and still fail if the directory stays locked.
        for attempt in range(6):
            try:
                self.temporary.cleanup()
                return
            except PermissionError as error:
                if getattr(error, "winerror", None) not in (32, 33) or attempt == 5:
                    raise
                time.sleep(0.1 * (attempt + 1))

    def generate(self) -> None:
        result = subprocess.run(
            [sys.executable, "-I", str(GENERATOR), str(self.ir), str(self.out)],
            cwd=ROOT, capture_output=True, text=True, encoding="utf-8", timeout=60,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def text(self, filename: str) -> str:
        return (self.out / filename).read_text(encoding="utf-8")

    def handwritten(self, filename: str) -> str:
        return BINDGEN["region"](self.text(filename), *BLOCKS[1])

    def snapshot(self) -> dict[str, bytes]:
        return {path.name: path.read_bytes() for path in self.out.iterdir()}

    def patch_blocks(self, filename: str) -> None:
        text = self.text(filename)
        for number, (begin, end) in enumerate(BLOCKS):
            body = BINDGEN["region"](text, begin, end)
            text = BINDGEN["splice"](
                text, begin, end, body + f"\n// retained {filename} block {number}\n",
            )
        (self.out / filename).write_text(text, encoding="utf-8")

    def test_empty_directory_has_every_entry_and_global_initializer(self) -> None:
        expected = {"binding_init.h", "binding_init.cc"}
        for entry in self.entries:
            stem = "binding_" + BINDGEN["snake"](entry["cpp_name"])
            expected.update((stem + ".h", stem + ".cc"))
            self.assertIn(f'  Init{entry["cpp_name"]}Binding();', self.text("binding_init.cc"))
        self.assertEqual(set(self.snapshot()), expected)
        includes = BINDGEN["region"](self.text("binding_init.cc"), *BLOCKS[0])
        self.assertIn('#include "binding_win32api.h"', includes)
        self.assertIn("  InitWin32APIBinding();", self.text("binding_init.cc"))
        self.assertIn("  InitializeNativeGC();", self.text("binding_init.cc"))

    def test_pointer_codec_distinguishes_gc_objects_from_resources_and_nullable_aliases(self) -> None:
        for cpp_type in ("Node3D*", "urge::Node3D *", "MaybePtr<Node3D>"):
            with self.subTest(cpp_type=cpp_type):
                self.assertEqual(BINDGEN["object_inner"](cpp_type), "Node3D")
        for cpp_type in ("char*", "const char*", "void*", "uint8_t*", "Network*",
                         "ForeignResource*", "std::shared_ptr<Node3D>",
                         "Persistent<Node3D>", "RefPtr<Node3D>"):
            with self.subTest(cpp_type=cpp_type):
                self.assertIsNone(BINDGEN["object_inner"](cpp_type))
        for cpp_type, check in (("Node3D*", "CheckObject"),
                                ("MaybePtr<Node3D>", "CheckMaybeObject")):
            with self.subTest(cpp_type=cpp_type):
                fmt, declarations, targets, checks, argument = BINDGEN["param_plan"](
                    cpp_type, "node",
                )
                self.assertEqual(fmt, "o")
                self.assertEqual(declarations, [("VALUE", "node_val"), ("urge::Node3D*", "node")])
                self.assertEqual(targets, ["node_val"])
                self.assertEqual(checks, [f"node = {check}<urge::Node3D>(node_val, kNode3DDataType);"])
                self.assertEqual(argument, "node")
        # A default value does not turn a required raw pointee into a nullable API.
        plan = BINDGEN["ParamPlan"]([{"name": "node", "type": "Node3D*", "default": "nullptr"}])
        self.assertIn("node = CheckObject<urge::Node3D>(node_val, kNode3DDataType);", plan.checks)
        self.assertIn("RB_GC_GUARD(node_val);", plan.keep_alive)

    def test_native_return_slots_transfer_heap_roots_not_raw_ownership(self) -> None:
        for cpp_type in ("Node3D*", "urge::Node3D*", "MaybePtr<Node3D>"):
            with self.subTest(cpp_type=cpp_type):
                self.assertEqual(BINDGEN["return_slot"](cpp_type),
                                 ["urge::GarbageCollector::Root* result = nullptr;"])
                self.assertEqual(BINDGEN["emit_store"](cpp_type, "GetNode()", "  "), [
                    "  urge::Node3D* value = GetNode();",
                    "  result = new urge::GarbageCollector::Root(value);",
                ])
                self.assertEqual(BINDGEN["emit_convert"](cpp_type, "  "),
                                 ["  return OwnedWrapObject(result, kNode3DDataType);"])
                self.assertEqual(BINDGEN["emit_failed_result_cleanup"](cpp_type, "  "),
                                 ["  DeleteFailedResult(result, exception_state);"])
        constructor = "\n".join(BINDGEN["emit_initialize"]({
            "cpp_name": "Node3D",
            "constructors": [{"params": [], "has_exception": True}],
        }))
        self.assertRegex(constructor, re.compile(
            r"urge::Node3D\* value = urge::MakeGarbageCollected<urge::Node3D>\(\s*"
            r"exception_state\);\s*if \(!exception_state.HadException\(\)\)\s*"
            r"result = new urge::GarbageCollector::Root\(value\);",
        ))
        self.assertLess(constructor.index("RaiseException(exception_state);"),
                        constructor.index("return OwnedSetupSelf(self, result);"))

    def test_generated_ruby_frames_carry_only_pod_root_pointers(self) -> None:
        for path in self.out.glob("*.cc"):
            with self.subTest(filename=path.name):
                generated = BINDGEN["region"](
                    path.read_text(encoding="utf-8"),
                    BINDGEN["GENERATED_BEGIN"], BINDGEN["GENERATED_END"],
                )
                self.assertNotRegex(generated, r"\b(?:Persistent|Member|WeakMember)\s*<")
                self.assertNotRegex(generated, r"\bGarbageCollector::Root\s+\w+\s*[({;=]")
                if "OwnedSetupSelf(self, result" in generated or "OwnedWrapObject(result," in generated:
                    self.assertIn("urge::GarbageCollector::Root* result = nullptr;", generated)
                    self.assertIn("new urge::GarbageCollector::Root(value)", generated)
                for name in (entry["cpp_name"] for entry in self.entries):
                    self.assertNotRegex(generated, r"\burge::" + re.escape(name) + r"\s*\*\s*result\b")

    def assert_safe_aggregate_seeds(self) -> None:
        video = self.handwritten("binding_video.cc")
        gamepad = self.handwritten("binding_gamepad.cc")
        for text in (video, gamepad):
            self.assertIn("CheckArgc(argc, 0);", text)
            self.assertNotRegex(text, r"\bEXC_(BEGIN|END)\b")
            self.assertNotRegex(text, r"\b(?:try|catch)\s*[({]")
            self.assertNotIn("delete result", text)  # cleanup belongs to rb_ensure
        self.assertRegex(video, re.compile(
            r"InfoMap\* result = nullptr;\s*"
            r"\{\s*result = new InfoMap\(self_obj->PlayerInfo\(\)\);\s*\}\s*"
            r"return OwnedConvertBox<InfoMap>\(result, \[\]\(const InfoMap& entries\)",
        ))
        self.assertGreater(video.index("rb_hash_new()"), video.index("return OwnedConvertBox"))
        self.assertNotRegex(video, r"\bInfoMap\s+\w+\s*[;={]")
        self.assertRegex(gamepad, re.compile(
            r"std::vector<urge::Gamepad::DeviceInfo>\* result = nullptr;\s*"
            r"\{\s*result = new std::vector<urge::Gamepad::DeviceInfo>\(\s*"
            r"urge::Gamepad::GetAvailableDevices\(\)\);\s*\}\s*"
            r"return OwnedConvertBox<std::vector<urge::Gamepad::DeviceInfo>>\(\s*"
            r"result, \[\]\(const std::vector<urge::Gamepad::DeviceInfo>& values\)",
        ))
        self.assertNotRegex(gamepad, r"std::vector<urge::Gamepad::DeviceInfo>\s+\w+\s*[;={]")
        self.assertGreater(gamepad.index("rb_ary_new()"), gamepad.index("return OwnedConvertBox"))
        for key in ("id", "name", "path", "player", "guid"):
            self.assertIn(f'rb_str_new2("{key}")', gamepad)
        self.assertIn('DefineMethod(klass, "player_info", Video_PlayerInfo);', video)
        self.assertIn("rb_utf8_encoding()", video)
        self.assertIn("rb_utf8_encoding()", gamepad)

    def test_aggregate_seeds_own_only_heap_boxes_across_ruby_conversion(self) -> None:
        self.assert_safe_aggregate_seeds()

    def test_no_cold_output_uses_retired_helpers_or_unowned_generated_wrapping(self) -> None:
        for path in self.out.glob("*.cc"):
            with self.subTest(filename=path.name):
                text = path.read_text(encoding="utf-8")
                self.assertNotRegex(text, (
                    r"\b(?:EXC_BEGIN|EXC_END|RefPtr|RefCounted|MakeRefCounted|"
                    r"AddRef|Release|GetVectorOf)\b"
                ))
                self.assertNotRegex(text, r"\bCheckString\([^;]*,\s*&")
                generated = BINDGEN["region"](
                    text, BINDGEN["GENERATED_BEGIN"], BINDGEN["GENERATED_END"],
                )
                self.assertNotRegex(generated, r"\b(?:SetupSelfData|WrapObject)\s*\(")

    def test_node3d_snapshot_and_native_lifecycle_survive_cold_generation(self) -> None:
        node = self.handwritten("binding_node3d.cc")
        self.assertIn("using ChildrenSnapshot = std::vector<urge::GarbageCollector::Root>;", node)
        self.assertRegex(node, re.compile(
            r"ChildrenSnapshot\* children = nullptr;\s*\{\s*"
            r"const auto snapshot = node->NativeChildrenSnapshot\(\);\s*"
            r"ChildrenSnapshot roots;\s*roots.reserve\(snapshot.size\(\)\);\s*"
            r"for \(auto\* child : snapshot\)\s*roots.emplace_back\(child\);\s*"
            r"children = new ChildrenSnapshot\(std::move\(roots\)\);\s*\}",
        ))
        self.assertIn("WrapObject((*children)[i].get(), kNode3DDataType)", node)
        self.assertIn("delete reinterpret_cast<ChildrenSnapshot*>(opaque);", node)
        self.assertIn("rb_ensure(ConvertNode3DChildren, opaque, ReleaseNode3DChildren, opaque)", node)
        # The stack vector exists only in the pure C++ scope, never across Ruby.
        native_scope = node.split("  {\n", 1)[1].split("\n  }", 1)[0]
        self.assertNotRegex(native_scope, r"\b(?:rb_\w+|WrapObject|OwnedWrapObject)\s*\(")
        self.assertIn('DefineMethod(klass, "children", Node3D_Children);', self.text("binding_node3d.cc"))
        for name in ("Node3D", "Gamepad", "Video", "TilemapXP", "TilemapVX", "Sprite"):
            with self.subTest(name=name):
                text = self.text("binding_" + BINDGEN["snake"](name) + ".cc")
                self.assertIn(f"RegisterNativeType<urge::{name}>(klass, k{name}DataType);", text)
                self.assertIn(f"CheckConstructorSelf(self, k{name}DataType);", text)
                self.assertIn("return OwnedSetupSelf(self, result);", text)
                self.assertIn('DefineMethod(klass, "initialize_copy", RejectNativeCopy);', text)
        for name in ("Sprite", "TilemapXP", "TilemapVX"):
            text = self.text("binding_" + BINDGEN["snake"](name) + ".cc")
            self.assertRegex(text, (
                r"BINDING_ATTR_OBJECT_REF\(" + name + r",\s*urge::" + name
                + r",\s*Viewport,\s*urge::Viewport,\s*kViewportDataType,\s*CheckMaybeObject\);"
            ))

    def assert_public_compatibility_seeds(self) -> None:
        for stem, methods in PUBLIC_GLUE.items():
            text = self.text("binding_" + stem + ".cc")
            for ruby_name, function in methods:
                with self.subTest(stem=stem, method=ruby_name):
                    self.assertIn(f"RB_FUNC({function})", text)
                    self.assertRegex(text, (
                        r'Define(?:Method|ClassMethod|ModuleFunction)\(\w+,\s*"'
                        + re.escape(ruby_name) + r'",\s*' + function + r'\);'
                    ))
        for stem, constants in PUBLIC_CONSTANTS.items():
            text = self.text("binding_" + stem + ".cc")
            for name in constants:
                with self.subTest(stem=stem, constant=name):
                    self.assertIn(f'rb_intern("{name}")', text)
        self.assertIn("urge::kKeyboardBindings", self.handwritten("binding_input.cc"))
        self.assertIn("DefineInputKeyConstants(mod);", self.text("binding_input.cc"))

        disposable = self.handwritten("binding_disposable.cc")
        for name in ("Disposable_DisposeCompat", "Disposable_IsDisposedCompat"):
            self.assertRegex(disposable, (
                r"RB_FUNC\(" + name + r"\) \{\s*CheckArgc\(argc, 0\);\s*"
                r"if \(!RTYPEDDATA_DATA\(self\) && IsLegacyDisposable\(self\)\)"
            ))
        self.assertEqual(disposable.count("GetSelfData<urge::Disposable, true>(self)"), 2)
        sprite = self.handwritten("binding_sprite.cc")
        self.assertRegex(sprite, r"if \(!RTYPEDDATA_DATA\(self\)\)\s*return Qnil;")
        self.assertIn("GetSelfData<urge::Sprite, true>(self)", sprite)
        self.assertIn("urge::GarbageCollector::Root* result = nullptr;", sprite)
        self.assertIn("urge::Bitmap* bitmap = *sprite->Attr_Bitmap();", sprite)
        self.assertIn("result = new urge::GarbageCollector::Root(bitmap);", sprite)
        self.assertNotRegex(sprite, r"\b(?:RefPtr|AddRef|Release)\b")
        self.assertIn("return OwnedWrapObject(result, kBitmapDataType);", sprite)
        self.assertNotIn("rb_iv_set", sprite)
        for stem, count, proxy in (("tilemap_xp", 7, "TilemapAutotileArray"),
                                   ("tilemap_vx", 9, "TilemapBitmapArray")):
            text = self.text("binding_" + stem + ".cc")
            body = self.handwritten("binding_" + stem + ".cc")
            self.assertIn(f'rb_define_class("{proxy}", rb_cObject)', text)
            self.assertIn('rb_iv_set(obj, "@parent", parent);', body)
            self.assertIn('rb_iv_set(parent, "_TilemapSlots", obj);', body)
            self.assertEqual(body.count(f"if (index < 0 || index >= {count})"), 2)
            self.assertIn("urge::GarbageCollector::Root* result = nullptr;", body)
            getter = "GetAutotile" if stem == "tilemap_xp" else "GetBitmap"
            self.assertIn(f"urge::Bitmap* bitmap = parent->{getter}(index);", body)
            self.assertIn("result = new urge::GarbageCollector::Root(bitmap);", body)
            self.assertNotRegex(body, r"\b(?:RefPtr|AddRef|Release)\b")
            self.assertIn("return OwnedWrapObject(result, kBitmapDataType);", body)
            self.assertIn("return bitmap_val;", body)
            self.assertNotRegex(body, r'rb_iv_(?:get|set)\([^;]*"@values"')
            self.assertNotIn("rb_ary_new", body)
        input_body = self.handwritten("binding_input.cc")
        for check in ("SYMBOL_P(value)", "RB_TYPE_P(value, T_STRING)", "FIXNUM_P(value)"):
            self.assertIn(check, input_body)
        self.assertNotRegex(input_body, r"std::string\s+\w+\s*[;={]")

    def test_cold_output_contains_every_handwritten_public_api(self) -> None:
        self.assert_public_compatibility_seeds()

    def test_existing_and_recreated_files_keep_the_cold_public_contract(self) -> None:
        for filename in SEEDED_FILES:
            self.patch_blocks(filename)
        self.generate()
        self.assert_public_compatibility_seeds()
        for filename in SEEDED_FILES:
            (self.out / filename).unlink()
            self.generate()
            self.assert_public_compatibility_seeds()
        shutil.rmtree(self.out)
        self.generate()
        self.assert_public_compatibility_seeds()

    def test_empty_append_blocks_restore_the_public_compatibility_hooks(self) -> None:
        for name, seed in BINDGEN["HANDWRITTEN_INIT_SEED"].items():
            filename = "binding_" + BINDGEN["snake"](name) + ".cc"
            text = BINDGEN["splice"](self.text(filename), *BLOCKS[2], "")
            (self.out / filename).write_text(text, encoding="utf-8")
        self.generate()
        for name, seed in BINDGEN["HANDWRITTEN_INIT_SEED"].items():
            with self.subTest(name=name):
                filename = "binding_" + BINDGEN["snake"](name) + ".cc"
                self.assertEqual(BINDGEN["region"](self.text(filename), *BLOCKS[2]), seed.strip("\n"))
        self.assert_public_compatibility_seeds()

    def test_existing_manual_table_and_binary_accessor_contracts_are_seeded(self) -> None:
        table = self.handwritten("binding_table.cc")
        getter = table.split("RB_FUNC(Table_Set)")[0]
        self.assertNotIn("return Qnil;", getter)
        self.assertIn("return INT2NUM(result);", getter)
        self.assertIn("result = 0;", getter)
        self.assertIn("self_obj->Set(static_cast<int16_t>(value), x, y, z);", table)
        for stem, name in (("fetch", "Fetch_Body"), ("network_event", "NetworkEvent_Data")):
            body = self.handwritten("binding_" + stem + ".cc")
            self.assertRegex(body, re.compile(
                r"RB_FUNC\(" + name + r"\).*?CheckArgc\(argc, 0\);", re.DOTALL,
            ))
            self.assertIn("rb_ascii8bit_encoding()", body)
            self.assertIn("const char*", body)

    def test_incremental_generation_preserves_all_handwritten_blocks(self) -> None:
        files = SEEDED_FILES
        for filename in files:
            self.patch_blocks(filename)
        expected = {
            filename: [BINDGEN["region"](self.text(filename), *block) for block in BLOCKS]
            for filename in files
        }
        self.generate()
        for filename in files:
            with self.subTest(filename=filename):
                self.assertEqual(
                    [BINDGEN["region"](self.text(filename), *block) for block in BLOCKS],
                    expected[filename],
                )

    def test_deleting_one_output_restores_seed_not_previous_handwritten_blocks(self) -> None:
        self.patch_blocks("binding_rect.cc")
        self.generate()
        untouched = (self.out / "binding_rect.cc").read_bytes()
        files = SEEDED_FILES
        for filename in files:
            with self.subTest(filename=filename):
                path = self.out / filename
                cold = path.read_bytes()
                self.patch_blocks(filename)
                self.generate()
                self.assertIn(f"// retained {filename} block 1", self.text(filename))
                path.unlink()  # force seeds; preserved blocks cannot mask a stale template
                self.generate()
                self.assertEqual(path.read_bytes(), cold)
                self.assertNotRegex(
                    self.text(filename), (
                        r"\b(?:EXC_BEGIN|EXC_END|RefPtr|RefCounted|MakeRefCounted|"
                        r"AddRef|Release|GetVectorOf)\b"
                    ),
                )
                if filename in ("binding_gamepad.cc", "binding_video.cc"):
                    self.assert_safe_aggregate_seeds()
                if filename == "binding_init.cc":
                    self.assertIn('#include "binding_win32api.h"', self.text(filename))
                self.assertEqual((self.out / "binding_rect.cc").read_bytes(), untouched)
        for filename in ("binding_gamepad.h", "binding_video.h", "binding_node3d.h"):
            with self.subTest(filename=filename):
                path = self.out / filename
                cold = path.read_bytes()
                path.unlink()
                self.generate()
                self.assertEqual(path.read_bytes(), cold)

    def test_deleting_the_whole_directory_restores_identical_cold_outputs(self) -> None:
        expected = self.snapshot()
        shutil.rmtree(self.out)
        self.generate()
        self.assertEqual(self.snapshot(), expected)

    def test_regeneration_is_byte_and_timestamp_idempotent(self) -> None:
        expected = self.snapshot()
        timestamps = {path.name: path.stat().st_mtime_ns for path in self.out.iterdir()}
        self.generate()
        self.assertEqual(self.snapshot(), expected)
        self.assertEqual(
            {path.name: path.stat().st_mtime_ns for path in self.out.iterdir()}, timestamps,
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)

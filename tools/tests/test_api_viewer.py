#!/usr/bin/env python3
"""Backend-free API viewer regressions. Run: python -I tools/tests/test_api_viewer.py"""

from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import runpy
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
VIEWER = runpy.run_path(str(ROOT / "bind/viewer/viewer.py"))


def fixture() -> dict:
    return {
        "classes": [{
            "name": "Socket", "kind": "class", "cpp_name": "Socket",
            "constants": [
                {"name": "OPEN", "cpp_name": "State::Open", "value": "0"},
                {"name": "NEXT", "cpp_name": "State::Next", "value": None},
            ],
            "instance_methods": [{"ruby_name": "read", "cpp_name": "Read", "params": []}],
        }],
        "modules": [{
            "name": "GPU", "kind": "module",
            "constants": [{
                "name": "MASK", "cpp_name": "Mask", "value": "1ULL << 63",
                "doc": "Wide unsigned mask",
            }],
            "functions": [{"ruby_name": "read", "cpp_name": "Read", "params": []}],
        }],
    }


class ViewerConstantTests(unittest.TestCase):
    def test_constants_in_classes_and_modules_use_ruby_lookup(self) -> None:
        ir = fixture()
        for target in ("Socket::OPEN", "GPU::MASK"):
            with self.subTest(target=target):
                entry, member = VIEWER["resolve"](ir, target)
                self.assertEqual(VIEWER["format_signature"](member), target)
                self.assertEqual(member["kind"], "constant")
                self.assertEqual(VIEWER["return_type"](member), "Integer")
                self.assertIsNone(member["writer"])
                self.assertEqual(VIEWER["iter_members"](entry)[0]["kind"], "constant")
        self.assertIsNone(VIEWER["resolve"](ir, "Socket.OPEN")[1])
        self.assertIsNone(VIEWER["resolve"](ir, "Socket#OPEN")[1])
        self.assertIsNone(VIEWER["resolve"](ir, "Socket::read")[1])
        self.assertIsNone(VIEWER["resolve"](ir, "Socket::OPEN=")[1])
        self.assertEqual(VIEWER["resolve"](ir, "Socket#read")[1]["kind"], "instance")
        self.assertEqual(VIEWER["resolve"](ir, "GPU.read")[1]["kind"], "module_function")

    def test_values_remain_source_text_including_zero_and_implicit_enums(self) -> None:
        ir = fixture()
        for target, expected in (
            ("Socket::OPEN", "0"),
            ("Socket::NEXT", "implicit (see C++ member)"),
            ("GPU::MASK", "1ULL << 63"),
        ):
            with self.subTest(target=target):
                entry, member = VIEWER["resolve"](ir, target)
                self.assertEqual(VIEWER["constant_value"](member), expected)
                self.assertIn("value     " + expected, VIEWER["render_member"](entry, member))
                self.assertIn("initializer: " + expected, VIEWER["render_owner"](entry))
        self.assertIn("Wide unsigned mask", VIEWER["render_member"](*VIEWER["resolve"](ir, "GPU::MASK")))

    def test_search_supports_qualified_names_types_initializers_and_docs(self) -> None:
        ir = fixture()
        for query, expected in (
            ("Socket::OPEN", "Socket::OPEN"),
            ("State::Next", "Socket::NEXT"),
            ("1ULL << 63", "GPU::MASK"),
            ("Wide unsigned", "GPU::MASK"),
        ):
            with self.subTest(query=query):
                self.assertEqual(VIEWER["search"](ir, query)[0]["call"], expected)
        self.assertEqual(len(VIEWER["search"](ir, "Integer")), 3)

    def test_binary_payload_getters_are_visible_and_described(self) -> None:
        ir = VIEWER["load_ir"]()
        for target in ("NetworkEvent#data", "Fetch#body"):
            with self.subTest(target=target):
                entry, member = VIEWER["resolve"](ir, target)
                self.assertIsNotNone(member)
                self.assertIn("binary", member["flags"])
                self.assertIn("ASCII-8BIT", VIEWER["render_member"](entry, member))
                self.assertEqual(VIEWER["return_type"](member), "String")
        self.assertTrue({"NetworkEvent#data", "Fetch#body"} <=
                        {m["call"] for m in VIEWER["search"](ir, "ASCII-8BIT")})
        self.assertIsNone(VIEWER["resolve"](ir, "Fetch#text")[1])
        self.assertIsNone(VIEWER["resolve"](ir, "WebSocket#send_text")[1])
        self.assertEqual(VIEWER["resolve"](ir, "WebSocket#send")[1]["cpp_name"], "Send")

    def test_old_ir_without_constants_keeps_methods(self) -> None:
        ir = fixture()
        for entry in ir["classes"] + ir["modules"]:
            del entry["constants"]
            self.assertEqual(len(VIEWER["iter_members"](entry)), 1)
        self.assertEqual(VIEWER["search"](ir, "read")[0]["ruby_name"], "read")

    def test_cli_show_search_stats_and_unknown_constant(self) -> None:
        with tempfile.TemporaryDirectory(prefix="urge-viewer-") as directory:
            path = Path(directory) / "api.json"
            path.write_text(json.dumps(fixture()), encoding="utf-8")

            def command(*args: str) -> tuple[int, str]:
                output = io.StringIO()
                with redirect_stdout(output):
                    code = VIEWER["main"](["--ir", str(path), *args])
                return code, output.getvalue()

            code, output = command("show", "GPU::MASK", "--json")
            self.assertEqual(code, 0)
            self.assertEqual(json.loads(output)["member"]["value"], "1ULL << 63")
            code, output = command("show", "GPU::MISSING")
            self.assertEqual(code, 1)
            self.assertIn("no member matched", output)
            code, output = command("search", "GPU::MASK")
            self.assertEqual(code, 0)
            self.assertIn("initializer: 1ULL << 63", output)
            code, output = command("stats", "--json")
            self.assertEqual(code, 0)
            self.assertEqual(json.loads(output)["by_kind"]["constant"], 3)

    def test_project_ir_constant_inventory_is_visible(self) -> None:
        ir = VIEWER["load_ir"]()
        expected = sum(len(entry.get("constants", [])) for entry in VIEWER["owners"](ir))
        actual = sum(member["kind"] == "constant" for entry in VIEWER["owners"](ir)
                     for member in VIEWER["iter_members"](entry))
        self.assertGreater(expected, 0)
        self.assertEqual(actual, expected)
        self.assertEqual(VIEWER["resolve"](ir, "Input::DOWN")[1]["value"], "2")
        self.assertEqual(VIEWER["resolve"](ir, "WebSocket::OPEN")[1]["value"], "1")


if __name__ == "__main__":
    unittest.main(verbosity=2)

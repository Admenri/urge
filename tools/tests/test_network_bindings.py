#!/usr/bin/env python3
"""Engine-free regressions for the all-binary network binding/IR contract.

Run: python -I tools/tests/test_network_bindings.py
The real headers and checked-in IR are read only; cold/incremental glue is
written into temporary directories. Network/VM integration belongs to the
separate network probes, not this generator suite.
"""

from __future__ import annotations

import copy
import json
from pathlib import Path
import re
import runpy
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
HEADER = "core/network/network.h"
API = runpy.run_path(str(ROOT / "bind/gen_api_json.py"))
GLUE = runpy.run_path(str(ROOT / "bind/generate_binding.py"))
RETIRED = r"\b(?:NetworkEvent_Data|DefineNetworkEventData|Fetch_Body|DefineFetchBody|GetText|SendText|SendBinary)\b"


def network_document(document: dict) -> dict:
    return {group: [entry for entry in document[group] if entry["header"] == HEADER]
            for group in ("classes", "modules")}


def old_document(document: dict) -> dict:
    """Pre-migration IR, independent of the generator's migration fingerprints."""
    old = copy.deepcopy(document)
    for entry in old["classes"]:
        for method in entry["instance_methods"]:
            if method["cpp_name"] in ("GetData", "GetBody"):
                method.update(cpp_name="GetText", ruby_name="text")
                method.pop("binary")
        if entry["cpp_name"] == "NetworkEvent":
            entry["constants"] = [
                {"name": name.upper(), "cpp_name": name, "value": str(value)}
                for value, name in enumerate(("Open", "Text", "Binary", "Close", "Error"))
            ]
        if entry["cpp_name"] == "WebSocket":
            send = next(m for m in entry["instance_methods"] if m["cpp_name"] == "Send")
            index = entry["instance_methods"].index(send)
            entry["instance_methods"][index:index + 1] = [
                dict(send, cpp_name="SendText", ruby_name="send_text"),
                dict(send, cpp_name="SendBinary", ruby_name="send_binary"),
            ]
    return old


def legacy_glue(owner: str, borrowed: bool, checked: bool) -> str:
    """The original seed and the later pointer/length helper seen in autogen."""
    member = "data" if owner == "NetworkEvent" else "body"
    symbol = "NetworkEvent_Data" if owner == "NetworkEvent" else "Fetch_Body"
    helper = "DefineNetworkEventData" if owner == "NetworkEvent" else "DefineFetchBody"
    if owner == "NetworkEvent":
        comment = ("// `data()` answers a `const std::string&` the object owns, so phase 2 can hand\n"
                   "// back a pointer and a length without copying or owning anything.") if borrowed else (
                   "// `NetworkEvent#data` answers a Ruby String tagged ASCII-8BIT: the payload is\n"
                   "// raw bytes and the generated `s` codec would tag it UTF-8.  The C++ accessor\n"
                   "// therefore carries no `URGE_BINDING()` marker.")
    else:
        comment = ("// Same shape as NetworkEvent#data: the body belongs to the Fetch object, so\n"
                   "// phase 2 only reads a pointer and a length out of it.") if borrowed else (
                   "// `Fetch#body` answers ASCII-8BIT bytes, for the same reason as\n"
                   "// `NetworkEvent#data`.")
    check = "  CheckArgc(argc, 0);\n" if checked else ""
    if borrowed:
        body = f"""
  const char* {member} = nullptr;
  size_t size = 0;

  {{
    const std::string& payload = self_obj->{member}();
    {member} = payload.data();
    size = payload.size();
  }}

  return rb_enc_str_new({member}, static_cast<long>(size),
                        rb_ascii8bit_encoding());
"""
    else:
        body = f"""
  const std::string& {member} = self_obj->{member}();
  return rb_enc_str_new({member}.data(), static_cast<long>({member}.size()),
                        rb_ascii8bit_encoding());
"""
    return f"""{comment}
RB_FUNC({symbol}) {{
  auto* self_obj = GetSelfData<urge::{owner}>(self);
{check}{body}}}

void {helper}(VALUE klass) {{
  DefineMethod(klass, "{member}", {symbol});
}}
"""


def replace_region(path: Path, name: str, body: str) -> None:
    text = path.read_text(encoding="utf-8")
    text = GLUE["splice"](text, GLUE[name + "_BEGIN"], GLUE[name + "_END"],
                          body.rstrip("\n") + "\n" if body else "")
    path.write_text(text, encoding="utf-8", newline="\r\n")


def function(text: str, symbol: str) -> str:
    match = re.search(r"^RB_FUNC\(" + re.escape(symbol) + r"\) \{.*?^\}", text, re.M | re.S)
    if not match:
        raise AssertionError("missing generated function " + symbol)
    return match.group()


def snapshot(directory: Path) -> dict:
    return {path.name: (path.read_bytes(), path.stat().st_mtime_ns)
            for path in directory.glob("binding_*")}


class NetworkBindingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.document = network_document(API["build_document"]())
        cls.entries = {e["cpp_name"]: e for entries in cls.document.values() for e in entries}

    def generate(self, document: dict, directory: Path) -> None:
        ir = directory.parent / (directory.name + ".json")
        ir.write_text(json.dumps(document), encoding="utf-8")
        result = subprocess.run([sys.executable, "-I", str(ROOT / "bind/generate_binding.py"),
                                 str(ir), str(directory)], cwd=ROOT, capture_output=True,
                                text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def assert_surface(self, directory: Path) -> None:
        for owner, entry in self.entries.items():
            path = directory / ("binding_" + GLUE["snake"](owner) + ".cc")
            text = path.read_text(encoding="utf-8")
            methods = entry.get("instance_methods", entry.get("functions", []))
            registered = re.findall(r'Define(?:Method|ModuleFunction)\(\w+, "([^"]+)"', text)
            expected = [m["ruby_name"] for m in methods]
            if entry.get("constructors"):
                expected.append("initialize")
            self.assertCountEqual(registered, expected)
            self.assertCountEqual(re.findall(r'rb_const_set\(\w+, rb_intern\("([^"]+)"\)', text),
                                  [c["name"] for c in entry["constants"]])
            self.assertNotRegex(text, RETIRED)
            self.assertNotRegex(text, r'"(?:text|send_text|send_binary|TEXT|BINARY)"')

    def test_checked_in_network_ir_matches_real_headers(self) -> None:
        checked = json.loads((ROOT / "bind/api_reference.json").read_text(encoding="utf-8"))
        self.assertEqual(network_document(checked), self.document)
        self.assertEqual(set(self.entries), {"NetworkEvent", "WebSocket", "Fetch", "Network"})
        event = self.entries["NetworkEvent"]
        self.assertEqual([(c["name"], c["value"]) for c in event["constants"]],
                         [("OPEN", "0"), ("MESSAGE", "1"), ("CLOSE", "2"), ("ERROR", "3")])
        for owner, name in (("NetworkEvent", "data"), ("Fetch", "body")):
            method = next(m for m in self.entries[owner]["instance_methods"] if m["ruby_name"] == name)
            self.assertTrue(method["binary"])
            self.assertEqual(method["params"], [])
            self.assertEqual(method["return"], {"cpp_type": "std::string", "ruby_type": "String"})
        for entry in self.entries.values():
            self.assertFalse(entry["unsupported"])

    def test_cold_generation_and_no_write_idempotence(self) -> None:
        with tempfile.TemporaryDirectory(prefix="urge-network-binding-", dir=ROOT) as temporary:
            out = Path(temporary, "cold")
            self.generate(self.document, out)
            self.assert_surface(out)
            before = snapshot(out)
            self.generate(self.document, out)
            self.assertEqual(snapshot(out), before)
        for name in ("binding_network_event.cc", "binding_fetch.cc"):
            self.assertNotIn(name, GLUE["HANDWRITTEN_SEED"])
        for name in ("NetworkEvent", "Fetch"):
            self.assertNotIn(name, GLUE["EXTRA_INIT_CALLS"])
        # Unrelated overrides must not be removed with the network helpers.
        self.assertIn("binding_video.cc", GLUE["HANDWRITTEN_SEED"])
        self.assertIn("Video", GLUE["EXTRA_INIT_CALLS"])

    def test_binary_getters_text_metadata_and_exact_argument_counts(self) -> None:
        model = GLUE["Model"](self.document)
        texts = {name: "\n".join(GLUE["emit_module"](entry) if entry["kind"] == "module"
                                else GLUE["emit_class"](model, entry))
                 for name, entry in self.entries.items()}
        for owner, getter in (("NetworkEvent", "GetData"), ("Fetch", "GetBody")):
            body = function(texts[owner], owner + "_" + getter)
            self.assertIn("CheckArgc(argc, 0);", body)
            self.assertIn("result = new std::string(self_obj->" + getter + "());", body)
            self.assertIn("result->data(), static_cast<long>(result->size())", body)
            self.assertIn("rb_ascii8bit_encoding()", body)
            self.assertNotIn("rb_utf8_encoding()", body)
            self.assertIn("delete result;", body)
        metadata = {"NetworkEvent": ("GetReason", "GetErrorMessage"),
                    "WebSocket": ("GetUrl", "GetError"),
                    "Fetch": ("GetStatusText", "GetError", "GetHeader")}
        for owner, getters in metadata.items():
            for getter in getters:
                body = function(texts[owner], owner + "_" + getter)
                self.assertIn("rb_utf8_encoding()", body)
                self.assertNotIn("rb_ascii8bit_encoding()", body)
        self.assertIn("WrapVectorOf<std::string>(*result)", function(texts["Fetch"], "Fetch_GetHeaders"))
        send = function(texts["WebSocket"], "WebSocket_Send")
        self.assertIn("CheckArgc(argc, 1);", send)
        self.assertIn('ParseArgs(argc, argv, "o", &data_val);', send)
        self.assertIn("CheckString(data_val, &data_data, &data_size);", send)
        self.assertIn("self_obj->Send(std::string(data_data, data_size));", send)
        self.assertNotIn("encoding", send)
        self.assertIn("CheckArgc(argc, 1);", function(texts["Fetch"], "Fetch_GetHeader"))
        arities = {"WebSocket": {"initialize": [1, 2], "SendPing": [0, 1], "Close": [0, 1, 2]},
                   "Fetch": {"initialize": [1, 2, 3, 4]},
                   "Network": {"OpenWebSocket": [1, 2], "OpenFetch": [1, 2, 3, 4]}}
        for owner, methods in arities.items():
            for method, expected in methods.items():
                body = function(texts[owner], owner + "_" + method)
                self.assertEqual([int(n) for n in re.findall(r"argc == (\d+)", body)], expected)
                self.assertIn("ArgcError(argc);", body)
                self.assertNotIn("argc >=", body)
                self.assertNotIn("argv[", body)
                self.assertNotRegex(body, r'ParseArgs\([^;]*"[^"\n]*\|')

    def test_incremental_known_helpers_converge_to_cold_output(self) -> None:
        with tempfile.TemporaryDirectory(prefix="urge-network-migration-", dir=ROOT) as temporary:
            root = Path(temporary)
            cold = root / "cold"
            self.generate(self.document, cold)
            cold_bytes = {name: state[0] for name, state in snapshot(cold).items()}
            for borrowed in (False, True):
                for checked in (False, True):
                    with self.subTest(borrowed=borrowed, checked=checked):
                        out = root / f"old-{int(borrowed)}-{int(checked)}"
                        self.generate(old_document(self.document), out)
                        for owner in ("NetworkEvent", "Fetch"):
                            path = out / ("binding_" + GLUE["snake"](owner) + ".cc")
                            replace_region(path, "HANDWRITTEN", legacy_glue(owner, borrowed, checked))
                        self.generate(self.document, out)
                        self.assert_surface(out)
                        self.assertEqual({name: state[0] for name, state in snapshot(out).items()}, cold_bytes)
                        before = snapshot(out)
                        self.generate(self.document, out)
                        self.assertEqual(snapshot(out), before)

    def test_migration_preserves_unrelated_handwritten_regions_and_old_tail_format(self) -> None:
        with tempfile.TemporaryDirectory(prefix="urge-network-custom-", dir=ROOT) as temporary:
            out = Path(temporary, "glue")
            self.generate(old_document(self.document), out)
            includes = '#include "custom_network_patch.h"\n// retained include comment'
            before = 'void BeforePatch() { /* untouched */ }\n'
            after = '\nvoid AfterPatch() { const char* text = "body()"; (void)text; }'
            for owner in ("NetworkEvent", "Fetch"):
                path = out / ("binding_" + GLUE["snake"](owner) + ".cc")
                replace_region(path, "HANDWRITTEN_INCLUDES", includes)
                replace_region(path, "HANDWRITTEN", before + legacy_glue(owner, True, True) + after)
                helper = "DefineNetworkEventData" if owner == "NetworkEvent" else "DefineFetchBody"
                symbol = "NetworkEvent_Data" if owner == "NetworkEvent" else "Fetch_Body"
                member = "data" if owner == "NetworkEvent" else "body"
                tail = (f"void Init{owner}BindingAppend(VALUE klass) {{\n"
                        "  BeforePatch();\n" + f"  {helper}(klass);\n"
                        f'  DefineMethod(klass, "{member}", {symbol});\n'
                        "  AfterPatch();\n}")
                replace_region(path, "HANDWRITTEN_INIT", tail)
            # Older generators had no tail markers at all.
            ws = out / "binding_web_socket.cc"
            text = ws.read_text(encoding="utf-8")
            begin, end = GLUE["HANDWRITTEN_INIT_BEGIN"], GLUE["HANDWRITTEN_INIT_END"]
            text = text[:text.index(begin)] + text[text.index(end) + len(end):]
            ws.write_text(text, encoding="utf-8", newline="\r\n")
            self.generate(self.document, out)
            for owner in ("NetworkEvent", "Fetch"):
                text = (out / ("binding_" + GLUE["snake"](owner) + ".cc")).read_text(encoding="utf-8")
                self.assertEqual(GLUE["region"](text, GLUE["HANDWRITTEN_INCLUDES_BEGIN"],
                                               GLUE["HANDWRITTEN_INCLUDES_END"]), includes)
                hand = GLUE["region"](text, GLUE["HANDWRITTEN_BEGIN"], GLUE["HANDWRITTEN_END"])
                self.assertIn(before.rstrip("\n"), hand)
                self.assertIn(after.lstrip("\n"), hand)
                self.assertNotRegex(hand, RETIRED)
                self.assertEqual(GLUE["region"](text, GLUE["HANDWRITTEN_INIT_BEGIN"],
                                               GLUE["HANDWRITTEN_INIT_END"]),
                                 f"void Init{owner}BindingAppend(VALUE klass) {{\n"
                                 "  BeforePatch();\n  AfterPatch();\n}")
            self.assertIn("void InitWebSocketBindingAppend(VALUE klass)", ws.read_text(encoding="utf-8"))
            before_run = snapshot(out)
            self.generate(self.document, out)
            self.assertEqual(snapshot(out), before_run)

    def test_customized_legacy_helper_is_rejected_without_rewriting_file(self) -> None:
        model = GLUE["Model"](self.document)
        with tempfile.TemporaryDirectory(prefix="urge-network-unsafe-", dir=ROOT) as temporary:
            for owner in ("NetworkEvent", "Fetch"):
                entry = self.entries[owner]
                path = Path(temporary, "binding_" + GLUE["snake"](owner) + ".cc")
                GLUE["generate_cc"](model, entry, str(path))
                source = legacy_glue(owner, True, True)
                variants = [source.replace("rb_ascii8bit_encoding()", "rb_utf8_encoding()"),
                            source.replace('DefineMethod(klass, "', 'DefineMethod(klass, "custom_'),
                            source.replace('DefineMethod(klass, "', 'DefineMethod(klass, " '),
                            source + ("void Custom() { DefineNetworkEventData(Qnil); }" if owner == "NetworkEvent"
                                      else "void Custom() { DefineFetchBody(Qnil); }")]
                for custom in variants:
                    with self.subTest(owner=owner, custom=custom):
                        replace_region(path, "HANDWRITTEN", custom)
                        original = path.read_bytes(), path.stat().st_mtime_ns
                        with self.assertRaisesRegex(ValueError, "migrate it manually"):
                            GLUE["generate_cc"](model, entry, str(path))
                        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), original)

    def test_layout_variants_and_literal_whitespace_in_tail_registrations(self) -> None:
        for owner in ("NetworkEvent", "Fetch"):
            entry = self.entries[owner]
            path = "binding_" + GLUE["snake"](owner) + ".cc"
            hand = "RB_FUNC" + legacy_glue(owner, True, True).split("RB_FUNC", 1)[1]
            hand = hand.replace("RB_FUNC(", "RB_FUNC ( ")
            hand = "\n".join("  " + line for line in hand.splitlines())
            migrated, tail = GLUE["migrate_network_getter"](entry, path, hand, "")
            self.assertEqual((migrated.strip(), tail), ("", ""))
            symbol = "NetworkEvent_Data" if owner == "NetworkEvent" else "Fetch_Body"
            member = "data" if owner == "NetworkEvent" else "body"
            custom_tail = f'DefineMethod(klass, " {member}", {symbol});'
            with self.assertRaisesRegex(ValueError, "migrate it manually"):
                GLUE["migrate_network_getter"](entry, path, "", custom_tail)

    def test_migration_requires_matching_header_filename_and_binary_replacement(self) -> None:
        for owner in ("NetworkEvent", "Fetch"):
            entry = self.entries[owner]
            path = "binding_" + GLUE["snake"](owner) + ".cc"
            hand = legacy_glue(owner, True, True).rstrip("\n")
            for candidate, candidate_path in ((dict(entry, header="other.h"), path),
                                               (entry, "binding_other.cc"),
                                               (next(e for e in old_document(self.document)["classes"]
                                                     if e["cpp_name"] == owner), path)):
                self.assertEqual(GLUE["migrate_network_getter"](candidate, candidate_path, hand, ""),
                                 (hand, ""))


if __name__ == "__main__":
    unittest.main(verbosity=2)

"""Parse the WebGPU C header (webgpu.h) into structured IDL data.

This is a lightweight, regex based state machine that scans the C
declarations emitted by ``webgpu.h`` and produces a JSON-serializable
dict consumed by :mod:`generator`:

.. code-block:: text

    {
      "enum":     { "<Name>": ["<Member>", ...], ... },
      "bitmask":  { "<Name>": ["<Member>", ...], ... },
      "class":    { "<Name>": {"<Method>": {"params": [...], "return": ...}}, ... },
      "struct":   { "<Name>": [{"type": ..., "name": ..., "default": ...}, ...], ... },
      "function": { "<Name>": {"params": [...], "return": ...}, ... },
    }
"""

import json
import os
import re
from typing import Any, Dict, Optional


def _strip_wgpu_prefix(name: str) -> str:
    """Drop the leading 'WGPU'/'wgpu' prefix from a C type or function name."""
    return name[4:]


class WGPUParser:
    """State machine that extracts IDL information from the C header."""

    # Regexes for the various declaration shapes found in webgpu.h.
    _ENUM_PATTERN = re.compile(r"typedef enum (\w+)")
    _ENUM_MEMBER_PATTERN = re.compile(r"(\w+)\s*=")
    _BITMASK_PATTERN = re.compile(r"static\s+const\s+\w+\s+(\w+)\s*=")
    _STRUCT_PATTERN = re.compile(r"typedef\s+struct\s+(\w+)\s*\{")
    # The nested macro body may span several lines, hence re.DOTALL.
    _STRUCT_INIT_PATTERN = re.compile(
        r"\/\*\.(\w+)=\*\/\s*(?:_wgpu_MAKE_INIT_STRUCT\([^,]+,\s*\{(.+?)\}\)|([^,]+))",
        re.DOTALL,
    )

    def __init__(self) -> None:
        self._stage: Optional[str] = None
        self._line_cache: str = ""
        self._parse_data: Dict[str, Dict[str, Any]] = {
            "enum": {},
            "bitmask": {},
            "class": {},
            "function": {},
            "struct": {},
        }

    # -- Public API ---------------------------------------------------------

    def parse(self, code: str) -> Dict[str, Dict[str, Any]]:
        """Parse *code* and return the extracted IDL data."""
        for line in code.split("\n"):
            self._process_line(line.strip())
        return self._parse_data

    # -- Declaration handlers -----------------------------------------------

    def _process_enum(self, block: str) -> None:
        enum_name = self._ENUM_PATTERN.search(block).group(1)
        members = self._ENUM_MEMBER_PATTERN.findall(block)

        members_data = []
        for member in members:
            # Member names are emitted as "<ENUM>_<MEMBER>" by webgpu.h.
            names = member.split("_", 1)
            members_data.append(names[1])
        self._parse_data["enum"][_strip_wgpu_prefix(enum_name)] = members_data

    def _process_bitmask(self, block: str) -> None:
        match = self._BITMASK_PATTERN.search(block)
        if not match:
            return

        names = match.group(1).split("_", 1)
        bitmask_name = _strip_wgpu_prefix(names[0])
        bitmask_data = self._parse_data["bitmask"].setdefault(bitmask_name, [])
        bitmask_data.append(names[1])

    def _process_struct(self, block: str) -> None:
        block = block.replace("\n", "")
        struct_name = self._STRUCT_PATTERN.search(block).group(1)
        content = block[block.find("{") + 1 : block.find("}")]

        struct_members = []
        for member in content.split(";"):
            parts = member.strip().rsplit(" ", 1)
            if len(parts) < 2:
                continue
            struct_members.append({
                "type": parts[0],
                "name": parts[1],
            })

        self._parse_data["struct"][_strip_wgpu_prefix(struct_name)] = struct_members

    def _process_struct_init(self, block: str) -> None:
        struct_data = self._parse_struct_init_macro(block)
        struct_name = block[block.find("(") + 1 : block.find(",")]

        # Record the default value of every member mentioned by the macro.
        for name, value in struct_data.items():
            for target in self._parse_data["struct"][_strip_wgpu_prefix(struct_name)]:
                if target["name"] == name:
                    target["default"] = value

    def _process_function(self, block: str) -> None:
        first_token = block.find("(")
        last_token = block.find(")")
        start_token = block.rfind(" wgpu")
        func_name = block[start_token + 1 : first_token]

        params_decl = block[first_token + 1 : last_token]
        return_type = block[:start_token].replace("WGPU_EXPORT", "").strip()

        param_list = []
        for param in params_decl.split(","):
            parts = param.strip().rsplit(" ", 1)
            if len(parts) < 2:
                continue
            param_list.append({
                "type": parts[0],
                "name": parts[1],
            })

        # Methods whose first parameter is the object itself (and whose name
        # is "<Class><Method>") are routed to the class table; everything else
        # (including the <Struct>FreeMembers helpers) is a global function.
        if param_list and not func_name.endswith("FreeMembers"):
            first_param = param_list[0]
            class_name = _strip_wgpu_prefix(first_param["type"])
            method_name = _strip_wgpu_prefix(func_name)
            if method_name.startswith(class_name):
                class_data = self._parse_data["class"].setdefault(class_name, {})
                class_data[method_name[len(class_name):]] = {
                    "params": param_list[1:],
                    "return": return_type,
                }
                return

        self._parse_data["function"][_strip_wgpu_prefix(func_name)] = {
            "params": param_list,
            "return": return_type,
        }

    # -- Line dispatch ------------------------------------------------------

    def _process_line(self, line: str) -> None:
        # Enum
        if line.startswith("typedef enum"):
            self._stage = "enum"
        if self._stage == "enum" and line.startswith("}"):
            self._stage = None
            self._line_cache += line + "\n"
            self._process_enum(self._line_cache)
            self._line_cache = ""
            return

        # Bitmask
        if line.startswith("static const"):
            self._process_bitmask(line)
            return

        # Struct
        if line.startswith("typedef struct"):
            self._stage = "struct"
        if self._stage == "struct" and line.startswith("}"):
            self._stage = None
            self._line_cache += line + "\n"
            self._process_struct(self._line_cache)
            self._line_cache = ""
            return

        # Function (with or without the WGPU_EXPORT declaration)
        line_parts = line.split(" ")
        if line.startswith("WGPU_EXPORT") or (
            len(line_parts) > 1 and line_parts[1].startswith("wgpu")
        ):
            self._process_function(line)
            return

        # Struct init macro
        if line.startswith("#define") and "MAKE_INIT_STRUCT" in line:
            self._stage = "struct.init"
        if self._stage == "struct.init" and line == "})":
            self._stage = None
            self._line_cache += line + "\n"
            self._process_struct_init(self._line_cache)
            self._line_cache = ""
            return

        # Accumulate the current declaration block (dropping comment lines).
        if self._stage is not None:
            if self._stage == "struct.init" or not (
                line.startswith("//")
                or line.startswith("/*")
                or line.startswith("*/")
                or line.startswith("*")
            ):
                self._line_cache += line + "\n"

    # -- Helpers ------------------------------------------------------------

    def _parse_struct_init_macro(self, text: str) -> Dict[str, str]:
        """Recursively expand a ``WGPU_MAKE_INIT_STRUCT`` macro block."""
        text = text.replace("_wgpu_COMMA", ",").replace("\\", "")
        matches = self._STRUCT_INIT_PATTERN.findall(text)

        results = {}
        for key, nested_content, simple_value in matches:
            if nested_content:
                results[key] = self._parse_struct_init_macro(nested_content)
            else:
                results[key] = simple_value.strip()
        return results


if __name__ == "__main__":
    cwd = os.path.dirname(os.path.abspath(__file__))
    header_path = os.path.join(cwd, "webgpu.h")
    with open(header_path, "r") as file:
        content = file.read()

    parser = WGPUParser()
    parse_data = parser.parse(content)

    with open(header_path + ".json", "w+") as file:
        file.write(json.dumps(parse_data))

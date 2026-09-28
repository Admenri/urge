"""Render the C++ wrapper header from the IDL data produced by the parser.

:mod:`generator` drives a Jinja2 template (``template.hpp``) that turns the
JSON-serializable dict emitted by :class:`parser.WGPUParser` into the
``wgpu::`` C++ wrapper header (``webgpu_cpp.hpp``).
"""

import json
import os
from typing import Any, Dict

import jinja2

#: Default column limit used to wrap long declaration lines.
_WRAP_WIDTH = 100
#: Indentation applied to wrapped continuation lines.
_WRAP_CONTINUATION = "    "

#: Bind group layout entries are classified by testing these members against
#: their "not used" value, so a zero initialized layout member has to default to
#: it instead of the C ``INIT`` value ``Undefined``.
_BINDING_NOT_USED_DEFAULTS = {
    "WGPUBufferBindingType_Undefined": "BufferBindingType::BindingNotUsed",
    "WGPUSamplerBindingType_Undefined": "SamplerBindingType::BindingNotUsed",
    "WGPUTextureSampleType_Undefined": "TextureSampleType::BindingNotUsed",
    "WGPUStorageTextureAccess_Undefined": "StorageTextureAccess::BindingNotUsed",
}


class WGPUGenerator:
    """Jinja2 renderer that converts parsed IDL data into C++ glue code."""

    def __init__(self) -> None:
        cwd = os.path.dirname(os.path.abspath(__file__))
        template_path = os.path.join(cwd, "template.hpp")
        with open(template_path, "r") as file:
            template_text = file.read()

        self._environment = jinja2.Environment(
            extensions=["jinja2.ext.do", "jinja2.ext.loopcontrols"],
            lstrip_blocks=True,
            trim_blocks=True,
            line_comment_prefix="//*",
        )
        self._template = self._environment.from_string(template_text)
        self._data: Dict[str, Any] = {}

    # -- Type / value mapping ----------------------------------------------

    def _is_known_type(self, ctype: str, target: str) -> bool:
        """Return whether *ctype* (a C 'WGPU...' name) is listed under *target*."""
        if ctype.startswith("WGPU"):
            ctype = ctype[4:]
        return ctype in self._data[target]

    def as_cpptype(self, ctype: str, in_struct: bool = False) -> str:
        """Map a C type to its C++ wrapper equivalent."""
        ctype = ctype.replace("WGPU_NULLABLE", "").strip()
        if ctype == "WGPUProc":
            return ctype
        if ctype == "WGPUStatus":
            return "ConvertibleStatus"
        if ctype.startswith("WGPU"):
            if ctype.endswith("CallbackInfo"):
                return ctype
            if self._is_known_type(ctype, "class") and not in_struct:
                return ctype[4:] + " const&"
            return ctype[4:]
        return ctype

    def as_cppvalue(self, cvalue: str) -> str:
        """Map a C default value (from the struct init macros) to C++."""
        if cvalue in _BINDING_NOT_USED_DEFAULTS:
            return _BINDING_NOT_USED_DEFAULTS[cvalue]
        if cvalue == "NULL":
            return "nullptr"
        if cvalue == "WGPU_TRUE":
            return "true"
        if cvalue == "WGPU_FALSE":
            return "false"
        if cvalue.startswith("_wgpu_ENUM_ZERO_INIT"):
            return "{}"
        # A nested struct which the C init macro leaves zeroed. Without this the
        # macro stays as it is written, and a member of a struct type is then
        # initialized with a scalar instead of being zero initialized.
        if cvalue == "_wgpu_STRUCT_ZERO_INIT":
            return "{}"
        if cvalue.startswith("WGPU"):
            # Struct init macro.
            if cvalue.endswith("_INIT"):
                return "{}"
            # Plain C define.
            if cvalue.startswith("WGPU_"):
                return cvalue

            if self._is_known_type(cvalue, "class"):
                # Class handle.
                return cvalue[4:]
            # Enum value: "<ENUM>_<MEMBER>" -> "<Enum>::<Member>".
            enum_parts = cvalue.split("_", 1)
            enum_value = enum_parts[1]
            if enum_value[0].isdigit():
                enum_value = "e" + enum_value
            return enum_parts[0][4:] + "::" + enum_value
        return cvalue

    # -- Formatting helpers ------------------------------------------------

    def _wrap_declaration(
        self,
        text: str,
        max_width: int = _WRAP_WIDTH,
        continuation: str = _WRAP_CONTINUATION,
    ) -> str:
        """Wrap a long declaration at its argument/parameter commas.

        When *text* fits within ``max_width`` characters it is returned
        unchanged. Otherwise it is broken after every comma that sits at
        parenthesis depth 0 (e.g. struct initializer lists) or depth 1
        (e.g. function parameter lists), and each continuation line is
        prefixed with *continuation*.
        """
        if len(text) <= max_width:
            return text

        lines = []
        current = ""
        depth = 0
        at_line_start = False
        for char in text:
            # Drop the whitespace that follows a broken comma so continuation
            # lines start exactly at the requested indent.
            if at_line_start and char == " ":
                continue
            current += char
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            if char == "," and depth <= 1:
                lines.append(current.rstrip())
                current = continuation
                at_line_start = True
            else:
                at_line_start = False
        lines.append(current.rstrip())
        return "\n".join(lines)

    # -- Rendering ---------------------------------------------------------

    def render(self, json_data: Dict[str, Any]) -> str:
        """Render *json_data* through the template and return the header text."""
        self._data = json_data
        render_params = {
            "as_cppvalue": self.as_cppvalue,
            "as_cpptype": self.as_cpptype,
            "wrap": self._wrap_declaration,
            "json_data": json_data,
            "list": list,
            "len": len,
        }
        return self._template.render(render_params)


if __name__ == "__main__":
    cwd = os.path.dirname(os.path.abspath(__file__))
    json_path = os.path.join(cwd, "webgpu.h.json")
    with open(json_path, "r") as file:
        data = json.load(file)

    generator = WGPUGenerator()
    header = generator.render(data)

    output_path = os.path.join(cwd, "webgpu_cpp.hpp")
    with open(output_path, "w+") as file:
        file.write(header)


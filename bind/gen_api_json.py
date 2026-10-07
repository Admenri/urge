#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""URGE binding IR generator.

Scans the core headers (`core/*.h`) for the declarations a `URGE_BINDING()`
annotation marks as exported and emits a single JSON document,
`bind/api_reference.json`, which `bind/generate_binding.py` turns into the
CRuby glue code.  The rules this script implements are documented in
`bind/Bindgen.md`.

Usage:
    python bind/gen_api_json.py [output.json]

Defaults to `bind/api_reference.json` next to this script.  The output is
deterministic: keys are written in a fixed order, classes follow CLASS_ORDER
and modules are sorted by name, so regenerating produces a byte identical file
as long as the headers and `meta.generated_at` aside do not change.

This script only reads the headers; it never modifies anything.
"""

from __future__ import annotations

import glob
import hashlib
import json
import os
import re
import sys
from datetime import datetime, timezone
from typing import Any, Optional

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIR = os.path.join(REPO_ROOT, "core")
RULES_FILE = "bind/Bindgen.md"
DEFAULT_OUTPUT = os.path.join(REPO_ROOT, "bind", "api_reference.json")

# A class or a declaration is exported to Ruby when the `URGE_BINDING(...)`
# annotation precedes it.  An empty body is a plain export marker; a
# `Name : "..."` body additionally renames the Ruby side (the one thing the
# mechanical `camel_to_snake` rule cannot produce).  The macro itself expands
# to nothing, see `core/definition.h`.

# `Singleton<T>` classes which are exported as Ruby modules, not as classes.
#
# This list is not cosmetic: a module is reached through `Ty::Get()` and owns no
# Ruby object, while a class goes through `RB_DEF_TYPE`/`SetupSelfData`, which
# require `Release()`/`AddRef()`.  `Singleton` has neither, so a `Singleton<T>`
# left out of this set fails to compile the moment its glue is generated.
MODULE_CLASSES = {"Graphics", "Input", "Audio", "Mouse", "GPUDevice"}

# Fixed presentation order (also the registration order of binding_init.cc:
# every class must follow the class it derives from).  A class that is not
# listed still gets exported -- it is appended in name order -- but only after
# the listed ones, which can put it *before* its base class, so a new derived
# class has to be appended here too.
CLASS_ORDER = [
    "Disposable",
    "GPUObject",
    "GPUBufferDescriptor",
    "GPUTextureDescriptor",
    "GPUTextureViewDescriptor",
    "GPUSamplerDescriptor",
    "GPUShaderModuleDescriptor",
    "GPUBindGroupLayoutDescriptor",
    "GPUPipelineLayoutDescriptor",
    "GPUBindGroupDescriptor",
    "GPUComputePipelineDescriptor",
    "GPURenderPipelineDescriptor",
    "GPURenderPassDescriptor",
    "GPUComputePassDescriptor",
    "GPUQuerySetDescriptor",
    "GPUBuffer",
    "GPUTexture",
    "GPUTextureView",
    "GPUSampler",
    "GPUShaderModule",
    "GPUBindGroupLayout",
    "GPUPipelineLayout",
    "GPUBindGroup",
    "GPURenderPipeline",
    "GPUComputePipeline",
    "GPUQuerySet",
    "GPUCommandBuffer",
    "GPUCommandEncoder",
    "GPURenderPassEncoder",
    "GPUComputePassEncoder",
    "GPUQueue",
    "Node",
    "Geometry",
    "Graphics",
    "Input",
    "Bitmap",
    "Color",
    "Font",
    "Palette",
    "Plane",
    "Rect",
    "Sprite",
    "Table",
    "Tone",
    "Vector2",
    "Vector3",
    "Vector4",
    "TilemapVX",
    "TilemapXP",
    "Viewport",
    "WindowVX",
    "WindowXP",
    "Effect",
    "AudioStream",
]

# Naming policy: a declaration's Ruby name is its C++ name converted by
# camel_to_snake(), and nothing else.  There is deliberately **no override
# table** -- a name the mechanical rule cannot produce (`[]`, `exist?`, `rect`,
# ...) is spelled out on the declaration itself with URGE_BINDING(Name : "..."),
# which keeps the name next to the thing it names instead of in a second list
# that silently rots when a declaration is renamed.  See Bindgen.md
# "命名映射规则".

# C++ parameter / return types the glue layer knows how to marshal.  Everything
# else (glm::*, wgpu::*, SDL_*, ZValue, ...) marks a declaration as internal.
SCALAR_TYPE_FORMAT = {
    "int8_t": "i",
    "int16_t": "i",
    "int32_t": "i",
    "int": "i",
    "uint8_t": "u",
    "uint16_t": "u",
    "uint32_t": "u",
    "unsigned int": "u",
    "int64_t": "l",
    "uint64_t": "p",
    "float": "f",
    "double": "f",
    "bool": "b",
    "std::string": "s",
    "const char*": "z",
    "char*": "z",
}

REFPTR_RE = re.compile(r"^RefPtr<\s*(?P<inner>[\w:]+)\s*>$")
VECTOR_RE = re.compile(r"^std::vector<\s*(?P<inner>.+?)\s*>$")

# `std::vector<T>` crosses the boundary as a Ruby Array, so whether one is
# bindable turns on its element: `T` has to be a type the glue holds an element
# codec for (`VectorCodec`, cruby_utils.h).  The table maps the element spelling
# to the canonical C++ type that codec is instantiated with -- `int` and
# `unsigned int` are the same type as the fixed width ones on every platform
# this engine builds for, and the C++ family keeps exactly one specialization
# per element, so the two spellings share one.
#
# `bool` is absent on purpose: `std::vector<bool>` is a packed bitset whose
# element is a proxy rather than a `bool`, so it is not an interchangeable
# container.  A `std::vector` of anything else -- `RefPtr<T>`, `glm::vec2`, a
# nested vector -- is left unbound, the same as any other internal type.
#
# `generate_binding.py` mirrors this list in `VECTOR_CODECS`; a new element has
# to be added in both places, plus a `VectorCodec` specialization on the glue.
VECTOR_ELEMENTS = {
    "int8_t": "int8_t",
    "int16_t": "int16_t",
    "int32_t": "int32_t",
    "int": "int32_t",
    "uint8_t": "uint8_t",
    "uint16_t": "uint16_t",
    "uint32_t": "uint32_t",
    "unsigned int": "uint32_t",
    "int64_t": "int64_t",
    "uint64_t": "uint64_t",
    "float": "float",
    "double": "double",
    "std::string": "std::string",
}

# The Ruby documentation type of one vector element.
VECTOR_RUBY_ELEMENTS = {
    "float": "Float",
    "double": "Float",
    "std::string": "String",
}


def vector_element(cpp_type: str) -> Optional[str]:
    """The element of a `std::vector<T>`, or None when the type is not one."""
    m = VECTOR_RE.match(cpp_type.strip())
    return m.group("inner") if m else None

# Documentation level Ruby types (the IR is also the API reference source).
RUBY_TYPE_MAP = {
    "void": "nil",
    "int8_t": "Integer",
    "int16_t": "Integer",
    "int32_t": "Integer",
    "int": "Integer",
    "uint8_t": "Integer",
    "uint16_t": "Integer",
    "uint32_t": "Integer",
    "int64_t": "Integer",
    "uint64_t": "Integer",
    "bool": "Boolean",
    "float": "Float",
    "double": "Float",
    "std::string": "String",
    "const char*": "String",
    "char*": "String",
    "void*": "Integer",
}


# ---------------------------------------------------------------------------
# Small C++ helpers
# ---------------------------------------------------------------------------


def strip_comments(text: str) -> str:
    """Remove C/C++ comments, keeping the code otherwise untouched."""
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text[i : i + 2] == "//":
            j = text.find("\n", i)
            i = n if j == -1 else j
        elif text[i : i + 2] == "/*":
            j = text.find("*/", i + 2)
            if j == -1:
                break
            i = j + 2
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def split_top_level(text: str, sep: str = ",") -> list[str]:
    """Split on `sep` at paren/brace/bracket depth 0."""
    parts = []
    depth = 0
    cur = []
    for c in text:
        if c in "([{":
            depth += 1
        elif c in ")]}":
            depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur))
            cur = []
        else:
            cur.append(c)
    parts.append("".join(cur))
    return parts


def find_matching(text: str, open_pos: int) -> int:
    """Return the index just past the `}` matching the `{` at open_pos."""
    depth = 0
    i = open_pos
    n = len(text)
    while i < n:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return n


def split_statements(body: str) -> list[str]:
    """Split a class / export body into top level declarations.

    A declaration ends at a `;` at depth 0, or after a balanced `{...}` block
    (function body, initializer list body) optionally followed by `;`.
    """
    stmts = []
    i = 0
    n = len(body)
    while i < n:
        while i < n and body[i].isspace():
            i += 1
        if i >= n:
            break
        start = i
        depth = 0
        while i < n:
            c = body[i]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            elif c == "{" and depth == 0:
                i = find_matching(body, i)
                while i < n and body[i].isspace():
                    i += 1
                if i < n and body[i] == ";":
                    i += 1
                break
            elif c == ";" and depth == 0:
                i += 1
                break
            i += 1
        stmts.append(body[start:i])
    return [s.strip() for s in stmts if s.strip()]


# ---------------------------------------------------------------------------
# Type helpers
# ---------------------------------------------------------------------------

ANNOTATION_RE = re.compile(r"^\s*URGE_BINDING\s*\((?P<body>.*?)\)\s*", re.S)
ANNOTATION_NAME_RE = re.compile(r"\bName\s*:\s*\"(?P<name>[^\"]*)\"")


def is_bindable_type(cpp_type: str) -> bool:
    t = cpp_type.strip()
    if t in SCALAR_TYPE_FORMAT or REFPTR_RE.match(t):
        return True
    element = vector_element(t)
    return element is not None and element in VECTOR_ELEMENTS


def refptr_inner(cpp_type: str) -> Optional[str]:
    m = REFPTR_RE.match(cpp_type.strip())
    return m.group("inner") if m else None


def map_ruby_type(cpp_type: str) -> str:
    """Map a C++ type to a documentation level Ruby type."""
    t = cpp_type.strip()
    if t in RUBY_TYPE_MAP:
        return RUBY_TYPE_MAP[t]
    element = vector_element(t)
    if element is not None and element in VECTOR_ELEMENTS:
        return "Array<" + VECTOR_RUBY_ELEMENTS.get(element, "Integer") + ">"
    inner = refptr_inner(t)
    if inner:
        return inner.split("::")[-1]
    m = re.match(r"std::vector<RefPtr<\s*([\w:]+)\s*>>$", t)
    if m:
        return "Array<" + m.group(1).split("::")[-1] + ">"
    m = re.match(r"std::optional<(\w+)>$", t)
    if m:
        return map_ruby_type(m.group(1))
    return t.split("::")[-1]


def camel_to_snake(name: str) -> str:
    """CamelCase (with acronyms) -> snake_case.

    StretchBlt -> stretch_blt, BGMPlay -> bgm_play, ZoomX -> zoom_x, OX -> ox.
    """
    out = []
    for i, c in enumerate(name):
        if c.isupper():
            if i > 0 and (
                name[i - 1].islower() or (i + 1 < len(name) and name[i + 1].islower())
            ):
                out.append("_")
            out.append(c.lower())
        else:
            out.append(c)
    return "".join(out)


def ruby_method_name(cpp_name: str) -> str:
    """The Ruby name of a method.  Purely mechanical -- see the policy above."""
    return camel_to_snake(cpp_name)


def ruby_attr_base(cpp_name: str) -> str:
    """The base name of an `ATTR(type, name)` pair.  Same rule as a method."""
    return camel_to_snake(cpp_name)


# ---------------------------------------------------------------------------
# Declaration parsing
# ---------------------------------------------------------------------------


def parse_params(params_text: str) -> list[dict[str, Any]]:
    """Parse a C++ parameter list into {name, type, default} entries."""
    params = []
    if not params_text.strip():
        return params
    prev_type = None
    for part in split_top_level(params_text):
        part = part.strip()
        if not part:
            continue
        if part == "...":
            params.append({"name": "...", "type": "..."})
            continue
        # split off the default value
        default = None
        eq = -1
        depth = 0
        for k, ch in enumerate(part):
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "=" and depth == 0:
                eq = k
                break
        if eq != -1:
            default = part[eq + 1 :].strip()
            part = part[:eq].strip()
        if not part:
            continue
        # `type name[16]` array parameters
        arr = re.search(r"\[[^\]]*\]$", part)
        arr_suffix = ""
        if arr:
            arr_suffix = arr.group(0)
            part = part[: arr.start()].strip()
        tokens = part.split()
        name = tokens[-1].rstrip(",")
        type_tokens = tokens[:-1]
        if not type_tokens:
            # continuation of the previous type (e.g. `int x, y`)
            if prev_type is not None:
                type_tokens = [prev_type]
            else:
                type_tokens = []
        type_str = " ".join(type_tokens).strip()
        if type_str:
            prev_type = type_str
        params.append(
            {"name": name, "type": type_str + arr_suffix, "default": default}
        )
    return params


def analyze_call(stmt: str) -> Optional[dict]:
    """Analyze a declaration containing a call (method / constructor).

    Returns {func_name, params, return_type} or None when the statement has no
    top level call (data member, nested type, ...).
    """
    paren = -1
    depth = 0
    for k, ch in enumerate(stmt):
        if ch == "(":
            depth += 1
            if paren == -1:
                paren = k
        elif ch == ")":
            depth -= 1
        elif ch == "{" and paren == -1:
            break
    if paren == -1:
        return None
    name_start = paren
    while name_start > 0 and (stmt[name_start - 1].isalnum() or stmt[name_start - 1] in "_~"):
        name_start -= 1
    func_name = stmt[name_start:paren].strip()
    depth = 0
    close = -1
    for k in range(paren, len(stmt)):
        if stmt[k] == "(":
            depth += 1
        elif stmt[k] == ")":
            depth -= 1
            if depth == 0:
                close = k
                break
    params = parse_params(stmt[paren + 1 : close])
    return_type = stmt[:name_start].strip()
    return_type = re.sub(
        r"^(static|virtual|inline|explicit|constexpr)\s+", "", return_type
    ).strip()
    if return_type.startswith("virtual "):
        return_type = return_type[len("virtual ") :].strip()
    return {
        "func_name": func_name,
        "params": params,
        "return_type": return_type,
        "is_static": bool(re.match(r"^\s*(static|constexpr)\b", stmt)),
    }


ATTR_RE = re.compile(
    r"^(?P<mods>(?:static\s+|virtual\s+)*)"
    r"ATTR\s*\(\s*(?P<type>.+?)\s*,\s*(?P<name>\w+)\s*\)"
    r"(?P<tail>\s*override\s*)?"
    r"\s*(?P<body>\{.*\})?"
    r"\s*;?\s*$",
    re.S,
)

MARSHAL_DUMP_RE = re.compile(r"^MARSHAL_DUMP\s*\(\s*(?P<cls>\w+)\s*\)\s*;?$")
MARSHAL_LOAD_RE = re.compile(r"^MARSHAL_LOAD\s*\(\s*(?P<cls>\w+)\s*\)\s*;?$")


def _normalize_scopes(body: str) -> str:
    """Turn an access specifier into a statement boundary.

    `split_statements` only breaks at `;` and balanced braces, so a body that
    opens with `public:` would glue that label onto the first declaration and
    hide its `URGE_BINDING()` annotation.  An access specifier is replaced by a
    `;`, which is an empty statement at class scope.
    """
    return re.sub(r"\b(?:public|protected|private)\s*:", ";\n", body)


def take_annotation(stmt: str) -> tuple[Optional[dict], str]:
    """Split a leading `URGE_BINDING(...)` annotation off a statement.

    A `URGE_BINDING()` with an empty body marks the declaration that follows as
    exported to Ruby; an optional `Name : "..."` additionally renames it.  The
    annotation applies to the very next declaration only, which is why it is
    consumed here instead of being carried around: `URGE_BINDING` expands to
    nothing in C++, so a header reader sees it as part of the declaration it
    documents, see `core/table.h`.
    """
    annotation = None
    while True:
        m = ANNOTATION_RE.match(stmt)
        if not m:
            break
        body = m.group("body")
        name = ANNOTATION_NAME_RE.search(body)
        annotation = {"name": name.group("name") if name else None}
        stmt = stmt[m.end() :]
    return annotation, stmt.strip()


def parse_class(
    name: str,
    inner: str,
    inner_start: int,
    class_start: int,
    source: str,
    text: str,
) -> dict:
    """Parse the class body: only `URGE_BINDING()` marked declarations export.

    A declaration -- a constructor, a destructor, a method, an `ATTR(...)` or a
    `MARSHAL_*` -- is exported to Ruby exactly when the statement before it
    carries a `URGE_BINDING(...)` annotation.  Everything else in the body is
    internal to the engine and is ignored here.
    """
    markers = [m.start() for m in re.finditer(r"URGE_BINDING\s*\(", inner)]
    last_marker = markers[-1] if markers else 0
    export = "core/%s:%d-%d" % (
        source,
        text.count("\n", 0, class_start) + 1,
        text.count("\n", 0, inner_start + last_marker) + 1,
    )

    result: dict[str, Any] = {
        "constructors": [],
        "initialize_copy": None,
        "instance_methods": [],
        "attributes": [],
        "data_attributes": [],
        "class_methods": [],
        "class_attributes": [],
        "marshal": {"dump": False, "load": False},
        "index": None,
        "unsupported": [],
        "export": export,
    }

    for raw_stmt in split_statements(_normalize_scopes(strip_comments(inner))):
        annotation, stmt = take_annotation(raw_stmt)
        if annotation is None or not stmt:
            continue
        if re.match(r"^(struct|class|enum|union)\b", stmt):
            continue

        m = MARSHAL_DUMP_RE.match(stmt)
        if m:
            result["marshal"]["dump"] = True
            continue
        m = MARSHAL_LOAD_RE.match(stmt)
        if m:
            result["marshal"]["load"] = True
            continue

        m = ATTR_RE.match(stmt)
        if m:
            mods = m.group("mods")
            static = bool(re.search(r"static\s+", mods))
            override = bool(re.search(r"virtual\s+", mods)) or bool(m.group("tail"))
            cpp_name = m.group("name")
            cpp_type = m.group("type").strip()
            base = annotation["name"] or ruby_attr_base(cpp_name)
            attr = {
                "name": base,
                "setter": base + "=",
                "cpp_name": cpp_name,
                "cpp_type": cpp_type,
                "ruby_type": map_ruby_type(cpp_type),
                "static": static,
                "override": override,
                "header_annotation": bool(annotation["name"]),
                "export": export,
            }
            if static:
                result["class_attributes"].append(attr)
            else:
                result["attributes"].append(attr)
            if not is_bindable_type(cpp_type):
                result["unsupported"].append(
                    {
                        "kind": "attribute",
                        "signature": "ATTR(%s, %s)" % (cpp_type, cpp_name),
                        "reason": "internal attribute type",
                    }
                )
            continue

        call = analyze_call(stmt)
        if call is None:
            for dm in parse_data_members(stmt):
                result["data_attributes"].append(dm)
            continue

        func_name = call["func_name"]
        return_type = call["return_type"]
        params = call["params"]

        if func_name.startswith("~"):
            # A marked destructor is exported for documentation only: Ruby
            # never calls it, the typed data release does.
            continue

        if func_name == name:
            bad = [p["type"] for p in params if not is_bindable_type(p["type"])]
            if bad:
                result["unsupported"].append(
                    {
                        "kind": "constructor",
                        "signature": "%s(%s)"
                        % (name, ", ".join(p["type"] for p in params)),
                        "reason": "internal parameter type: " + ", ".join(bad),
                    }
                )
                continue
            if len(params) == 1 and params[0]["type"] == "RefPtr<" + name + ">":
                result["initialize_copy"] = {"params": params, "export": export}
            else:
                result["constructors"].append({"params": params, "export": export})
            continue

        bad = [p["type"] for p in params if not is_bindable_type(p["type"])]
        if bad or not (is_bindable_type(return_type) or return_type == "void"):
            result["unsupported"].append(
                {
                    "kind": "method",
                    "signature": "%s %s(%s)"
                    % (return_type, func_name, ", ".join(p["type"] for p in params)),
                    "reason": "internal parameter type: "
                    + (", ".join(bad) if bad else return_type),
                }
            )
            continue

        ruby_name = annotation["name"] or ruby_method_name(func_name)

        if ruby_name.startswith("["):
            # Index operator accessor: exposed as `#[]` / `#[]=`, which cannot
            # be expressed as a plain method (the Ruby side gets the operands in
            # a different order than the C++ signature).  The body is written by
            # hand, see Bindgen.md "特殊绑定".
            if result["index"] is None:
                result["index"] = {}
            result["index"]["set" if ruby_name == "[]=" else "get"] = {
                "ruby_name": ruby_name,
                "cpp_name": func_name,
                "params": params,
                "export": export,
            }
            continue

        entry = {
            "ruby_name": ruby_name,
            "cpp_name": func_name,
            "return": {"cpp_type": return_type, "ruby_type": map_ruby_type(return_type)},
            "params": params,
            "header_annotation": bool(annotation["name"]),
            "export": export,
        }
        if call["is_static"]:
            result["class_methods"].append(entry)
        else:
            result["instance_methods"].append(entry)

    return result


def parse_data_members(stmt: str) -> list[dict[str, Any]]:
    """Parse a public data member declaration into attribute entries."""
    stmt = stmt.rstrip(";").strip()
    members = []
    prev_type = None
    for part in split_top_level(stmt):
        part = part.strip()
        if not part:
            continue
        default = None
        eq = part.find("=")
        if eq != -1:
            default = part[eq + 1 :].strip()
            part = part[:eq].strip()
        tokens = part.split()
        if not tokens:
            continue
        if len(tokens) == 1:
            if prev_type is None:
                continue
            member_name = tokens[0].rstrip(",")
            type_str = prev_type
        else:
            member_name = tokens[-1].rstrip(",")
            type_str = " ".join(tokens[:-1])
            prev_type = type_str
        members.append(
            {
                "name": member_name,
                "setter": member_name + "=",
                "cpp_name": member_name,
                "cpp_type": type_str,
                "ruby_type": map_ruby_type(type_str),
                "static": False,
                "override": False,
                "default": default,
            }
        )
    return members


# ---------------------------------------------------------------------------
# Header / class model
# ---------------------------------------------------------------------------

CLASS_HEADER_RE = re.compile(r"\bclass\s+(\w+)\s*(?:final)?\s*(?::\s*public\s+([^{]+?))?\s*\{")


def extract_classes(text: str) -> list[dict[str, Any]]:
    """Find top level classes and return name / parent / body / inner body."""
    classes = []
    for m in CLASS_HEADER_RE.finditer(text):
        name = m.group(1)
        parent = None
        if m.group(2):
            first = split_top_level(m.group(2).strip())[0].strip()
            first = re.sub(r"<.*>$", "", first).strip()
            parent = first
        open_pos = text.find("{", m.start())
        end = find_matching(text, open_pos)
        classes.append(
            {
                "name": name,
                "parent": parent,
                "inner": text[open_pos + 1 : end - 1],
                "inner_start": open_pos + 1,
                "start": m.start(),
                "end": end,
            }
        )
    return classes


def class_is_exported(text: str, class_start: int) -> bool:
    """Whether a `URGE_BINDING()` annotation precedes the class declaration.

    The annotation sits on the line just above the class (only comments may
    separate the two), so the check strips the comments between and looks for
    the marker.
    """
    prefix = text[:class_start]
    while True:
        prefix = prefix.rstrip()
        if prefix.endswith("*/"):
            begin = prefix.rfind("/*")
            if begin == -1:
                break
            prefix = prefix[:begin]
            continue
        newline = prefix.rfind("\n")
        if "//" in prefix[newline + 1 :]:
            prefix = prefix[: newline + 1]
            continue
        break
    return (
        re.search(r"URGE_BINDING\s*\((?:[^()]|\([^()]*\))*\)\s*$", prefix)
        is not None
    )


# ---------------------------------------------------------------------------
# Document assembly
# ---------------------------------------------------------------------------


def check_base_order(ordered: list[str], classes: dict[str, dict[str, Any]]) -> None:
    """Assert that every exported class follows its exported base class.

    `ordered` is the registration order of binding_init.cc, and CRuby requires a
    superclass to exist before its subclass is defined.  A class that is neither
    derived from an exported class nor listed in CLASS_ORDER is fine; the trap is
    a *new derived* class (a `Node` subclass, say) that nobody appended to
    CLASS_ORDER: it is quietly appended in name order, which can land it before
    its base.  That produces a runtime failure at `RB_DEF_TYPE` time, far from
    its cause, so fail the build here instead.
    """
    position = {name: i for i, name in enumerate(ordered)}
    for name in ordered:
        parent = classes[name].get("cpp_parent")
        if not parent or parent not in position:
            continue  # external base (e.g. `Singleton<T>`, a std type) -- not ours
        if position[parent] > position[name]:
            raise SystemExit(
                f"gen_api_json: '{name}' derives from '{parent}' but is registered "
                f"first (position {position[name]} vs {position[parent]}).\n"
                f"  Add '{name}' to CLASS_ORDER after '{parent}' in "
                f"{os.path.relpath(os.path.abspath(__file__), REPO_ROOT)}."
            )


def build_document() -> dict[str, Any]:
    headers = sorted(glob.glob(os.path.join(SOURCE_DIR, "*.h")))
    header_names = [os.path.relpath(h, REPO_ROOT).replace("\\", "/") for h in headers]

    classes: dict[str, dict[str, Any]] = {}
    modules: dict[str, dict[str, Any]] = {}

    for header in headers:
        source = os.path.basename(header)
        with open(header, "r", encoding="utf-8", errors="replace") as f:
            text = f.read()
        for cls in extract_classes(text):
            name = cls["name"]
            if name in classes or name in modules:
                continue
            if not class_is_exported(text, cls["start"]):
                continue
            desc = parse_class(
                name, cls["inner"], cls["inner_start"], cls["start"], source, text
            )
            desc["cpp_name"] = name
            desc["cpp_parent"] = cls["parent"]
            desc["header"] = "core/" + source

            if name in MODULE_CLASSES:
                desc["kind"] = "module"
                desc["name"] = name
                desc["singleton"] = True
                desc["functions"] = desc.pop("instance_methods")
                module_attributes = desc.pop("attributes")
                for attr in module_attributes:
                    desc["functions"].append(
                        {
                            "ruby_name": attr["name"],
                            "cpp_name": attr["cpp_name"],
                            "return": {
                                "cpp_type": attr["cpp_type"],
                                "ruby_type": attr["ruby_type"],
                            },
                            "params": [],
                            "header_annotation": attr["header_annotation"],
                            "attr": True,
                            "attr_value_type": attr["cpp_type"],
                            "export": attr["export"],
                        }
                    )
                    desc["functions"].append(
                        {
                            "ruby_name": attr["setter"],
                            "cpp_name": attr["cpp_name"],
                            "return": {"cpp_type": "void", "ruby_type": "nil"},
                            "params": [
                                {
                                    "name": "value",
                                    "type": attr["cpp_type"],
                                    "default": None,
                                }
                            ],
                            "header_annotation": attr["header_annotation"],
                            "attr": True,
                            "setter": True,
                            "attr_value_type": attr["cpp_type"],
                            "export": attr["export"],
                        }
                    )
                for key in (
                    "constructors",
                    "initialize_copy",
                    "data_attributes",
                    "class_methods",
                    "class_attributes",
                    "marshal",
                    "index",
                ):
                    desc.pop(key, None)
                modules[name] = desc
            else:
                desc["kind"] = "class"
                desc["name"] = name
                desc["ruby_superclass"] = cls["parent"] or "Object"
                # A class without an exported constructor is glue only: Ruby
                # must never allocate it, see Bindgen.md.
                desc["glue"] = not desc["constructors"]
                classes[name] = desc

    ordered = [n for n in CLASS_ORDER if n in classes]
    ordered += sorted(n for n in classes if n not in CLASS_ORDER)
    check_base_order(ordered, classes)
    class_list = [classes[n] for n in ordered]
    module_list = [modules[n] for n in sorted(modules)]

    skipped = []
    for entries in (class_list, module_list):
        for desc in entries:
            for item in desc.get("unsupported", []):
                skipped.append(
                    {
                        "class": desc["cpp_name"],
                        "kind": item["kind"],
                        "signature": item["signature"],
                        "reason": item["reason"],
                    }
                )

    return {
        "meta": {
            "title": "URGE Binding IR",
            "generator": os.path.relpath(os.path.abspath(__file__), REPO_ROOT).replace(
                "\\", "/"
            ),
            "rules": RULES_FILE,
            "generated_at": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "source_headers": header_names,
            "skipped": skipped,
        },
        "modules": module_list,
        "classes": class_list,
        "aliases": [],
    }


def strip_volatile(document: dict) -> dict:
    """The document without the fields that change on every run.

    `meta.generated_at` is a wall-clock stamp, so it cannot take part in any
    content comparison.  Everything else -- including `meta.source_headers`,
    whose order follows the glob -- is part of the contract.
    """
    stable = dict(document)
    meta = dict(stable.get("meta") or {})
    meta.pop("generated_at", None)
    stable["meta"] = meta
    return stable


def document_digest(document: dict) -> str:
    """Digest of a document, `generated_at` excluded.

    Canonical (sorted keys, no whitespace) so that it depends on content only,
    not on how the JSON happens to be laid out, and so that the digest of a
    header-derived document equals the digest of the same document read back
    from disk.
    """
    payload = json.dumps(
        strip_volatile(document), sort_keys=True, ensure_ascii=False, separators=(",", ":")
    ).encode("utf-8")
    return hashlib.sha256(payload).hexdigest()


def ir_digest() -> tuple[str, dict]:
    """Digest and document of the IR the current headers would produce.

    Pure: reads the headers, touches nothing on disk.  This is the source of
    truth for "have the core headers changed?", and it is the *only* thing the
    configure-time hook has to be able to trust -- keeping it here means the
    digest can never drift from the document `main()` writes.  See
    `bind/gen_binding.cmake`.
    """
    document = build_document()
    return document_digest(document), document


def read_document(path: str) -> dict:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def write_document(path: str, document: dict) -> bool:
    """Write the IR to `path` unless the file already holds the same content.

    Returns True if the file was written.  The comparison ignores
    `meta.generated_at` (see `strip_volatile`): that stamp changes on every run,
    so comparing raw bytes would rewrite an unchanged IR every time -- and
    every `binding_*.cc` includes the IR's *digest*, not the file, so the only
    effect of that rewrite is to touch an mtime and make MSBuild reconsider the
    whole binding target.  Re-running the generator on unchanged headers has to
    be a no-op on disk; see `bind/gen_binding.cmake`.
    """
    if os.path.exists(path):
        try:
            if document_digest(read_document(path)) == document_digest(document):
                return False
        except (ValueError, OSError):
            pass  # unreadable or malformed: fall through and overwrite
    with open(path, "w", encoding="utf-8", newline="\r\n") as f:
        json.dump(document, f, indent=2, ensure_ascii=False)
        f.write("\n")
    return True


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_OUTPUT
    document = build_document()
    wrote = write_document(out, document)
    rel = os.path.relpath(out, REPO_ROOT).replace("\\", "/")
    print("%s %s" % ("Generated" if wrote else "Unchanged", rel))
    print("  classes: %d, modules: %d, skipped: %d"
          % (
              len(document["classes"]),
              len(document["modules"]),
              len(document["meta"]["skipped"]),
          ))
    return 0


if __name__ == "__main__":
    # `--digest [json]` prints a digest and writes nothing; it is what the
    # configure-time hook compares the headers against.  With no argument it
    # digests the document the headers would produce; with a path it digests
    # that file, `generated_at` excluded either way, so the two are directly
    # comparable.  Everything else keeps the historical contract
    # (`gen_api_json.py [output.json]`).
    argv = sys.argv[1:]
    if argv and argv[0] == "--digest":
        if len(argv) > 1:
            sys.stdout.write(document_digest(read_document(argv[1])) + "\n")
        else:
            sys.stdout.write(ir_digest()[0] + "\n")
        sys.exit(0)
    sys.exit(main())

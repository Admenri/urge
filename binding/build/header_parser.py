"""
HeaderParser - C++ header to IDL JSON parser for URGE_BINDING annotated classes.

Parses C++ headers with URGE_BINDING() annotations into the IDL JSON format.
Handles 7 annotation patterns:
  1. URGE_BINDING(x=1, y=2)        →  desc: {"x": "1", "y": "2"}
  2. URGE_BINDING() class XXX : YYY →  class declaration
  3. URGE_BINDING() <static> type M(type n, URGE_EXCEPTION)  →  method
  4. URGE_BINDING() type name = default;                      →  member field
  5. URGE_BINDING() URGE_ATTRIBUTE_DECLARE(name, type)        →  attribute
  6. URGE_BINDING() using Cb = base::Callback<ret(args)>      →  callback
  7. URGE_BINDING() enum class name : range { ... }           →  enum

Output format:
{
  "class": {
    "ClassName": {
      "desc": {},
      "filename": "relative/path.h",
      "parent": "ParentClass",
      "method": { ... },       // regular classes with methods
      "attribute": { ... },    // regular classes with attributes
      "member": [ ... ],       // data-only classes (member fields)
      "enum": { ... },         // nested enums (either type)
      "callback": { ... }      // callback type aliases (either type)
    }
  }
}
"""

import argparse
import json
import os
import re


# ---------------------------------------------------------------------------
# helpers
# ---------------------------------------------------------------------------

def _strip_comments(content: str) -> str:
    """Remove C++ style comments (// and /* */)."""
    content = re.sub(r"//[^\n]*", "", content)
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    return content


def _find_matching_brace(text: str, start: int) -> int:
    """Find the index of the matching '}' for an opening '{'."""
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return i - 1


def _find_matching_paren(text: str, start: int) -> int:
    """Find the index of the matching ')' for an opening '('."""
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
        i += 1
    return i - 1


def _find_matching_angle(text: str, start: int) -> int:
    """Find the index of the matching '>' for an opening '<'."""
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "<":
            depth += 1
        elif text[i] == ">":
            depth -= 1
        i += 1
    return i - 1


def _normalize_type(text: str) -> str:
    """Collapse whitespace in a type expression."""
    text = text.strip()
    result = []
    i = 0
    while i < len(text):
        ch = text[i]
        if ch in (" \t\n\r"):
            if result and result[-1] != " " and result[-1] not in ("<", ">", ","):
                result.append(" ")
            i += 1
            while i < len(text) and text[i] in (" \t\n\r"):
                i += 1
            continue
        elif ch == ",":
            result.append(", ")
            i += 1
            while i < len(text) and text[i] in (" \t\n\r"):
                i += 1
            continue
        else:
            result.append(ch)
            i += 1
    return "".join(result).strip()


def _parse_binding_desc(binding_args: str) -> dict:
    """
    Parse URGE_BINDING(x=1, y=2) arguments into a desc dict.
    URGE_BINDING() or empty → {}
    URGE_BINDING(x=1, y=2) → {"x": "1", "y": "2"}
    """
    desc = {}
    if not binding_args or not binding_args.strip():
        return desc
    for part in binding_args.split(","):
        part = part.strip()
        if not part:
            continue
        eq = part.find("=")
        if eq == -1:
            desc[part] = ""
        else:
            key = part[:eq].strip()
            val = part[eq + 1:].strip()
            desc[key] = val
    return desc


def _parse_param_list(params_text: str) -> list:
    """
    Parse comma-separated parameters, skipping URGE_EXCEPTION.
    Returns [{"name": ..., "type": ...}, ...].
    """
    params = []
    if not params_text.strip():
        return params

    parts = []
    current = ""
    depth = 0
    for ch in params_text:
        if ch in "(<":
            depth += 1
            current += ch
        elif ch in ")>":
            depth -= 1
            current += ch
        elif ch == "," and depth == 0:
            parts.append(current.strip())
            current = ""
        else:
            current += ch
    if current.strip():
        parts.append(current.strip())

    for part in parts:
        part = part.strip()
        if not part:
            continue
        if "URGE_EXCEPTION" in part or "ExceptionState" in part:
            continue
        words = part.split()
        if len(words) >= 2:
            name = words[-1]
            typ = " ".join(words[:-1])
            params.append({"name": name, "type": _normalize_type(typ)})

    return params


# ---------------------------------------------------------------------------
# HeaderParser
# ---------------------------------------------------------------------------

_BINDING_RE = re.compile(r"URGE_BINDING\s*\(([^)]*)\)")


class HeaderParser:
    """
    Parses C++ headers with URGE_BINDING annotations into IDL JSON.
    """

    def __init__(self):
        pass

    # ------------------------------------------------------------------
    # public API
    # ------------------------------------------------------------------

    def parse_file(self, header_path: str, base_dir: str = None) -> dict:
        """
        Parse a single C++ header file.

        Args:
            header_path: Absolute path to the header file.
            base_dir: Base directory for computing relative paths.

        Returns:
            {"class": {"ClassName": {...}}}
        """
        with open(header_path, "r", encoding="utf-8") as f:
            content = f.read()

        if base_dir:
            filepath = os.path.relpath(
                os.path.abspath(header_path),
                os.path.abspath(base_dir),
            ).replace("\\", "/")
        else:
            filepath = os.path.basename(header_path)

        ns_body = self._extract_namespace_body(content)
        if not ns_body:
            return {"class": {}}

        classes = self._extract_top_level(ns_body, filepath)
        return {"class": classes}

    def parse_directory(self, content_dir: str) -> dict:
        """
        Parse all C++ headers in a directory tree.

        Args:
            content_dir: Path to the content directory.

        Returns:
            {"class": {"ClassName1": {...}, ...}}
        """
        result = {"class": {}}
        headers = self._collect_headers(content_dir)

        for full_path, rel_path in headers:
            with open(full_path, "r", encoding="utf-8") as f:
                content = f.read()

            ns_body = self._extract_namespace_body(content)
            if not ns_body:
                continue

            classes = self._extract_top_level(ns_body, rel_path)
            result["class"].update(classes)

        return result

    # ------------------------------------------------------------------
    # internal: top-level extraction
    # ------------------------------------------------------------------

    def _extract_namespace_body(self, content: str, ns: str = "content") -> str:
        m = re.search(rf"namespace\s+{ns}\s*\{{", content)
        if not m:
            return ""
        start = m.end()
        depth = 1
        i = start
        while i < len(content) and depth > 0:
            if content[i] == "{":
                depth += 1
            elif content[i] == "}":
                depth -= 1
            i += 1
        return content[start : i - 1]

    def _collect_headers(self, content_dir: str) -> list:
        headers = []
        content_dir = os.path.normpath(content_dir)
        for root, dirs, files in os.walk(content_dir):
            for f in files:
                if f.endswith(".h"):
                    full_path = os.path.join(root, f)
                    rel_path = os.path.relpath(full_path, content_dir).replace("\\", "/")
                    headers.append((full_path, rel_path))
        return headers

    def _extract_top_level(self, ns_body: str, filepath: str) -> dict:
        """Scan namespace body for top-level URGE_BINDING() class declarations."""
        classes = {}
        clean = _strip_comments(ns_body)
        clean = re.sub(r"#.*", "", clean)

        pos = 0
        while pos < len(clean):
            binding_pos = -1
            depth = 0
            for i in range(pos, len(clean)):
                ch = clean[i]
                if ch == "{":
                    depth += 1
                elif ch == "}":
                    depth -= 1
                elif depth == 0 and clean[i:].startswith("URGE_BINDING"):
                    binding_pos = i
                    break

            if binding_pos == -1:
                break

            rem = clean[binding_pos:]

            # Match class (not enum class)
            class_match = re.match(
                r"URGE_BINDING\s*\(([^)]*)\)\s+class\s+(\w+)", rem
            )
            if class_match and not re.match(
                r"URGE_BINDING\s*\([^)]*\)\s+enum\s+class", rem
            ):
                binding_args = class_match.group(1)
                name = class_match.group(2)
                after_decl = rem[class_match.end():]

                brace_pos = after_decl.find("{")
                if brace_pos == -1:
                    pos = binding_pos + class_match.end()
                    continue

                # Extract parent
                parent = "Object"
                decl = after_decl[:brace_pos]
                parent_match = re.search(r":\s*public\s+(.+?)\s*$", decl)
                if parent_match:
                    parent = _normalize_type(parent_match.group(1).strip())

                # Extract class body
                body_start = class_match.end() + brace_pos + 1
                body_end = _find_matching_brace(rem, body_start)
                body_text = rem[body_start:body_end]

                info = self._parse_class_body(
                    body_text, filepath, parent, binding_args
                )
                classes[name] = info

                pos = binding_pos + body_end + 1
                continue

            pos = binding_pos + 1

        return classes

    # ------------------------------------------------------------------
    # internal: class body parsing
    # ------------------------------------------------------------------

    def _parse_class_body(
        self, body_text: str, filepath: str, parent: str, class_binding_args: str
    ) -> dict:
        """
        Parse the body of a URGE_BINDING-annotated class.
        Returns the class info dict.
        """
        info = {
            "desc": _parse_binding_desc(class_binding_args),
            "filename": filepath,
            "parent": parent,
            "method": {},
            "attribute": {},
            "member": [],
            "enum": {},
            "callback": {},
        }

        clean = _strip_comments(body_text)
        clean = re.sub(r"#.*", "", clean)
        clean = re.sub(r"\b(public|private|protected)\s*:", "", clean)

        blocks = self._split_binding_blocks(clean)

        for binding_args, block in blocks:
            block = block.strip().rstrip(";").strip()
            if not block:
                continue

            desc = _parse_binding_desc(binding_args)

            # --- 5: URGE_ATTRIBUTE_DECLARE(name, type) ---
            attr_decl = re.match(
                r"URGE_ATTRIBUTE_DECLARE\s*\((\w+)\s*,\s*(.+?)\)",
                block,
            )
            if attr_decl:
                attr_name = attr_decl.group(1)
                attr_type = _normalize_type(attr_decl.group(2))
                info["attribute"][attr_name] = {
                    "desc": desc,
                    "value": attr_type,
                }
                continue

            # URGE_ATTRIBUTE full form (with getter/setter)
            attr_full = re.match(
                r"URGE_ATTRIBUTE\s*\(\s*(\w+)\s*,\s*(.+?)\s*,",
                block,
            )
            if attr_full:
                attr_name = attr_full.group(1)
                attr_type = _normalize_type(attr_full.group(2))
                info["attribute"][attr_name] = {
                    "desc": desc,
                    "value": attr_type,
                }
                continue

            # --- 7: enum class name : range { ... } ---
            enum_match = re.match(
                r"enum\s+class\s+(\w+)\s*(?::\s*(\w+))?", block
            )
            if enum_match:
                enum_name = enum_match.group(1)
                enum_range = enum_match.group(2) or "uint32_t"
                brace_start = block.find("{")
                if brace_start == -1:
                    continue
                brace_end = _find_matching_brace(block, brace_start + 1)
                enum_body = block[brace_start + 1 : brace_end]
                members = []
                for part in enum_body.split(","):
                    part = part.strip()
                    eq_idx = part.find("=")
                    if eq_idx != -1:
                        members.append(part[:eq_idx].strip())
                    elif part:
                        members.append(part)
                info["enum"][enum_name] = {
                    "desc": desc,
                    "range": enum_range,
                    "member": members,
                }
                continue

            # --- 6: using Callback = base::RepeatingCallback<ret(args)> ---
            using_match = re.match(r"using\s+(\w+)\s*=\s*", block)
            if using_match:
                callback_name = using_match.group(1)
                rest = block[using_match.end():]
                lt_pos = rest.find("<")
                if lt_pos == -1:
                    continue
                gt_pos = _find_matching_angle(rest, lt_pos + 1)
                template_content = rest[lt_pos + 1 : gt_pos]
                paren_pos = template_content.find("(")
                if paren_pos == -1:
                    continue
                ret_type = _normalize_type(template_content[:paren_pos])
                paren_end = _find_matching_paren(template_content, paren_pos + 1)
                params_text = template_content[paren_pos + 1 : paren_end]
                params = _parse_param_list(params_text)
                info["callback"][callback_name] = {
                    "desc": desc,
                    "return": ret_type,
                    "param": params,
                }
                continue

            # --- field vs method ---
            paren_idx = block.find("(")
            eq_idx = block.find("=")

            is_field = False
            if paren_idx == -1:
                is_field = True
            elif eq_idx != -1 and eq_idx < paren_idx:
                is_field = True

            # --- 4: member field: type name = default; ---
            if is_field:
                field_block = block.rstrip(";").strip()
                eq_pos = field_block.find("=")
                decl_part = (
                    field_block[:eq_pos].strip() if eq_pos != -1 else field_block
                )
                parts = decl_part.split()
                if len(parts) >= 2:
                    field_name = parts[-1]
                    field_type = _normalize_type(" ".join(parts[:-1]))
                    info["member"].append({
                        "desc": desc,
                        "name": field_name,
                        "type": field_type,
                    })
                continue

            # --- 3: method (possibly static) ---
            static = False
            static_match = re.match(r"static\s+", block)
            if static_match:
                static = True
                block = block[static_match.end():].strip()
                paren_idx = block.find("(")

            if paren_idx == -1:
                continue

            # Strip method body { ... } if present
            brace_idx = block.find("{")
            if brace_idx != -1:
                block = block[:brace_idx].strip()

            prefix = block[:paren_idx].strip()
            parts = prefix.split()
            if len(parts) >= 2:
                method_name = parts[-1]
                ret_type = " ".join(parts[:-1])
            elif len(parts) == 1:
                method_name = parts[0]
                ret_type = "void"
            else:
                continue

            paren_end = _find_matching_paren(block, paren_idx + 1)
            params_text = block[paren_idx + 1 : paren_end]
            params = _parse_param_list(params_text)

            info["method"][method_name] = {
                "desc": desc,
                "static": static,
                "param": params,
                "return": _normalize_type(ret_type),
            }

        # Clean up empty collections and determine class type
        self._cleanup_class_info(info)
        return info

    @staticmethod
    def _cleanup_class_info(info: dict):
        """Remove empty collections from class info."""
        for key in ("method", "attribute", "member", "enum", "callback"):
            val = info[key]
            if isinstance(val, (list, dict)) and not val:
                del info[key]

    def _split_binding_blocks(self, body_text: str) -> list:
        """
        Split body_text into blocks delimited by URGE_BINDING annotations.
        Returns [(binding_args, block_text), ...].
        """
        blocks = []
        pos = 0
        while True:
            m = _BINDING_RE.search(body_text, pos)
            if not m:
                break
            binding_args = m.group(1)
            block_start = m.end()

            next_m = _BINDING_RE.search(body_text, block_start)
            block_end = next_m.start() if next_m else len(body_text)

            block_text = body_text[block_start:block_end].strip()
            blocks.append((binding_args, block_text))
            pos = block_end

        return blocks


# ---------------------------------------------------------------------------
# CLI entry point (for standalone use)
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Parse URGE binding C++ headers to IDL JSON"
    )
    parser.add_argument(
        "input",
        help="Path to a header file or content directory",
    )
    parser.add_argument(
        "-b", "--base-dir", default=None,
        help="Base directory for relative file paths",
    )
    parser.add_argument(
        "-o", "--output", default=None,
        help="Output JSON file path (prints to stdout if omitted)",
    )
    args = parser.parse_args()

    input_path = os.path.abspath(args.input)
    hp = HeaderParser()

    if os.path.isfile(input_path):
        base_dir = args.base_dir or os.path.dirname(input_path)
        result = hp.parse_file(input_path, base_dir)
    else:
        result = hp.parse_directory(input_path)

    json_str = json.dumps(result, indent=2, ensure_ascii=False)

    if args.output:
        output_path = os.path.abspath(args.output)
        os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            f.write(json_str)
        print(f"Generated IDL JSON: {output_path}")
    else:
        print(json_str)


if __name__ == "__main__":
    main()

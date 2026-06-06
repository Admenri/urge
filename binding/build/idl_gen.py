import argparse
import json
import os
import re
import sys


def strip_comments(content):
    content = re.sub(r"//[^\n]*", "", content)
    content = re.sub(r"/\*.*?\*/", "", content, flags=re.DOTALL)
    return content


def extract_namespace_body(content):
    m = re.search(r"namespace\s+content\s*\{", content)
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
    return content[start:i - 1]


def find_matching_brace(text, start):
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
        i += 1
    return i - 1


def find_matching_paren(text, start):
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "(":
            depth += 1
        elif text[i] == ")":
            depth -= 1
        i += 1
    return i - 1


def find_matching_angle(text, start):
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        if text[i] == "<":
            depth += 1
        elif text[i] == ">":
            depth -= 1
        i += 1
    return i - 1


def parse_type(text):
    text = text.strip()
    if not text:
        return text

    result = ""
    i = 0
    while i < len(text):
        if text[i] == "<":
            result += "<"
            i += 1
            inner = ""
            depth = 1
            while i < len(text) and depth > 0:
                if text[i] == "<":
                    depth += 1
                elif text[i] == ">":
                    depth -= 1
                if depth > 0:
                    inner += text[i]
                i += 1
            result += parse_type(inner) + ">"
        elif text[i] == ",":
            result += ", " + parse_type(text[i + 1:])
            break
        elif text[i] == " " or text[i] == "\n":
            i += 1
        else:
            j = i
            while j < len(text) and text[j] not in ("<", ">", ",", " ", "\n"):
                j += 1
            result += text[i:j]
            i = j
    return result


def extract_class_info(headers_text, filepath):
    classes = {}
    structs = {}

    binding_pattern = re.compile(r"URGE_BINDING\s*\(([^)]*)\)")
    binding_line_pattern = re.compile(r"^\s*URGE_BINDING")

    lines = headers_text.split("\n")

    i = 0
    while i < len(lines):
        line = lines[i]

        if not binding_line_pattern.match(line):
            i += 1
            continue

        binding_match = binding_pattern.search(line)
        binding_args = binding_match.group(1) if binding_match else ""

        # check if the next part is on the same line
        remaining = line[binding_match.end():].strip()

        # look ahead to find what this URGE_BINDING annotates
        j = i + 1
        combined = remaining
        while j < len(lines) and not combined.strip():
            combined = lines[j].strip()
            j += 1

        if not combined:
            # might be on same line if remaining non-empty
            next_line = remaining if remaining else (
                lines[j].strip() if j < len(lines) else ""
            )
        else:
            next_line = remaining if remaining else (
                lines[j - 1].strip() if j - 1 < len(lines) else ""
            )

        # Determine what comes next
        # class/struct/enum/attribute/method

        # Check for class declaration (possibly multi-line)
        class_match = re.match(r"class\s+(\w+)", next_line)
        struct_match = re.match(r"struct\s+(\w+)", next_line)

        if class_match and not re.match(r"enum\s+class", next_line):
            name = class_match.group(1)
            # look for parent class
            parent = "Object"

            # Merge lines until we find the opening brace
            k = i
            full_decl = ""
            while k < len(lines):
                full_decl += lines[k] + " "
                if "{" in lines[k]:
                    break
                k += 1

            full_decl_single = " ".join(full_decl.split())
            parent_match = re.search(r":\s*public\s+(.+?)\s*\{", full_decl_single)
            if parent_match:
                parent = parse_type(parent_match.group(1).strip())

            # extract class body
            body_start = k
            # Find the brace position
            brace_pos = lines[body_start].find("{")
            body_text = lines[body_start][brace_pos + 1:]
            k += 1
            depth = 1
            while k < len(lines) and depth > 0:
                for ch in lines[k]:
                    if ch == "{":
                        depth += 1
                    elif ch == "}":
                        depth -= 1
                if depth > 0:
                    body_text += "\n" + lines[k]
                k += 1

            # Parse class body
            info = parse_class_body(body_text, filepath, parent)
            classes[name] = info
            i = k
            continue

        elif struct_match:
            name = struct_match.group(1)
            parent = "Object"

            k = i
            full_decl = ""
            while k < len(lines):
                full_decl += lines[k] + " "
                if "{" in lines[k]:
                    break
                k += 1

            full_decl_single = " ".join(full_decl.split())
            parent_match = re.search(r":\s*public\s+(.+?)\s*\{", full_decl_single)
            if parent_match:
                parent = parse_type(parent_match.group(1).strip())

            body_start = k
            brace_pos = lines[body_start].find("{")
            body_text = lines[body_start][brace_pos + 1:]
            k += 1
            depth = 1
            while k < len(lines) and depth > 0:
                for ch in lines[k]:
                    if ch == "{":
                        depth += 1
                    elif ch == "}":
                        depth -= 1
                if depth > 0:
                    body_text += "\n" + lines[k]
                k += 1

            info = parse_struct_body(body_text, filepath, parent)
            structs[name] = info
            i = k
            continue

        elif re.match(r"enum\s+class", next_line):
            # enum class inside a class - handled by parse_class_body
            # but also could be standalone (unlikely as all are inside classes)
            i += 1
            continue

        elif re.match(r"URGE_ATTRIBUTE", next_line):
            # attribute declaration - handled by parse_class_body
            i += 1
            continue

        else:
            # method declaration - handled by parse_class_body
            i += 1
            continue

        i += 1

    return classes, structs


def parse_class_body(body_text, filepath, parent):
    info = {
        "filepath": filepath,
        "parent": parent,
        "members": {},
        "attributes": {},
        "enums": {},
        "callbacks": {},
    }

    body_text = strip_comments(body_text)
    body_text = re.sub(r"#.*", "", body_text)

    i = 0
    while i < len(body_text):
        while i < len(body_text) and body_text[i] in " \t\n\r":
            i += 1
        if i >= len(body_text):
            break

        rem = body_text[i:]
        m = re.match(r"URGE_BINDING\s*\(([^)]*)\)", rem)
        if not m:
            j = body_text.find("\n", i)
            if j == -1:
                break
            i = j + 1
            continue

        binding_args = m.group(1)
        i += m.end()

        while i < len(body_text) and body_text[i] in " \t\n\r":
            i += 1

        rem = body_text[i:]

        attr_declare = re.match(r"URGE_ATTRIBUTE_DECLARE\s*\((\w+)\s*,\s*(.+?)\)\s*;", rem)
        attr_full = re.match(
            r"URGE_ATTRIBUTE\s*\(\s*(\w+)\s*,\s*(.+?)\s*,", rem
        )

        if attr_declare:
            attr_name = attr_declare.group(1)
            attr_type = parse_type(attr_declare.group(2).strip())
            info["attributes"][attr_name] = {"value": attr_type}
            i += attr_declare.end()
            continue

        if attr_full:
            attr_name = attr_full.group(1)
            attr_type_str = attr_full.group(2).strip()
            attr_type = parse_type(attr_type_str)
            info["attributes"][attr_name] = {"value": attr_type}
            j = i + attr_full.end()
            depth = 0
            while j < len(body_text):
                if body_text[j] == "(":
                    depth += 1
                elif body_text[j] == ")":
                    if depth == 0:
                        break
                    depth -= 1
                j += 1
            if j < len(body_text) and body_text[j] == ")":
                j += 1
            i = j
            continue

        enum_match = re.match(r"enum\s+class\s+(\w+)\s*(?::\s*(\w+))?", rem)
        if enum_match:
            enum_name = enum_match.group(1)
            enum_range = enum_match.group(2) or "int32_t"
            i += enum_match.end()

            while i < len(body_text) and body_text[i] != "{":
                i += 1
            brace_start = i
            brace_end = find_matching_brace(body_text, brace_start + 1)
            enum_body = body_text[brace_start + 1:brace_end]

            members = []
            for part in enum_body.split(","):
                part = part.strip()
                eq_idx = part.find("=")
                if eq_idx != -1:
                    members.append(part[:eq_idx].strip())
                elif part:
                    members.append(part)

            info["enums"][enum_name] = {
                "range": enum_range,
                "members": members,
            }
            i = brace_end + 1
            continue

        using_match = re.match(r"using\s+(\w+)\s*=", rem)
        if using_match:
            callback_name = using_match.group(1)
            i += using_match.end()

            lt_pos = body_text.find("<", i)
            if lt_pos == -1:
                continue
            gt_pos = find_matching_angle(body_text, lt_pos + 1)
            template_content = body_text[lt_pos + 1:gt_pos]

            paren_pos = template_content.find("(")
            if paren_pos == -1:
                continue
            ret_type = parse_type(template_content[:paren_pos].strip())
            paren_end = find_matching_paren(template_content, paren_pos + 1)
            params_text = template_content[paren_pos + 1:paren_end]
            params = parse_method_params(params_text)

            info["callbacks"][callback_name] = {
                "return": ret_type,
                "params": params,
            }

            i = gt_pos + 1
            while i < len(body_text) and body_text[i] in " \t\n\r;":
                i += 1
            continue

        static = False
        static_match = re.match(r"static\s+", rem)
        if static_match:
            static = True
            i += static_match.end()
            rem = body_text[i:]

        sig = rem
        paren_pos = sig.find("(")
        if paren_pos == -1:
            nl = body_text.find("\n", i)
            i = nl + 1 if nl != -1 else len(body_text)
            continue

        prefix = sig[:paren_pos].strip()
        parts = prefix.split()
        if len(parts) >= 2:
            method_name = parts[-1]
            if parts[-2] == "operator":
                method_name = "operator" + parts[-1]
                ret_parts = parts[:-2]
            else:
                ret_parts = parts[:-1]
            ret_type = " ".join(ret_parts)
        elif len(parts) == 1:
            method_name = parts[0]
            ret_type = "void"
        else:
            i += 1
            continue

        paren_start = i + paren_pos
        paren_end = find_matching_paren(body_text, paren_start + 1)
        params_text = body_text[paren_start + 1:paren_end]
        params = parse_method_params(params_text)

        method_entry = {
            "static": static,
            "params": params,
            "return": parse_type(ret_type),
        }

        if method_name in info["members"]:
            existing = info["members"][method_name]
            if isinstance(existing, list):
                existing.append(method_entry)
            else:
                info["members"][method_name] = [existing, method_entry]
        else:
            info["members"][method_name] = method_entry

        i = paren_end + 1
        while i < len(body_text) and body_text[i] in " \t\n\r;":
            i += 1

    return info


def parse_struct_body(body_text, filepath, parent):
    info = {
        "filepath": filepath,
        "params": [],
    }

    body_text = strip_comments(body_text)
    body_text = re.sub(r"#.*", "", body_text)

    # Extract fields: Type name [= value];
    # Remove access specifiers
    body_text = re.sub(r"\bpublic\s*:", "", body_text)
    body_text = re.sub(r"\bprivate\s*:", "", body_text)
    body_text = re.sub(r"\bprotected\s*:", "", body_text)

    field_pattern = re.compile(r"(\w[\w:]*(?:\s*<[^>]*>)?)\s+(\w+)\s*(?:=\s*[^;]*)?\s*;")
    for m in field_pattern.finditer(body_text):
        typ = parse_type(m.group(1).strip())
        name = m.group(2)
        info["params"].append({"name": name, "type": typ})

    return info


def parse_method_params(params_text):
    params = []
    if not params_text.strip():
        return params

    # Split by comma but respect nested <> and ()
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
        # Skip URGE_EXCEPTION
        if "URGE_EXCEPTION" in part or "ExceptionState" in part:
            continue
        # Parse "Type name"
        words = part.split()
        if len(words) >= 2:
            name = words[-1]
            typ = " ".join(words[:-1])
            params.append({"name": name, "type": parse_type(typ)})

    return params


def collect_headers(content_dir):
    headers = []
    content_dir = os.path.normpath(content_dir)
    for root, dirs, files in os.walk(content_dir):
        for f in files:
            if f.endswith(".h"):
                full_path = os.path.join(root, f)
                rel_path = os.path.relpath(full_path, content_dir).replace("\\", "/")
                headers.append((full_path, rel_path))
    return headers


def main():
    parser = argparse.ArgumentParser(description="Parse URGE binding headers")
    parser.add_argument("content_dir", help="Path to content directory")
    parser.add_argument("-o", "--output", default=None, help="Output JSON file path")
    args = parser.parse_args()

    content_dir = os.path.abspath(args.content_dir)

    result = {"class": {}, "struct": {}}

    headers = collect_headers(content_dir)

    for full_path, rel_path in headers:
        with open(full_path, "r", encoding="utf-8") as f:
            content = f.read()

        body = extract_namespace_body(content)
        if not body:
            continue

        classes, structs = extract_class_info(body, rel_path)

        for name, info in classes.items():
            result["class"][name] = info

        for name, info in structs.items():
            result["struct"][name] = info

    if args.output:
        output_path = os.path.abspath(args.output)
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        with open(output_path, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2, ensure_ascii=False)
        print(f"Generated IDL JSON: {output_path}")


if __name__ == "__main__":
    main()
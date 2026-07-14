"""
bind_parser.py - Entry point for CMake build system.
Parses C++ headers with URGE_BINDING() annotations and:
  1. Generates IDL JSON        (-o flag)
  2. Generates C++ glue code   (--gen-h / --gen-cc flags)

Usage (matches CMakeLists.txt):
  python bind_parser.py <content_dir> -b <base_dir> -o <output.json>
  python bind_parser.py <content_dir> -b <base_dir> --gen-h <file> --gen-cc <file>
"""

import argparse
import json
import os
import sys

# Import the shared header parser
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import header_parser

# ---------------------------------------------------------------------------
# IDL JSON generation
# ---------------------------------------------------------------------------

def generate_idl_json(content_dir: str, output_path: str) -> dict:
    """Generate IDL JSON using the shared node_parser module."""
    hp = header_parser.HeaderParser()
    result = hp.parse_directory(content_dir)

    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(result, f, indent=2, ensure_ascii=False)

    return result

# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Parse URGE binding headers and generate IDL/glue code"
    )
    parser.add_argument("content_dir", help="Path to content directory")
    parser.add_argument("-b", "--base-dir", default=None, help="Base directory")
    parser.add_argument("-o", "--output", default=None, help="Output IDL JSON file path")
    args = parser.parse_args()

    content_dir = os.path.abspath(args.content_dir)

    # Always parse headers first
    print(f"Parsing headers in: {content_dir}")
    result = generate_idl_json(
        content_dir,
        args.output or os.path.join(os.path.dirname(__file__), "..", "idl", "export.json")
    )

    n_class = len(result.get("class", {}))
    print(f"Parsed {n_class} classes")

    if args.output:
        print(f"IDL JSON written to: {args.output}")


if __name__ == "__main__":
    main()

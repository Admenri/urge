"""Entry point that parses ``webgpu.h`` and emits the C++ wrapper header.

Run from CMake (see ``CMakeLists.txt``) as::

    autogen.py --header <webgpu.h> --output <webgpu_cpp.hpp>

This writes an intermediate ``<output>.json`` IDL dump next to the header and
renders ``webgpu_cpp.hpp`` through ``template.hpp``.
"""

import argparse
import json

from generator import WGPUGenerator
from parser import WGPUParser


def main() -> None:
    argv_parser = argparse.ArgumentParser(description="WebGPU Header Generator")
    argv_parser.add_argument(
        "--header", help="Header for parsing", type=str, required=True
    )
    argv_parser.add_argument(
        "--output", help="Header output path", type=str, required=True
    )
    args = argv_parser.parse_args()

    with open(args.header, "r") as file:
        content = file.read()

    # Parse the C IDL into structured data.
    parse_data = WGPUParser().parse(content)

    # Emit the intermediate JSON IDL (useful for debugging the parser).
    with open(args.output + ".json", "w+") as file:
        file.write(json.dumps(parse_data))

    # Render and write the C++ wrapper header.
    cpp_header = WGPUGenerator().render(parse_data)
    with open(args.output, "w+") as file:
        file.write(cpp_header)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Embed the existing URGE PNG or make a macOS icon without changing its design."""
import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("format", choices=("header", "icns"))
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.format == "header":
        data = args.source.read_bytes()
        lines = [", ".join(f"0x{value:02x}" for value in data[start:start + 16])
                 for start in range(0, len(data), 16)]
        header = ("// Generated from the URGE logo; do not edit.\n#pragma once\n"
                  "inline constexpr unsigned char kUrgeIconPng[] = {\n  " + ",\n  ".join(lines) + "\n};\n")
        if not args.output.exists() or args.output.read_text() != header:
            args.output.write_text(header)
        return
    with tempfile.TemporaryDirectory(prefix="urge-icon-") as folder:
        iconset = Path(folder) / "URGE.iconset"
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                pixels = str(size * scale)
                suffix = "@2x" if scale == 2 else ""
                name = iconset / f"icon_{size}x{size}{suffix}.png"
                subprocess.run(["/usr/bin/sips", "-z", pixels, pixels, str(args.source),
                                "--out", str(name)], check=True, stdout=subprocess.DEVNULL)
        subprocess.run(["/usr/bin/iconutil", "-c", "icns", str(iconset), "-o", str(args.output)], check=True)


if __name__ == "__main__":
    main()

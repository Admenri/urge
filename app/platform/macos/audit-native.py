#!/usr/bin/env python3
"""Reject runtime libraries outside the macOS system installation."""
from pathlib import Path
import argparse
import mmap
import struct
import subprocess


def minimum_versions(path):
    """Read deployment commands from a Mach-O file or every member of an ar.

    Static archives can silently carry newer deployment requirements than the
    final executable. Inspect members directly, including Rust's bundled std.
    """
    with path.open("rb") as file, mmap.mmap(file.fileno(), 0, access=mmap.ACCESS_READ) as data:
        def object_versions(offset, size):
            magic = data[offset:offset + 4]
            if magic not in (b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe"):
                return []  # Archive symbol tables and LLVM bitcode have no Mach-O commands.
            header_size = 32 if magic == b"\xcf\xfa\xed\xfe" else 28
            ncmds = struct.unpack_from("<I", data, offset + 16)[0]
            cursor = offset + header_size
            versions = []
            for _ in range(ncmds):
                command, length = struct.unpack_from("<II", data, cursor)
                if length < 8 or cursor + length > offset + size:
                    raise RuntimeError(f"Invalid Mach-O load command: {path}")
                if command == 0x32:  # LC_BUILD_VERSION
                    platform, version = struct.unpack_from("<II", data, cursor + 8)
                    if platform != 1:
                        raise RuntimeError(f"Non-macOS Mach-O member in {path}")
                    versions.append(version)
                elif command == 0x24:  # LC_VERSION_MIN_MACOSX
                    versions.append(struct.unpack_from("<I", data, cursor + 8)[0])
                cursor += length
            return versions

        if data[:8] != b"!<arch>\n":
            return object_versions(0, len(data))
        cursor, versions = 8, []
        while cursor < len(data):
            header = data[cursor:cursor + 60]
            if len(header) != 60 or header[58:] != b"`\n":
                raise RuntimeError(f"Invalid archive header: {path}")
            size = int(header[48:58])
            name = header[:16].strip()
            name_size = int(name[3:]) if name.startswith(b"#1/") else 0
            versions.extend(object_versions(cursor + 60 + name_size, size - name_size))
            cursor += 60 + size + size % 2
        return versions


def check_minimum(paths, target):
    components = [int(part) for part in target.split(".")]
    components += [0] * (3 - len(components))
    maximum = (components[0] << 16) | (components[1] << 8) | components[2]
    checked = 0
    for path in paths:
        versions = minimum_versions(path)
        if path.suffix != ".a" and not versions:
            raise RuntimeError(f"No macOS deployment version found: {path}")
        newer = [value for value in versions if value > maximum]
        if newer:
            value = max(newer)
            raise RuntimeError(f"{path} requires macOS {value >> 16}.{value >> 8 & 255}.{value & 255}, above {target}")
        checked += len(versions)
    print(f"PASS: {checked} Mach-O deployment records in {len(paths)} files are <= macOS {target}", flush=True)


def audit(binary):
    output = subprocess.check_output(["/usr/bin/otool", "-L", str(binary)], text=True)
    libraries = [line.strip().split(" (", 1)[0] for line in output.splitlines()[1:]]
    unexpected = [path for path in libraries if not path.startswith(("/usr/lib/", "/System/Library/"))]
    if unexpected:
        raise RuntimeError(f"Non-system runtime dependencies in {binary}: {unexpected}")
    print(f"PASS: {Path(binary).name} links only macOS system libraries ({len(libraries)})", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--target")
    parser.add_argument("--archives-root", type=Path, action="append", default=[])
    args = parser.parse_args()
    audit(args.binary)
    if args.target:
        archives = {path.resolve() for root in args.archives_root for path in root.rglob("*.a")}
        check_minimum([args.binary, *sorted(archives)], args.target)

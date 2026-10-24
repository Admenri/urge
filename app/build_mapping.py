# Licensed under the MIT License <http://opensource.org/licenses/MIT>.
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Index a game directory for the Emscripten build.

A browser has no directory to enumerate.  The game, on the other hand, asks for
resources the way RGSS always has -- by a name that may differ from the file on
disk in case and may leave the extension out:

    Bitmap.new("Graphics/A")     ->  Graphics/A.png

On a desktop that is answered by listing the directory (`IOService::OpenRead`).
On the web there is nothing to list until a file has been downloaded, and the
download cannot start before the real name is known.  This script breaks the
circle by walking the game directory at build time and writing the answer down:

    async_mapping.js    const asyncMappingData = {
                          "graphics/a.png": {
                            "path": "Graphics/A.png",
                            "size": 1234,
                            "preload": false
                          },
                          ...
                        };

The key is the lowercased path, which is what makes a lookup case-insensitive;
`path` carries the case the server actually serves.  `app/urge-helper.js` reads
the table and turns a request into a URL, `core/system/filesystem.cc` asks it
before every open.

Files whose names match one of the `--preload` patterns are marked `preload`
and fetched before the game starts instead of on demand: the configuration and
the script archive are needed before the first frame, and an `.acs` container
has to be a real file before PhysicsFS can mount it.

Usage:

    python app/build_mapping.py -i <game dir> -o <out>/async_mapping.js
"""

import argparse
import fnmatch
import json
import os
import pathlib
import posixpath
import sys

# Needed before the game's first frame: the configuration names the script
# archive, and a container has to exist in the filesystem before it is mounted.
DEFAULT_PRELOAD = "*.ini,*.acs,Data/Scripts.*"


def matches(relative_path, pattern):
    """Whether `relative_path` (lowercase, forward slashes) hits `pattern`.

    A pattern without a slash also matches a bare file name at any depth, so
    `*.ini` finds `Game.ini` and `System/Game.ini` alike; a pattern without a
    wildcard matches the directory of that name as a whole.
    """
    pattern = pattern.strip().lower()
    if not pattern:
        return False

    if fnmatch.fnmatchcase(relative_path, pattern):
        return True
    if "/" not in pattern and fnmatch.fnmatchcase(
            posixpath.basename(relative_path), pattern):
        return True
    return relative_path.startswith(pattern.rstrip("/") + "/")


def compile_patterns(value):
    patterns = []
    for part in value.replace(";", ",").split(","):
        part = part.strip()
        if part:
            patterns.append(part.lower())
    return patterns


def collect(root, preload, exclude, skip, verbose):
    """Walk `root` and return the mapping, keyed by the lowercased path."""
    entries = {}
    duplicates = []
    total_bytes = 0

    for item in sorted(root.rglob("*")):
        if not item.is_file():
            continue
        # The manifest is written into the tree it describes often enough to
        # matter -- never index the file being generated.
        if skip is not None and item.resolve() == skip:
            continue

        relative = item.relative_to(root).as_posix()
        lower = relative.lower()

        if any(matches(lower, pattern) for pattern in exclude):
            if verbose:
                print(f"  excluded  {relative}")
            continue

        size = item.stat().st_size
        preloaded = any(matches(lower, pattern) for pattern in preload)

        if lower in entries:
            duplicates.append((relative, entries[lower]["path"]))
            continue

        entries[lower] = {
            "path": relative,
            "size": size,
            "preload": preloaded,
        }
        total_bytes += size

        if verbose:
            print(f"  {'preload ' if preloaded else 'lazy    '} {relative}"
                  f"  {size}")

    for kept, dropped in duplicates:
        print(f"warning: '{kept}' differs only in case from '{dropped}'; "
              f"the first one wins", file=sys.stderr)

    return entries, total_bytes


def render(entries, total_bytes, source):
    """The JavaScript file: a header, then one entry per line.

    One line per file keeps a diff of a rebuilt manifest readable, which is
    worth more than the few bytes a compact dump would save -- the whole point
    of the file is that a human can check what the build indexed.
    """
    preloaded = sum(1 for entry in entries.values() if entry["preload"])
    lines = [
        "// Generated by app/build_mapping.py from "
        f"'{source}'.  Do not edit.",
        f"// {len(entries)} files, {total_bytes} bytes, "
        f"{preloaded} preloaded.",
        "const asyncMappingData = {",
    ]

    for key in sorted(entries):
        entry = entries[key]
        lines.append(
            f"  {json.dumps(key, ensure_ascii=False)}: {{"
            f" \"path\": {json.dumps(entry['path'], ensure_ascii=False)},"
            f" \"size\": {entry['size']},"
            f" \"preload\": {'true' if entry['preload'] else 'false'} }},"
        )

    lines.append("};")
    return "\n".join(lines) + "\n"


def write_if_changed(path, content):
    """Write only when the content differs, so a rebuild does not touch it."""
    try:
        with open(path, "r", encoding="utf-8", newline="") as handle:
            if handle.read() == content:
                return False
    except OSError:
        pass

    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Generate the web build's asset manifest")
    parser.add_argument("-i", "--input", required=True,
                        help="game directory to index")
    parser.add_argument("-o", "--output", required=True,
                        help="manifest to write (async_mapping.js)")
    parser.add_argument("-p", "--preload", default=DEFAULT_PRELOAD,
                        help="comma separated glob patterns loaded before the "
                             f"game starts (default: '{DEFAULT_PRELOAD}')")
    parser.add_argument("-e", "--exclude", default="",
                        help="comma separated glob patterns left out of the "
                             "manifest")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="list every indexed file")
    args = parser.parse_args()

    root = pathlib.Path(args.input)
    if not root.is_dir():
        print(f"error: '{args.input}' is not a directory", file=sys.stderr)
        return 1

    output = pathlib.Path(args.output)
    skip = None
    try:
        skip = output.resolve()
    except OSError:
        pass

    entries, total_bytes = collect(
        root,
        compile_patterns(args.preload),
        compile_patterns(args.exclude),
        skip,
        args.verbose)

    content = render(entries, total_bytes, args.input)
    changed = write_if_changed(output, content)

    state = "wrote" if changed else "unchanged"
    preloaded = sum(1 for entry in entries.values() if entry["preload"])
    print(f"{state} {args.output}: {len(entries)} files, "
          f"{total_bytes / 1048576.0:.2f} MiB, {preloaded} preloaded")
    return 0


if __name__ == "__main__":
    sys.exit(main())

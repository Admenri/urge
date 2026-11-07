# Licensed under the MIT License. SPDX-License-Identifier: MIT
# Copyright (c) 2026 URGE contributors.
"""Pack a directory into an uncompressed, password-protected EDB v1 bundle.

Usage: python tools/build_edb_tool.py <source-dir> <output.edb>
Install dependencies with: python -m pip install -r tools/requirements.txt
The password is prompted for interactively, or read as one UTF-8 line on stdin.
"""

import argparse
from dataclasses import dataclass
import getpass
import hashlib
import os
from pathlib import Path
import stat
import struct
import sys


MAGIC = b"URGEEDB\0"
VERSION = 1
HEADER_SIZE = 64
KEY_SIZE = 32
SALT_SIZE = 16
NONCE_SIZE = 24
TAG_SIZE = 16
CHUNK_SIZE = 65536
MAX_NAME = 4096
MAX_NODES = 1 << 20
MAX_INDEX_SIZE = 128 * 1024 * 1024
KDF_BLOCKS = 32768
KDF_PASSES = 3
MAX_ARCHIVE_SIZE = (1 << 63) - 1


@dataclass(frozen=True)
class BundleFile:
    path: Path
    name: bytes
    size: int
    nonce: bytes
    snapshot: tuple[int, int, int, int]


def file_snapshot(status: os.stat_result) -> tuple[int, int, int, int]:
    return status.st_dev, status.st_ino, status.st_size, status.st_mtime_ns


def validate_name(name: bytes) -> None:
    if (not name or len(name) > MAX_NAME or
            any(byte < 32 for byte in name) or b":" in name or b"\\" in name or
            any(part in (b"", b".", b"..") for part in name.split(b"/"))):
        raise ValueError(f"Invalid bundle path: {name!r}")
    name.decode("utf-8")


def collect_files(root: Path) -> tuple[list[BundleFile], int]:
    files = []
    nodes = {b"": True}
    name_bytes = 0
    index_size = 4
    directories = [root]
    is_junction = getattr(os.path, "isjunction", lambda path: False)
    while directories:
        with os.scandir(directories.pop()) as entries:
            for entry in entries:
                path = Path(entry.path)
                # On Windows DirEntry.stat() can omit device/inode fields;
                # use Path.stat() so the later handle comparison is reliable.
                status = path.stat(follow_symlinks=False)
                if stat.S_ISLNK(status.st_mode) or is_junction(path):
                    raise ValueError(f"Symbolic links and junctions are not supported: {path}")
                if stat.S_ISDIR(status.st_mode):
                    directories.append(path)
                    continue
                if not stat.S_ISREG(status.st_mode):
                    raise ValueError(f"Not a regular file: {path}")
                name = path.relative_to(root).as_posix().encode("utf-8")
                validate_name(name)
                if (len(files) >= MAX_NODES - 1 or
                        len(name) + 44 > MAX_INDEX_SIZE - index_size):
                    raise ValueError("Bundle index exceeds the format limits")
                parts = name.lower().split(b"/")  # Only ASCII bytes are folded.
                for i, part in enumerate(parts):
                    key = b"/".join(parts[:i + 1])
                    directory = i + 1 < len(parts)
                    if key in nodes:
                        if not directory or not nodes[key]:
                            raise ValueError(f"Paths conflict after case folding: {name!r}")
                    else:
                        size = len(key) + len(part)
                        if (len(nodes) >= MAX_NODES or
                                size > MAX_INDEX_SIZE - name_bytes):
                            raise ValueError("Bundle directory tree exceeds the format limits")
                        nodes[key] = directory
                        name_bytes += size
                files.append(BundleFile(path, name, status.st_size,
                                        os.urandom(NONCE_SIZE), file_snapshot(status)))
                index_size += len(name) + 44
    files.sort(key=lambda file: file.name)
    return files, index_size


def build_index(files: list[BundleFile], index_size: int) -> bytes:
    index = bytearray(struct.pack("<I", len(files)))
    offset = 0
    max_payload = MAX_ARCHIVE_SIZE - HEADER_SIZE - index_size - TAG_SIZE
    for file in files:
        chunks = (file.size + CHUNK_SIZE - 1) // CHUNK_SIZE
        stored_size = file.size + chunks * TAG_SIZE
        if file.size < 0 or stored_size > max_payload - offset:
            raise ValueError("Bundle exceeds the maximum archive size")
        index.extend(struct.pack("<I", len(file.name)))
        index.extend(file.name)
        index.extend(struct.pack("<QQ24s", offset, file.size, file.nonce))
        offset += stored_size
    return bytes(index)


def build_bundle(source: Path, output: Path, password: str) -> int:
    """Write a new EDB file and return its file count; never overwrite output."""
    password_bytes = password.encode("utf-8")
    if not 1 <= len(password_bytes) <= MAX_NAME:
        raise ValueError("Password must contain 1-4096 UTF-8 bytes")
    root = Path(source).resolve(strict=True)
    if not root.is_dir():
        raise ValueError(f"Source is not a directory: {root}")
    output = Path(output)
    destination = output.parent.resolve(strict=True) / output.name
    if os.path.lexists(destination):
        raise FileExistsError(f"Output already exists: {destination}")

    # These libraries implement the same Argon2id and XChaCha20-Poly1305
    # primitives as the engine's Monocypher reader. No native build is needed.
    from argon2.low_level import Type, hash_secret_raw
    from nacl.bindings import crypto_aead_xchacha20poly1305_ietf_encrypt

    files, index_size = collect_files(root)
    index = build_index(files, index_size)
    salt = os.urandom(SALT_SIZE)
    index_nonce = os.urandom(NONCE_SIZE)
    header = struct.pack("<8sII16s24sQ", MAGIC, VERSION, CHUNK_SIZE,
                         salt, index_nonce, index_size)
    key = hash_secret_raw(password_bytes, salt, time_cost=KDF_PASSES,
                          memory_cost=KDF_BLOCKS, parallelism=1,
                          hash_len=KEY_SIZE, type=Type.ID, version=19)
    encrypted_index = crypto_aead_xchacha20poly1305_ietf_encrypt(
        index, header, index_nonce, key)

    created = False
    try:
        with destination.open("xb") as out:
            created = True
            out.write(header)
            out.write(encrypted_index)  # Ciphertext followed by its 16-byte tag.
            for file in files:
                current = file.path.stat(follow_symlinks=False)
                if (not stat.S_ISREG(current.st_mode) or
                        file_snapshot(current) != file.snapshot):
                    raise ValueError(f"Source file changed while packing: {file.path}")
                file_key = hashlib.blake2b(file.nonce, key=key, digest_size=KEY_SIZE).digest()
                with file.path.open("rb") as source_file:
                    if file_snapshot(os.fstat(source_file.fileno())) != file.snapshot:
                        raise ValueError(f"Source file changed while packing: {file.path}")
                    remaining = file.size
                    chunk = 0
                    while remaining:
                        size = min(CHUNK_SIZE, remaining)
                        plain = source_file.read(size)
                        if len(plain) != size:
                            raise ValueError(f"Source file was truncated: {file.path}")
                        nonce = struct.pack("<Q", chunk) + bytes(16)
                        associated_data = struct.pack("<QQ", file.size, chunk)
                        out.write(crypto_aead_xchacha20poly1305_ietf_encrypt(
                            plain, associated_data, nonce, file_key))
                        remaining -= size
                        chunk += 1
                    if (source_file.read(1) or
                            file_snapshot(os.fstat(source_file.fileno())) != file.snapshot):
                        raise ValueError(f"Source file changed while packing: {file.path}")
    except BaseException:
        if created:
            destination.unlink(missing_ok=True)
        raise
    return len(files)


def read_password() -> str:
    if sys.stdin.isatty():
        return getpass.getpass("EDB password: ")
    line = sys.stdin.buffer.readline(MAX_NAME + 2)
    return line.removesuffix(b"\n").removesuffix(b"\r").decode("utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="directory to pack")
    parser.add_argument("output", type=Path, help="new EDB output file")
    args = parser.parse_args()
    try:
        count = build_bundle(args.source, args.output, read_password())
    except ModuleNotFoundError as error:
        print(f"Missing dependency: {error.name}. "
              "Run: python -m pip install -r tools/requirements.txt", file=sys.stderr)
        return 1
    except (OSError, ValueError) as error:
        print(f"Cannot build EDB: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        return 130
    print(f"Packed {count} files into {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

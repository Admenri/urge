# EDB bundles, revision 1

EDB is a read-only PhysicsFS archive, registered in every build as `EDB`.
Files are encrypted and packed without compression. Mounting decrypts only the
index; reads authenticate and decrypt 64 KiB chunks on demand. Each open stream
caches one chunk and supports arbitrary seeks and independent duplicates.

The packer is `tools/build_edb_tool.py` and requires Python 3.10 or later.
Install its dependencies and run it from the repository root:

```sh
python -m pip install -r tools/requirements.txt
python tools/build_edb_tool.py <source-dir> <output.edb>
```

The tool prompts for a non-empty password without echoing it. For automated
use, supply the password as one UTF-8 line on standard input, followed by a
newline. The tool uses `argon2-cffi` and `PyNaCl`; no CMake build is needed.
Existing output files, symbolic links and invalid paths are rejected. Empty
directories are omitted. Keep the password available to your application.

```ruby
URGE.set_bundle_password("your password")
raise "mount failed" unless URGE.add_load_path("assets.edb", "Assets", false)
# Resources are available through the engine as Assets/<bundled-path>.
URGE.set_bundle_password("") # Prevents subsequent EDB mounts until set again.
raise "unmount failed" unless URGE.remove_load_path("assets.edb")
```

Both load-path methods return a boolean. An empty mount string names the VFS
root. `append=true` places the path behind existing paths, `false` gives it
priority. Unmounting fails while any file in that archive remains open.
Passwords apply to subsequent mounts; mounted bundles retain their derived
keys, allowing several bundles with different passwords at the same time.
This password protects archive data; distributing the password in application
scripts lets anyone who can inspect those scripts decrypt the bundle.

All integers are unsigned little-endian. The 64-byte header contains:

| Offset | Bytes | Value |
| --- | --- | --- |
| 0 | 8 | `URGEEDB\0` |
| 8 | 4 | Version, 1 |
| 12 | 4 | Chunk size, 65536 |
| 16 | 16 | Random Argon2id salt |
| 32 | 24 | Random index nonce |
| 56 | 8 | Plaintext index size |

Argon2id v1.3 derives a 32-byte key from the password using 32768 KiB of memory,
three passes and one lane. The header is associated data for an
XChaCha20-Poly1305 encrypted index followed by its 16-byte authentication tag.
The plaintext index starts with a 4-byte file count. Each file record is:

`name_length:u32 | UTF-8 name | payload_offset:u64 | plaintext_size:u64 | file_nonce:24`

Offsets are relative to the first payload byte following the index tag.
Payloads are contiguous in index order, with no padding. Every file uses a
32-byte BLAKE2b subkey keyed by the archive key over its random 24-byte nonce.
Its payload consists of ciphertext chunks, each followed by a 16-byte tag.
The final chunk may be shorter; an empty file has no payload or tags.
Chunk nonces are `chunk_number:u64 | zero:16`; associated data is
`plaintext_size:u64 | chunk_number:u64`. Names are canonical relative UTF-8
paths, with ASCII-insensitive lookup. Traversal, duplicate paths, file/directory
conflicts, overlapping offsets, truncation and trailing data are rejected.
Index size is limited to 128 MiB, paths to 4096 bytes and the tree to 2^20 nodes.

The private ACS reader is available only during engine startup. Startup mounts
it through a one-use bootstrap reader and then deregisters `acs`. The mounted
bootstrap reader retains its file callbacks, but its archive-opening callback
is closed, so later load-path API calls cannot open ACS under any extension.
The loading window closes even when there is no startup ACS file.


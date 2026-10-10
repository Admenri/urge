"""Rewrite a MinGW import library so MSVC can link the interpreter it describes.

Reads the `*.dll.a` that `dlltool` produced for the CRuby in bind/external and
writes an equivalent archive that only describes the interpreter's own API:

    python filter_import_lib.py <in.dll.a> <out.lib> [--list]

Why this is needed
------------------
A MinGW-built DLL does not export only its own symbols.  `ld` also re-exports
the C runtime functions the DLL itself calls, so this archive carries members
for `round`, `fclose`, `getenv`, `strerror` and `tgamma` -- each one a real
definition, not just an import stub.  MSVC links its own C runtime next to it,
finds `round` defined in both `libucrt.lib` and here, and stops:

    libucrt.lib(round.obj) : error LNK2005: round already defined in ...

`/FORCE:MULTIPLE` would silence that, but it also decides the argument by
whichever library the linker happened to search first -- and the archive is
searched before the CRT, so the engine would end up calling the interpreter's
copy of `getenv` and `fclose` rather than its own.  That is a real behavioural
change, not a cosmetic one, so the members are removed instead.

What survives
-------------
Ruby's C API (`rb_*`, `ruby_*`, `rbimpl_*`, `RUBY_*`), what a statically
linked extension calls (`Init_*`), the regex engine's tables (`Onig*`), and the
import-library plumbing itself (`__nm_*`, `_head_*`, `*_iname`, the import
descriptor and its null thunk).  Everything else in this archive is an artifact
of the MinGW build; the linker reports it immediately if a dropped member turns
out to be needed after all.
"""

import struct
import sys

KEEP_PREFIXES = ("rb_", "rbimpl_", "rb", "ruby", "Init_", "Onig", "RUBY_")
KEEP_PATTERNS = ("__nm_", "_head_", "__IMPORT_DESCRIPTOR_",
                 "__NULL_IMPORT_DESCRIPTOR")
KEEP_SUFFIXES = ("_iname", "_NULL_THUNK_DATA")

AR_MAGIC = b"!<arch>\n"
AR_HEADER_SIZE = 60
COFF_MACHINE_AMD64 = 0x8664
COFF_SYMBOL_SIZE = 18
COFF_CLASS_EXTERNAL = 2


def keep_symbol(name):
    return (name.startswith(KEEP_PREFIXES) or name.startswith(KEEP_PATTERNS)
            or name.endswith(KEEP_SUFFIXES))


def parse_ar(data):
    """Split a GNU ar archive into its members, keeping each name field raw.

    The name field is kept verbatim rather than resolved through the `//` long
    name table: it is copied back out unchanged, and only the symbol index --
    the `/` member -- has to be rebuilt.
    """
    if data[:len(AR_MAGIC)] != AR_MAGIC:
        raise ValueError("not a GNU ar archive")
    members = []
    offset = len(AR_MAGIC)
    while offset + AR_HEADER_SIZE <= len(data):
        header = data[offset:offset + AR_HEADER_SIZE]
        if header[58:60] != b"\x60\x0a":
            break
        size = int(header[48:58].decode("latin1").strip())
        members.append({
            "field": header[0:16],
            "payload": data[offset + AR_HEADER_SIZE:offset + AR_HEADER_SIZE + size],
        })
        offset += AR_HEADER_SIZE + size + (size & 1)
    return members


def coff_defined_externals(payload):
    """The external symbols a COFF object defines, as opposed to imports."""
    if len(payload) < 20:
        return []
    machine, _sections = struct.unpack_from("<HH", payload, 0)
    if machine != COFF_MACHINE_AMD64:
        return []
    symbol_table, count = struct.unpack_from("<II", payload, 8)
    if not symbol_table or not count:
        return []
    if symbol_table + count * COFF_SYMBOL_SIZE > len(payload):
        return []
    strings = symbol_table + count * COFF_SYMBOL_SIZE
    names = []
    for index in range(count):
        entry = symbol_table + index * COFF_SYMBOL_SIZE
        raw = payload[entry:entry + 8]
        section, _type, storage, _aux = struct.unpack_from("<hHBB", payload,
                                                           entry + 12)
        if storage != COFF_CLASS_EXTERNAL or section == 0:
            continue
        if raw[:4] == b"\x00\x00\x00\x00":
            # A name longer than eight bytes lives in the object's string table.
            name_offset = struct.unpack_from("<I", payload, entry + 4)[0]
            end = payload.index(b"\x00", strings + name_offset)
            names.append(payload[strings + name_offset:end].decode("latin1"))
        else:
            names.append(raw.split(b"\x00")[0].decode("latin1"))
    return names


def member_header(name_field, size):
    return (name_field.ljust(16) + b"0".ljust(12) + b"0".ljust(6) +
            b"0".ljust(6) + b"100644".ljust(8) + str(size).encode().ljust(10) +
            b"\x60\x0a")


def build_archive(keep, long_table):
    """Lay the surviving members out again behind a freshly built symbol index.

    The index is the archive's `/` member: a big-endian symbol count, one
    big-endian file offset per symbol, then the NUL-terminated names.  It has
    to know where each member lands, so its own size -- which depends only on
    the number of symbols -- is worked out first.
    """
    entries = sorted((symbol, index)
                     for index, (_field, _payload, symbols) in enumerate(keep)
                     for symbol in symbols)

    def make_index(offsets=None):
        index = struct.pack(">I", len(entries))
        index += b"".join(struct.pack(">I", (offsets or [0] * len(entries))[i])
                          for _symbol, i in entries)
        index += b"".join(symbol.encode("latin1") + b"\x00"
                          for symbol, _i in entries)
        if len(index) & 1:
            index += b"\n"
        return index

    prefix = len(AR_MAGIC) + AR_HEADER_SIZE + len(make_index())
    if long_table:
        prefix += AR_HEADER_SIZE + len(long_table) + (len(long_table) & 1)

    offsets = []
    position = prefix
    for _field, payload, _symbols in keep:
        offsets.append(position)
        position += AR_HEADER_SIZE + len(payload) + (len(payload) & 1)

    out = bytearray(AR_MAGIC)
    index = make_index(offsets)
    out += member_header(b"/", len(index)) + index
    if long_table:
        out += member_header(b"//", len(long_table)) + long_table
        if len(long_table) & 1:
            out += b"\n"
    for field, payload, _symbols in keep:
        out += member_header(field, len(payload)) + payload
        if len(payload) & 1:
            out += b"\n"
    return bytes(out)


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2

    source, destination = sys.argv[1], sys.argv[2]
    listing = "--list" in sys.argv

    members = parse_ar(open(source, "rb").read())

    keep, drop, long_table = [], [], b""
    for member in members:
        field = member["field"].decode("latin1").rstrip()
        if field == "/":            # the symbol index, rebuilt below
            continue
        if field == "//":           # the long name table, carried over as is
            long_table = member["payload"]
            continue
        symbols = coff_defined_externals(member["payload"])
        exported = [s for s in symbols if not s.startswith("__imp_")]
        if exported and not any(keep_symbol(s) for s in exported):
            drop.append(exported)
            continue
        keep.append((member["field"], member["payload"], symbols))

    if listing:
        print("members kept   :", len(keep))
        print("members dropped:", len(drop))
        for exported in drop:
            print("   drop", exported)

    open(destination, "wb").write(build_archive(keep, long_table))
    print("wrote %s (%d members, %d dropped)" % (destination, len(keep),
                                                 len(drop)))
    return 0


if __name__ == "__main__":
    sys.exit(main())

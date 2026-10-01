#!/usr/bin/env python3
"""Read the URGE binding IR: serve the HTML reference, or query it from a script.

``bind/api_reference.json`` is produced by ``bind/gen_api_json.py`` and consumed
by ``bind/generate_binding.py``.  This tool is the third consumer.  Its job is to
answer "what does the Ruby side of URGE actually expose?" without making the
reader open a single C++ header.

Commands
--------

    serve    start a local HTTP server over ``bind/`` and open the HTML viewer
    embed    inline the IR into ``viewer/api_embed.js`` so ``index.html`` works
             straight from ``file://`` (no server, no CORS)
    list     list the classes and modules
    show     print one class, module or member
    search   find members by name, signature or type
    stats    print summary counts

Target syntax
-------------

    Sprite              a class or module
    Sprite#bitmap       an instance method or attribute
    Sprite.bitmap       a class method or class attribute
    Sprite#bitmap=      a writer (``#`` for instance, ``.`` for class side)
    Input.press?        module functions are class-side by the same rule

Exit status is 0 on success, 1 when the target is unknown, and 2 on a usage
error, so ``search`` can be wired into other scripts.
"""

from __future__ import annotations

import argparse
import http.server
import json
import re
import socketserver
import sys
import webbrowser
from pathlib import Path

VIEWER_DIR = Path(__file__).resolve().parent
BIND_DIR = VIEWER_DIR.parent
IR_PATH = BIND_DIR / "api_reference.json"
EMBED_PATH = VIEWER_DIR / "api_embed.js"

# Sides of the API surface, in the order a reader usually wants them.
KIND_LABELS = {
    "new": "constructor",
    "index": "index operator",
    "instance": "instance method",
    "attribute": "attribute",
    "data_attribute": "data attribute",
    "class": "class method",
    "class_attribute": "class attribute",
    "module_function": "module function",
    "module_attribute": "module attribute",
    "marshal": "marshal protocol",
    "initialize_copy": "copy protocol",
    "unsupported": "unsupported",
}

# Kinds reachable with a ``.`` receiver rather than ``#``.
CLASS_SIDE_KINDS = frozenset(
    ("new", "class", "class_attribute", "module_function", "module_attribute", "marshal")
)

# The order the detail pane renders sections in.
SECTION_ORDER = [
    "new",
    "index",
    "instance",
    "attribute",
    "data_attribute",
    "class",
    "class_attribute",
    "module_function",
    "module_attribute",
    "marshal",
    "initialize_copy",
    "unsupported",
]


# --------------------------------------------------------------------------- #
# Loading
# --------------------------------------------------------------------------- #


def load_ir(path: Path | None = None) -> dict:
    """Read ``api_reference.json``, or die with a pointer to the generator."""
    path = Path(path) if path else IR_PATH
    try:
        with open(path, encoding="utf-8") as handle:
            return json.load(handle)
    except FileNotFoundError:
        sys.exit(
            "error: %s not found.\n"
            "Run: python bind/gen_api_json.py" % path
        )
    except json.JSONDecodeError as exc:
        sys.exit("error: %s is not valid JSON: %s" % (path, exc))


def owners(ir: dict) -> list[dict]:
    """Every class and module, classes first (the IR already orders them)."""
    return list(ir.get("classes", [])) + list(ir.get("modules", []))


def find_owner(ir: dict, name: str) -> dict | None:
    lowered = name.lower()
    for entry in owners(ir):
        if entry["name"].lower() == lowered:
            return entry
    return None


# --------------------------------------------------------------------------- #
# Normalising the IR into a flat member list
# --------------------------------------------------------------------------- #


_FLOAT_LITERAL_RE = re.compile(r"^(-?(?:\d+\.\d*|\d*\.\d+|\d+))f$")


def ruby_default(param: dict) -> str | None:
    """Translate a C++ default into what a script author would actually write.

    The IR records the C++ spelling (``nullptr``, ``{}``, ``255.f``), which is
    noise for a Ruby reader: ``RefPtr<Color> color = {}`` is ``color = nil``.
    Only the handful of shapes the headers use are mapped; anything else is
    passed through untouched so an unmapped default stays visible rather than
    being silently mangled.
    """
    default = param.get("default")
    if default is None or default == "":
        return None
    cpp_type = param.get("type", "")
    if default in ("nullptr", "NULL"):
        return "nil"
    if default in ("{}", "{ }"):
        if "vector" in cpp_type:
            return "[]"
        if "string" in cpp_type:
            return '""'
        if cpp_type.startswith("RefPtr"):
            return "nil"
        return "{}"
    if default in ("std::string()", "std::string{}"):
        return '""'
    if default == "true":
        return "true"
    if default == "false":
        return "false"
    match = _FLOAT_LITERAL_RE.match(default)
    if match:
        literal = match.group(1)
        return literal + "0" if literal.endswith(".") else literal
    return default


def _sig_param(param: dict) -> str:
    """``x`` / ``x = 0`` -- the Ruby-ish spelling of one parameter."""
    name = param.get("name", "?")
    default = ruby_default(param)
    if default is None:
        return name
    return "%s = %s" % (name, default)


def _param_list(params: list[dict]) -> str:
    return ", ".join(_sig_param(p) for p in params)


def _member(
    entry: dict,
    kind: str,
    ruby_name: str,
    *,
    cpp_name: str = "",
    params: list[dict] | None = None,
    returns: dict | None = None,
    setter: str | None = None,
    cpp_type: str = "",
    flags: list[str] | None = None,
    export: str = "",
    note: str = "",
) -> dict:
    """One callable/attribute, flattened into the shape the viewer renders."""
    owner = entry["name"]
    class_side = kind in CLASS_SIDE_KINDS
    separator = "." if class_side else "#"
    # ``setter`` arrives as ``bitmap=``; the idiomatic Ruby spelling puts spaces
    # around the ``=``, so drop the trailing one before rebuilding the call.
    setter_name = setter[:-1] if setter and setter.endswith("=") else setter
    return {
        "owner": owner,
        "owner_kind": entry.get("kind", "class"),
        "kind": kind,
        "kind_label": KIND_LABELS.get(kind, kind),
        "ruby_name": ruby_name,
        "cpp_name": cpp_name,
        "params": params or [],
        "return": returns,
        "setter": setter,
        # Attributes read with `#name` and write with `#name = value`; spelled
        # out here so callers never have to reassemble the pairing.
        "writer": ("%s%s%s = value" % (owner, separator, setter_name)) if setter else None,
        "cpp_type": cpp_type,
        "flags": flags or [],
        "export": export or entry.get("export", ""),
        "note": note,
        "class_side": class_side,
        "call": "%s%s%s" % (owner, separator, ruby_name),
    }


def _method_member(entry: dict, method: dict, kind: str) -> dict:
    flags = []
    if method.get("header_annotation"):
        flags.append("annotation")
    if method.get("override"):
        flags.append("override")
    return _member(
        entry,
        kind,
        method["ruby_name"],
        cpp_name=method.get("cpp_name", ""),
        params=method.get("params", []),
        returns=method.get("return"),
        flags=flags,
        export=method.get("export", ""),
    )


def _attr_member(entry: dict, attr: dict, kind: str, ruby_type: str = "") -> dict:
    """An ATTR(): a reader taking no arguments plus a ``name = value`` writer."""
    flags = []
    if attr.get("override"):
        flags.append("override")
    if attr.get("header_annotation"):
        flags.append("annotation")
    if attr.get("static"):
        flags.append("static")
    resolved_type = ruby_type or attr.get("ruby_type", "")
    return _member(
        entry,
        kind,
        attr["name"],
        cpp_name=attr.get("cpp_name", ""),
        params=[],
        returns={"ruby_type": resolved_type, "cpp_type": attr.get("cpp_type", "")},
        setter=attr.get("setter"),
        cpp_type=attr.get("cpp_type", ""),
        flags=flags,
        export=attr.get("export", ""),
    )


def iter_members(entry: dict) -> list[dict]:
    """Every member the IR says this class/module exposes, in rendering order."""
    found: list[dict] = []

    if entry.get("kind") == "module":
        # Modules store ATTR() as a *pair* of flat functions rather than the
        # nested ``attributes`` list classes use: the reader carries ``attr``,
        # the writer additionally carries ``setter``.  Fold them back together
        # so a module attribute reads like a class one.
        functions = entry.get("functions", [])
        writers = {f["ruby_name"]: f for f in functions if f.get("setter")}
        for func in functions:
            if func.get("setter"):
                continue
            if func.get("attr"):
                name = func["ruby_name"]
                found.append(
                    _member(
                        entry,
                        "module_attribute",
                        name,
                        cpp_name=func.get("cpp_name", ""),
                        params=[],
                        returns=func.get("return"),
                        setter=(name + "=") if writers.get(name + "=") else None,
                        cpp_type=func.get("attr_value_type", ""),
                        export=func.get("export", ""),
                    )
                )
            else:
                found.append(_method_member(entry, func, "module_function"))
        found.sort(key=lambda m: SECTION_ORDER.index(m["kind"]))
        return found

    for ctor in entry.get("constructors", []):
        found.append(
            _member(
                entry,
                "new",
                "new",
                params=ctor.get("params", []),
                # The Ruby return is the class itself; the C++ side is the
                # constructor symbol, shown in the C++ column rather than
                # repeated inside the return type.
                returns={"ruby_type": entry["name"]},
                cpp_name="%s::%s" % (entry.get("cpp_name", entry["name"]), entry.get("cpp_name", entry["name"])),
                export=ctor.get("export", ""),
                note="allocates and calls #initialize",
            )
        )

    index = entry.get("index")
    if index:
        for side, role in (("get", "reader"), ("set", "writer")):
            node = index.get(side)
            if not node:
                continue
            found.append(
                _member(
                    entry,
                    "index",
                    node["ruby_name"],
                    cpp_name=node.get("cpp_name", ""),
                    params=node.get("params", []),
                    export=node.get("export", ""),
                    note=role,
                )
            )

    for method in entry.get("instance_methods", []):
        found.append(_method_member(entry, method, "instance"))
    for attr in entry.get("attributes", []):
        found.append(_attr_member(entry, attr, "attribute"))
    for attr in entry.get("data_attributes", []):
        found.append(_attr_member(entry, attr, "data_attribute"))
    for method in entry.get("class_methods", []):
        found.append(_method_member(entry, method, "class"))
    for attr in entry.get("class_attributes", []):
        found.append(_attr_member(entry, attr, "class_attribute"))

    marshal = entry.get("marshal") or {}
    if marshal.get("dump") and marshal.get("load"):
        found.append(
            _member(
                entry,
                "marshal",
                "_load",
                params=[{"name": "data", "type": "String"}],
                returns={"cpp_type": "", "ruby_type": entry["name"]},
                export=entry.get("export", ""),
                note="class method, restores the object from #_dump",
            )
        )

    copy = entry.get("initialize_copy")
    if copy:
        found.append(
            _member(
                entry,
                "initialize_copy",
                "initialize_copy",
                params=copy.get("params", []),
                export=copy.get("export", ""),
                note="private, driven by #dup and #clone",
            )
        )

    if marshal.get("dump") and marshal.get("load"):
        dumped = _member(
            entry,
            "marshal",
            "_dump",
            params=[{"name": "limit", "type": "Integer"}],
            returns={"cpp_type": "", "ruby_type": "String"},
            export=entry.get("export", ""),
            note="private, driven by Marshal.dump",
        )
        found.append(dumped)

    for node in entry.get("unsupported", []):
        signature = node.get("signature", "?")
        skipped = _member(
            entry,
            "unsupported",
            signature,
            export=entry.get("export", ""),
            note=node.get("reason", ""),
        )
        # The IR stores the C++ spelling; present it as written rather than
        # prefixing it with a Ruby receiver it never got.
        skipped["call"] = "%s %s" % (entry["name"], signature)
        found.append(skipped)

    found.sort(key=lambda m: SECTION_ORDER.index(m["kind"]))
    return found


# Kinds whose Ruby spelling carries no argument list.
_BARE_KINDS = frozenset(
    ("attribute", "data_attribute", "class_attribute", "module_attribute")
)


def format_signature(member: dict) -> str:
    """``Sprite#bitmap``, ``Sprite#update()``, ``Font.default_bold``."""
    if member["kind"] == "unsupported":
        return member["ruby_name"]
    if member["kind"] in _BARE_KINDS:
        return member["call"]
    return "%s(%s)" % (member["call"], _param_list(member["params"]))


def format_writer(member: dict) -> str | None:
    """``Sprite#bitmap = value``, or None for anything that is not an ATTR()."""
    return member.get("writer")


def format_returns(member: dict) -> str:
    returns = member.get("return")
    if not returns:
        return "--"
    ruby = returns.get("ruby_type") or "--"
    cpp = returns.get("cpp_type")
    return "%s  (%s)" % (ruby, cpp) if cpp and cpp != "void" else ruby


# --------------------------------------------------------------------------- #
# Lookup
# --------------------------------------------------------------------------- #


TARGET_RE = re.compile(r"^(?P<owner>[A-Za-z_][\w]*)(?:(?P<sep>[#.])(?P<member>[^\s]+))?$")


def resolve(ir: dict, target: str) -> tuple[dict, dict | None]:
    """Split ``Sprite#bitmap`` into (owner entry, member record or None)."""
    match = TARGET_RE.match(target.strip())
    if not match:
        sys.exit("error: cannot parse target %r" % target)
    entry = find_owner(ir, match.group("owner"))
    if entry is None:
        sys.exit("error: no class or module named %r" % match.group("owner"))

    member_name = match.group("member")
    if member_name is None:
        return entry, None

    want_class_side = match.group("sep") == "."
    found = [m for m in iter_members(entry) if m["kind"] != "unsupported"]

    # 1. Exact name first: ``[]=`` and ``bitmap=`` are literal Ruby names, so a
    #    trailing ``=`` must not be mistaken for a suffix before this is tried.
    for member in found:
        if member["ruby_name"] == member_name and member["class_side"] == want_class_side:
            return entry, member
    for member in found:
        if member["ruby_name"] == member_name:
            return entry, member

    # 2. ``Sprite#bitmap=`` names the writer of the ATTR() whose reader is
    #    ``bitmap``; both live on one record, so hand back the record itself.
    if member_name.endswith("="):
        bare = member_name[:-1]
        for member in found:
            if member["ruby_name"] == bare and member["class_side"] == want_class_side:
                return entry, member
        for member in found:
            if member["ruby_name"] == bare:
                return entry, member

    return entry, None


def search(ir: dict, query: str, limit: int = 40) -> list[dict]:
    """Rank members by how well they match ``query``.

    Scores: exact name 0, prefix 1, substring 2, ``owner#name`` substring 3,
    parameter/type hit 4.  Ties break on the call form so output is stable.
    """
    needle = query.strip().lower()
    if not needle:
        return []

    hits: list[tuple[int, str, dict]] = []
    for entry in owners(ir):
        for member in iter_members(entry):
            returns = member.get("return") or {}
            hay = "%s %s %s %s" % (
                member["call"],
                member["cpp_name"],
                returns.get("ruby_type") or "",
                " ".join(p.get("name", "") + " " + p.get("type", "") for p in member["params"]),
            )
            name = member["ruby_name"].lower()
            call = member["call"].lower()
            if name == needle:
                rank = 0
            elif name.startswith(needle):
                rank = 1
            elif needle in call:
                rank = 2
            elif needle in hay:
                rank = 4
            else:
                continue
            hits.append((rank, member["call"], member))

    hits.sort(key=lambda item: (item[0], item[1]))
    return [member for _, _, member in hits[:limit]]


# --------------------------------------------------------------------------- #
# Plain-text rendering
# --------------------------------------------------------------------------- #


def render_owner(entry: dict) -> str:
    lines = ["%s %s" % (entry.get("kind", "class"), entry["name"])]
    detail = []
    if entry.get("ruby_superclass"):
        detail.append("superclass %s" % entry["ruby_superclass"])
    if entry.get("cpp_parent"):
        detail.append("C++ base %s" % entry["cpp_parent"])
    if entry.get("header"):
        detail.append(entry["header"])
    if detail:
        lines.append("  " + " | ".join(detail))

    badges = []
    if entry.get("glue"):
        badges.append("hand-written glue")
    if (entry.get("marshal") or {}).get("dump"):
        badges.append("marshalable")
    if entry.get("index"):
        badges.append("indexed")
    if entry.get("unsupported"):
        badges.append("%d unsupported" % len(entry["unsupported"]))
    if badges:
        lines.append("  " + " | ".join(badges))

    members = iter_members(entry)
    live = [m for m in members if m["kind"] != "unsupported"]
    lines.append("")
    # Every member of a class shares that class's export range, so it is
    # stated once above rather than repeated on each row.
    lines.append("%-16s %-46s %s" % ("KIND", "SIGNATURE", "RETURNS"))
    lines.append("-" * 92)
    for member in members:
        lines.append(
            "%-16s %-46s %s"
            % (member["kind_label"], format_signature(member), format_returns(member))
        )
    lines.append("")
    lines.append("%d members (%d exported, %d skipped)"
                 % (len(members), len(live), len(members) - len(live)))
    return "\n".join(lines)


def render_member(entry: dict, member: dict) -> str:
    lines = [format_signature(member)]
    writer = format_writer(member)
    if writer:
        lines.append("%s   # same ATTR(), the writer side" % writer)
    lines.append("")
    lines.append("kind      %s" % member["kind_label"])
    lines.append("owner     %s %s" % (entry.get("kind", "class"), entry["name"]))
    if member["cpp_name"]:
        lines.append("C++       %s" % member["cpp_name"])
    lines.append("returns   %s" % format_returns(member))
    if member["params"]:
        lines.append("params")
        for param in member["params"]:
            default = ruby_default(param)
            suffix = " = %s" % default if default is not None else ""
            lines.append("  %-12s %s%s" % (param.get("name", "?"), param.get("type", "?"), suffix))
    if member["flags"]:
        lines.append("flags     %s" % ", ".join(member["flags"]))
    if member["cpp_type"]:
        lines.append("C++ type  %s" % member["cpp_type"])
    if member["class_side"] and member["kind"] != "unsupported":
        lines.append("receiver  class side (a . call)")
    if member["export"]:
        lines.append("source    %s" % member["export"])
    if member["note"]:
        lines.append("note      %s" % member["note"])
    return "\n".join(lines)


# --------------------------------------------------------------------------- #
# Commands
# --------------------------------------------------------------------------- #


def cmd_serve(args: argparse.Namespace) -> int:
    """Serve ``bind/`` over loopback so the viewer can fetch a fresh IR.

    Only ever bound to 127.0.0.1 -- this exposes source headers and has no auth.
    """
    ir = load_ir(args.ir)

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *positional, **keywords):
            # Serve from bind/ so both /viewer/index.html and
            # /api_reference.json resolve, letting the page fetch a live IR.
            super().__init__(*positional, directory=str(BIND_DIR), **keywords)

        def end_headers(self):
            # The whole point is reflecting the current IR, so never cache.
            self.send_header("Cache-Control", "no-store, must-revalidate")
            super().end_headers()

        def log_message(self, fmt, *values):  # keep the console readable
            if "api_reference.json" in (fmt % values):
                return
            sys.stderr.write("  %s\n" % (fmt % values))

    class Server(socketserver.TCPServer):
        allow_reuse_address = True

    port = args.port
    for candidate in range(port, port + 10):
        try:
            server = Server(("127.0.0.1", candidate), Handler)
            break
        except OSError:
            continue
    else:
        sys.exit("error: no free port in %d-%d" % (port, port + 9))

    url = "http://127.0.0.1:%d/viewer/index.html" % candidate
    meta = ir.get("meta", {})
    print("URGE binding viewer")
    print("  IR         %s" % (args.ir or IR_PATH))
    print("  generated  %s" % meta.get("generated_at", "--"))
    print("  classes    %d   modules %d" % (len(ir.get("classes", [])), len(ir.get("modules", []))))
    print("  serving    %s" % BIND_DIR)
    print("  open       %s" % url)
    print("  stop       Ctrl-C")

    if not args.no_open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        server.server_close()
    return 0


def cmd_embed(args: argparse.Namespace) -> int:
    """Inline the IR so ``index.html`` works from ``file://``."""
    ir = load_ir(args.ir)
    payload = json.dumps(ir, ensure_ascii=False, separators=(",", ":"))
    out = Path(args.out) if args.out else EMBED_PATH
    body = (
        "/* Generated by bind/viewer/viewer.py -- do not edit.\n"
        " * Regenerate after bind/gen_api_json.py:  python bind/viewer/viewer.py embed\n"
        " * This is only the file:// fallback; over HTTP index.html fetches\n"
        " * api_reference.json directly and ignores this file.\n"
        " */\n"
        "window.__URGE_API__ = %s;\n" % payload
    )
    with open(out, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(body)
    print("wrote %s (%d bytes, IR generated %s)"
          % (out, len(body.encode("utf-8")), ir.get("meta", {}).get("generated_at", "--")))
    print("index.html now works by double-clicking; re-run after regenerating the IR.")
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    ir = load_ir(args.ir)
    if args.json:
        print(json.dumps(
            [
                {
                    "name": e["name"],
                    "kind": e.get("kind"),
                    "superclass": e.get("ruby_superclass") or e.get("cpp_parent"),
                    "header": e.get("header"),
                    "members": len(iter_members(e)),
                }
                for e in owners(ir)
            ],
            ensure_ascii=False,
            indent=2,
        ))
        return 0

    for entry in owners(ir):
        members = iter_members(entry)
        line = "%-8s %-12s" % (entry.get("kind", "class"), entry["name"])
        parent = entry.get("ruby_superclass") or entry.get("cpp_parent")
        if parent:
            line += " < %-10s" % parent
        line += " %3d members  %s" % (len(members), entry.get("header", ""))
        print(line)
    print()
    print("%d classes, %d modules" % (len(ir.get("classes", [])), len(ir.get("modules", []))))
    return 0


def cmd_show(args: argparse.Namespace) -> int:
    ir = load_ir(args.ir)
    entry, member = resolve(ir, args.target)
    if member is None:
        if "#" in args.target or "." in args.target:
            print("no member matched %r; members of %s:" % (args.target, entry["name"]))
            print(render_owner(entry))
            return 1
        if args.json:
            print(json.dumps(
                {"class": entry, "members": iter_members(entry)}, ensure_ascii=False, indent=2
            ))
        else:
            print(render_owner(entry))
        return 0
    if args.json:
        print(json.dumps({"class": {k: v for k, v in entry.items() if k != "members"},
                          "member": member}, ensure_ascii=False, indent=2))
    else:
        print(render_member(entry, member))
    return 0


def cmd_search(args: argparse.Namespace) -> int:
    ir = load_ir(args.ir)
    hits = search(ir, args.query, args.limit)
    if args.json:
        print(json.dumps(hits, ensure_ascii=False, indent=2))
        return 0 if hits else 1
    if not hits:
        print("no match for %r" % args.query)
        return 1
    for member in hits:
        print("%-40s %-14s %s" % (format_signature(member), member["kind_label"], member["export"]))
    print()
    print("%d match(es)" % len(hits))
    return 0


def cmd_stats(args: argparse.Namespace) -> int:
    ir = load_ir(args.ir)
    meta = ir.get("meta", {})
    counts: dict[str, int] = {}
    owners_ = owners(ir)
    for entry in owners_:
        for member in iter_members(entry):
            counts[member["kind"]] = counts.get(member["kind"], 0) + 1

    if args.json:
        print(json.dumps(
            {
                "generated_at": meta.get("generated_at"),
                "classes": len(ir.get("classes", [])),
                "modules": len(ir.get("modules", [])),
                "skipped": len(meta.get("skipped", [])),
                "by_kind": counts,
            },
            ensure_ascii=False,
            indent=2,
        ))
        return 0

    print("IR           %s" % (args.ir or IR_PATH))
    print("generated    %s" % meta.get("generated_at", "--"))
    print("classes      %d" % len(ir.get("classes", [])))
    print("modules      %d" % len(ir.get("modules", [])))
    print("skipped      %d" % len(meta.get("skipped", [])))
    print()
    for kind in SECTION_ORDER:
        if kind in counts:
            print("  %-18s %d" % (KIND_LABELS.get(kind, kind), counts[kind]))
    print()
    print("total members  %d" % sum(counts.values()))
    if meta.get("skipped"):
        print()
        print("skipped at export time:")
        for node in meta["skipped"]:
            print("  %-8s %s" % (node.get("class", "?"), node.get("signature", "?")))
            print("           %s" % node.get("reason", ""))
    return 0


# --------------------------------------------------------------------------- #
# Entry point
# --------------------------------------------------------------------------- #


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="viewer.py",
        description="Serve or query the URGE binding IR (bind/api_reference.json).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("Commands\n----------\n", 1)[-1].split("Target syntax")[0].rstrip(),
    )
    parser.add_argument("--ir", metavar="PATH", help="path to api_reference.json")
    sub = parser.add_subparsers(dest="command")

    p_serve = sub.add_parser("serve", help="serve bind/ and open the HTML viewer")
    p_serve.add_argument("--port", type=int, default=8770)
    p_serve.add_argument("--no-open", action="store_true", help="do not launch a browser")
    p_serve.set_defaults(func=cmd_serve)

    p_embed = sub.add_parser("embed", help="inline the IR for file:// use")
    p_embed.add_argument("--out", metavar="PATH")
    p_embed.set_defaults(func=cmd_embed)

    p_list = sub.add_parser("list", help="list classes and modules")
    p_list.add_argument("--json", action="store_true")
    p_list.set_defaults(func=cmd_list)

    p_show = sub.add_parser("show", help="show a class, module or member")
    p_show.add_argument("target", metavar="TARGET", help="e.g. Sprite or Sprite#bitmap")
    p_show.add_argument("--json", action="store_true")
    p_show.set_defaults(func=cmd_show)

    p_search = sub.add_parser("search", help="search members by name, signature or type")
    p_search.add_argument("query", metavar="QUERY")
    p_search.add_argument("--limit", type=int, default=40)
    p_search.add_argument("--json", action="store_true")
    p_search.set_defaults(func=cmd_search)

    p_stats = sub.add_parser("stats", help="print summary counts")
    p_stats.add_argument("--json", action="store_true")
    p_stats.set_defaults(func=cmd_stats)

    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if not getattr(args, "command", None):
        parser.print_help()
        return 2
    if args.ir:
        args.ir = Path(args.ir)
        if not args.ir.is_absolute():
            args.ir = (Path.cwd() / args.ir).resolve()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())

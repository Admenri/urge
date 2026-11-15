#!/usr/bin/env python3
"""Exercise optional Windows consoles in an isolated, real GUI engine."""

from __future__ import annotations

import argparse
import ctypes
from ctypes import wintypes
from dataclasses import dataclass
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import time
import zlib


@dataclass(frozen=True)
class Case:
    name: str
    platform: str = ""  # An authored INI fragment; never inferred from argv.
    enabled: bool = False
    arguments: tuple[str, ...] = ()
    encoding: str = "utf-8"
    bom: bytes = b""
    mounted: str | None = None


CASES = (
    Case("missing-platform"),
    Case("missing-key", "[Platform]\r\nOtherSetting=authored\r\n"),
    Case("ini-false", "[Platform]\r\nWinConsole=false\r\n"),
    Case("ini-zero", "[Platform]\r\nWinConsole=0\r\n"),
    Case("ini-no", "[Platform]\r\nWinConsole=No\r\n"),
    Case("ini-invalid", "[Platform]\r\nWinConsole=not-a-boolean\r\n"),
    Case("ini-true", "[Platform]\r\nWinConsole=true\r\n", True),
    Case("ini-one", "[Platform]\r\nWinConsole=1\r\n", True),
    Case("ini-yes", "[Platform]\r\nWinConsole=YeS\r\n", True),
    Case("cli-exact", arguments=("console", "test"), enabled=True),
    Case("cli-or-false", "[Platform]\r\nWinConsole=false\r\n", True,
         ("debug", "console", "btest")),
    Case("cli-or-true", "[Platform]\r\nWinConsole=true\r\n", True,
         ("test", "console", "debug", "btest", "console")),
    Case("cli-nonmatching", arguments=(
        "Console", "--console", "preconsole", "consolex", "console=1",
        "console test", "console ", "--test", "Debug", "--btest", "btestx",
    )),
    Case("ini-utf8-bom", "[Platform]\r\nWinConsole=true\r\n", True,
         bom=b"\xef\xbb\xbf"),
    Case("ini-utf16le", "[Platform]\r\nWinConsole=true\r\n", True,
         encoding="utf-16le", bom=b"\xff\xfe"),
    Case("ini-utf16be", "[Platform]\r\nWinConsole=true\r\n", True,
         encoding="utf-16be", bom=b"\xfe\xff"),
    Case("ini-utf32le", "[Platform]\r\nWinConsole=true\r\n", True,
         encoding="utf-32le", bom=b"\xff\xfe\x00\x00"),
    Case("ini-utf32be", "[Platform]\r\nWinConsole=true\r\n", True,
         encoding="utf-32be", bom=b"\x00\x00\xfe\xff"),
    Case("cli-preserve-utf8-bom", "[Platform]\r\nWinConsole = No  \r\n", True,
         ("console",), bom=b"\xef\xbb\xbf"),
    Case("cli-preserve-utf16le", "[Platform]\r\nWinConsole = false  \r\n", True,
         ("console",), encoding="utf-16le", bom=b"\xff\xfe"),
    # A loose-file pre-read is not authoritative: the mounted INI wins both ways.
    Case("mounted-true", "[Platform]\r\nWinConsole=false\r\n", True,
         mounted="true"),
    Case("mounted-false", "[Platform]\r\nWinConsole=true\r\n",
         mounted="false"),
)


def pack_script(source: bytes) -> bytes:
    """Marshal 4.8 [[1, ASCII name, zlib(source)]]; no external Ruby required."""
    def length(value: int) -> bytes:
        if value == 0:
            return b"\x00"
        if value < 123:
            return bytes((value + 5,))
        data = value.to_bytes((value.bit_length() + 7) // 8, "little")
        if len(data) > 4:
            raise ValueError("script exceeds Ruby Marshal's four-byte length")
        return bytes((len(data),)) + data

    def string(value: bytes) -> bytes:
        return b'"' + length(len(value)) + value

    return (b"\x04\x08[" + length(1) + b"[" + length(3) + b"i" + length(1)
            + string(b"Windows console regression") + string(zlib.compress(source)))


def pack_ini_archive(payload: bytes) -> bytes:
    """A minimal RGSSAD v3 containing only Game.ini, with the standard XOR keys."""
    seed, key = 1, 0x12345678
    mask = seed * 9 + 3
    name = b"Game.ini"
    offset = 12 + 16 + len(name) + 4
    header = b"RGSSAD\x00\x03" + struct.pack("<I", seed)
    header += struct.pack("<4I", offset ^ mask, len(payload) ^ mask,
                          key ^ mask, len(name) ^ mask)
    header += bytes(value ^ ((mask >> (8 * (i % 4))) & 255)
                    for i, value in enumerate(name))
    header += struct.pack("<I", mask)  # Encrypted zero-offset end marker.
    encrypted = bytearray(payload)
    for start in range(0, len(encrypted), 4):
        for i in range(min(4, len(encrypted) - start)):
            encrypted[start + i] ^= (key >> (8 * i)) & 255
        key = (key * 7 + 3) & 0xFFFFFFFF
    return header + encrypted


def require_gui_executable(engine: Path) -> None:
    # CREATE_NO_WINDOW also hides a console-subsystem binary, so check the PE
    # header separately rather than letting that flag mask a CMake regression.
    with engine.open("rb") as file:
        dos = file.read(64)
        if len(dos) != 64 or dos[:2] != b"MZ":
            raise ValueError("--engine must be a Windows PE executable")
        file.seek(struct.unpack_from("<I", dos, 60)[0])
        header = file.read(24)
        optional = file.read(70)
    if len(header) != 24 or header[:4] != b"PE\x00\x00" or len(optional) != 70:
        raise ValueError("invalid PE headers")
    if struct.unpack_from("<H", optional)[0] not in (0x10B, 0x20B):
        raise ValueError("unsupported PE optional header")
    if struct.unpack_from("<H", optional, 68)[0] != 2:
        raise ValueError("engine is not IMAGE_SUBSYSTEM_WINDOWS_GUI (WIN32_EXECUTABLE)")


class DialogGuard:
    """Identify only this child's native modal dialogs; never touch other apps."""
    def __init__(self) -> None:
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND,
                                               wintypes.LPARAM)
        self.user32.EnumWindows.argtypes = (self.callback_type, wintypes.LPARAM)
        self.user32.EnumWindows.restype = wintypes.BOOL
        self.user32.GetWindowThreadProcessId.argtypes = (
            wintypes.HWND, ctypes.POINTER(wintypes.DWORD))
        self.user32.GetClassNameW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
        self.user32.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)

    def titles(self, pid: int) -> list[str]:
        found: list[str] = []

        @self.callback_type
        def visit(window: int, _: int) -> bool:
            owner = wintypes.DWORD()
            self.user32.GetWindowThreadProcessId(window, ctypes.byref(owner))
            if owner.value == pid:
                name = ctypes.create_unicode_buffer(64)
                self.user32.GetClassNameW(window, name, len(name))
                if name.value == "#32770":
                    title = ctypes.create_unicode_buffer(256)
                    self.user32.GetWindowTextW(window, title, len(title))
                    found.append(title.value or "untitled dialog")
            return True

        self.user32.EnumWindows(visit, 0)
        return found


def run_child(target: Path, case: Case, env: dict[str, str], timeout: float,
              guard: DialogGuard) -> tuple[int, list[str]]:
    directory = target.parent
    (directory / "parent-stdin.bin").write_bytes(b"INHERITED_INPUT_NOT_NUL\n")
    errors: list[str] = []
    with ((directory / "parent-stdin.bin").open("rb") as stdin,
          (directory / "parent-stdout.bin").open("wb") as stdout,
          (directory / "parent-stderr.bin").open("wb") as stderr):
        # No AttachConsole/AllocConsole in the runner. GUI children do not inherit
        # a parent's console; CREATE_NO_WINDOW additionally protects CUI launches.
        process = subprocess.Popen(
            [str(target), *case.arguments], cwd=directory, env=env,
            stdin=stdin, stdout=stdout, stderr=stderr, close_fds=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        deadline = time.monotonic() + timeout
        try:
            while process.poll() is None:
                dialogs = guard.titles(process.pid)
                if dialogs:
                    errors.append(f"unexpected modal dialog: {dialogs!r}")
                    break
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    errors.append(f"timeout after {timeout:g}s")
                    break
                try:
                    process.wait(timeout=min(0.1, remaining))
                except subprocess.TimeoutExpired:
                    pass
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)
    return process.returncode, errors


def ini_bytes(version: int, backend: str, platform: str, encoding: str,
              bom: bytes) -> bytes:
    filename = {1: "Scripts.rxdata", 2: "Scripts.rvdata", 3: "Scripts.rvdata2"}[version]
    text = (
        "; Authored whitespace, CRLF and Unicode must survive shutdown unchanged.\r\n"
        "[Game]\r\nTitle = Console regression / 中文 / 日本語 / 🎮  \r\n"
        f"RGSS={version}\r\nScripts=Data/{filename}\r\n"
        "RTP=\r\nRTP1=\r\nRTP2=\r\nRTP3=\r\n"
        f"[GFX]\r\nBackend={backend}\r\n"
        "[Display]\r\nWidth=320\r\nHeight=240\r\nVSync=false\r\n"
        "FrameSkip=false\r\nBackgroundRunning=true\r\n"
        # Prevent normal input-default migration from obscuring INI persistence.
        "[Input]\r\nPadBindings=true\r\n[KeyBinding]\r\nC=Space\r\n" + platform
    )
    return bom + text.encode(encoding)


def prepare_case(directory: Path, engine: Path, packed: bytes, version: int,
                 backend: str, case: Case) -> bytes:
    shutil.copy2(engine, directory / "Game.exe")
    # Shared-library builds need their siblings. Do not copy game files/INIs.
    for dll in engine.parent.glob("*.dll"):
        shutil.copy2(dll, directory / dll.name)
    (directory / "Data").mkdir()
    filename = {1: "Scripts.rxdata", 2: "Scripts.rvdata", 3: "Scripts.rvdata2"}[version]
    (directory / "Data" / filename).write_bytes(packed)
    (directory / "Fonts").mkdir()
    original = ini_bytes(version, backend, case.platform, case.encoding, case.bom)
    (directory / "Game.ini").write_bytes(original)
    if case.mounted is not None:
        mounted = ini_bytes(version, backend,
                            f"[Platform]\r\nWinConsole={case.mounted}\r\n",
                            "utf-16le", b"\xff\xfe")
        (directory / "Game.rgss3a").write_bytes(pack_ini_archive(mounted))
    authored_enabled = case.mounted == "true" if case.mounted is not None else (
        "WinConsole=true" in case.platform or "WinConsole=1" in case.platform
        or "WinConsole=YeS" in case.platform)
    expected = {
        "enabled": str(int(case.enabled)),
        "win_console": "true" if authored_enabled else "false",
        "debug": str(int(version == 1 and "debug" in case.arguments)),
        "test": str(int(version != 1 and "test" in case.arguments)),
        "btest": str(int("btest" in case.arguments)),
        "encoding": "UTF-16LE" if case.mounted is not None else case.encoding.upper(),
        "mounted": str(int(case.mounted is not None)),
    }
    (directory / "probe-expectations.txt").write_text(
        "".join(f"{key}={value}\n" for key, value in expected.items()), encoding="utf-8")
    return original


def inspect_case(directory: Path, original: bytes | None) -> list[str]:
    errors: list[str] = []
    report = directory / "console-results.txt"
    if not report.exists():
        errors.append("Ruby cleanup did not write console-results.txt")
    else:
        lines = report.read_text(encoding="utf-8").splitlines()
        if not lines or lines[0] != "ALL_OK":
            errors.extend(lines or ["empty Ruby result"])
    ini = directory / "Game.ini"
    if (ini.read_bytes() if ini.exists() else None) != original:
        errors.append("authored Game.ini bytes changed (or a missing INI was created)")
    for name in ("parent-stdout.bin", "parent-stderr.bin"):
        if (directory / name).stat().st_size:
            errors.append(f"output escaped to inherited {name}")
    log = directory / "Game.log"
    if not log.exists():
        errors.append("Game.log is missing")
    else:
        # Keep diagnostics bounded if a broken child floods the file.
        with log.open("rb") as file:
            text = file.read(262144).decode("utf-8", errors="replace")
        for marker in ("[Host] Game directory:", "[Ruby] starting the interpreter",
                       "[Config] IniEncoding can only be changed before startup"):
            if text.count(marker) != 1:
                errors.append(f"Game.log must contain exactly one {marker!r}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("build/console-integration"))
    parser.add_argument("--rgss", type=int, nargs="+", choices=(1, 2, 3), default=(1, 3))
    parser.add_argument("--case", action="append", choices=tuple(case.name for case in CASES))
    parser.add_argument("--backend", default="opengl")
    parser.add_argument("--timeout", type=float, default=45,
                        help="seconds per child (maximum 300; default 45)")
    args = parser.parse_args()
    if os.name != "nt":
        parser.error("these integration tests require Windows")
    if not 0 < args.timeout <= 300:
        parser.error("--timeout must be greater than zero and at most 300")
    if any(character in args.backend for character in "\r\n"):
        parser.error("--backend must be a single INI value")
    try:
        engine = args.engine.resolve(strict=True)
        require_gui_executable(engine)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    packed = pack_script(Path(__file__).with_name("probe.rb").read_bytes())
    # Use SDL's deterministic system startup messages to test its callback and
    # delayed replay without depending on drivers issuing a warning.
    env = os.environ.copy()
    for key in tuple(env):
        if key.upper().startswith(("URGE_", "RUBY", "SDL_")):
            del env[key]
    env.update(SDL_LOGGING="system=info,*=error", SDL_AUDIO_DRIVER="dummy",
               URGE_CRASH_DIALOG="0", URGE_CRASH_DIR=str(root))
    guard = DialogGuard()
    results = []
    for version in dict.fromkeys(args.rgss):
        for case in CASES:
            if args.case and case.name not in args.case:
                continue
            directory = Path(tempfile.mkdtemp(
                prefix=f"rgss{version}-{case.name}-控制台-", dir=root))
            print(f"Running RGSS{version} {case.name}: {directory}", flush=True)
            errors: list[str] = []
            try:
                original = prepare_case(directory, engine, packed, version, args.backend, case)
                code, errors = run_child(directory / "Game.exe", case, env, args.timeout, guard)
                if code != 0:
                    errors.append(f"engine exit code {code}")
                errors.extend(inspect_case(directory, original))
            except (OSError, ValueError, subprocess.SubprocessError) as error:
                errors.append(f"harness: {error}")
            passed = not errors
            results.append({"rgss": version, "case": case.name, "path": str(directory),
                            "passed": passed, "errors": errors})
            print(f"{'PASS' if passed else 'FAIL'} RGSS{version} {case.name}", flush=True)
            for error in errors:
                print(f"  {error}", flush=True)
    (root / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2),
                                       encoding="utf-8")
    failed = sum(not result["passed"] for result in results)
    print(f"{len(results) - failed}/{len(results)} passed; artifacts: {root}", flush=True)
    return int(failed != 0 or not results)


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Run real Ruby Fetch/WebSocket integration tests against a loopback fixture.

Uses only the Python standard library. No external Ruby or public network needed.
"""
from __future__ import annotations

import argparse
import base64
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import runpy
import shutil
import socket
import struct
import subprocess
import tempfile
import threading
import time
import zlib


BINARY_PAYLOAD = bytes(range(256)) * 1024


def frame(opcode: int, data: bytes, final: bool = True) -> bytes:
    size = len(data)
    header = bytes(((0x80 if final else 0) | opcode,))
    if size < 126:
        header += bytes((size,))
    elif size < 65536:
        header += b"\x7e" + struct.pack("!H", size)
    else:
        header += b"\x7f" + struct.pack("!Q", size)
    return header + data


def exact(stream, size: int) -> bytes:
    data = stream.read(size)
    if len(data) != size:
        raise EOFError("peer disconnected")
    return data


class Fixture(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *_):
        pass

    def respond(self, status: int, body: bytes,
                content_type: str = "text/plain; charset=utf-8", **headers):
        self.send_response(status, "Fixture Status" if status == 418 else None)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Content-Type", content_type)
        for key, value in headers.items():
            self.send_header(key.replace("_", "-"), value)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self.respond(200, b"head body", X_Test="head")

    def do_POST(self):
        data = exact(self.rfile, int(self.headers.get("Content-Length", "0")))
        self.respond(201, data, X_Request=self.headers.get("X-Request", "missing"))

    def do_GET(self):
        try:
            if self.headers.get("Upgrade", "").lower() == "websocket":
                self.websocket()
            elif self.path == "/redirect":
                self.respond(302, b"", Location="/get", X_Test="redirect")
            elif self.path == "/slow":
                # Headers are immediate; the body stalls to exercise abort/timeouts.
                self.send_response(200)
                self.send_header("Content-Length", "4")
                self.end_headers()
                time.sleep(3)
                self.wfile.write(b"slow")
            elif self.path == "/progress":
                self.send_response(200)
                self.send_header("Content-Length", "8")
                self.end_headers()
                self.wfile.write(b"half")
                self.wfile.flush()
                time.sleep(0.5)
                self.wfile.write(b"done")
            elif self.path in ("/binary", "/binary-status"):
                self.respond(418 if self.path == "/binary-status" else 200,
                             BINARY_PAYLOAD, content_type="application/octet-stream")
            elif self.path == "/binary-chunked":
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                for start in range(0, len(BINARY_PAYLOAD), 4093):
                    chunk = BINARY_PAYLOAD[start:start + 4093]
                    self.wfile.write(f"{len(chunk):x}\r\n".encode() + chunk + b"\r\n")
                self.wfile.write(b"0\r\n\r\n")
            elif self.path == "/empty":
                self.respond(200, b"", content_type="application/octet-stream")
            elif self.path == "/status":
                self.respond(418, b"teapot", X_Test="error-response")
            else:
                self.respond(200, "fetch 中文".encode(), X_Test="final")
        except (BrokenPipeError, ConnectionResetError, EOFError, socket.timeout):
            pass

    def websocket(self):
        if self.path == "/reject":
            self.respond(403, b"rejected")
            return
        if self.path == "/ws-slow":
            time.sleep(3)
        key = self.headers["Sec-WebSocket-Key"]
        accept = base64.b64encode(hashlib.sha1(
            (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        self.send_response(101)
        self.send_header("Upgrade", "websocket")
        self.send_header("Connection", "Upgrade")
        self.send_header("Sec-WebSocket-Accept", accept)
        if self.headers.get("Sec-WebSocket-Protocol"):
            self.send_header("Sec-WebSocket-Protocol", "urge-test")
        self.end_headers()
        self.connection.settimeout(10)
        if self.path == "/mixed":
            data = (chr(0xFEFF) + "echo\x00中文\x00").encode()
            self.wfile.write(frame(1, data) + frame(2, data) + frame(1, b"") + frame(2, b""))
            self.wfile.flush()
        elif self.path == "/fragments":
            self.wfile.write(frame(1, b"fragment-", False) + frame(9, b"server-ping")
                             + frame(0, "中文".encode(), True))
            self.wfile.flush()
        elif self.path == "/binary-fragments":
            self.wfile.write(frame(2, BINARY_PAYLOAD[:4093], False)
                             + frame(9, b"server-ping")
                             + frame(0, BINARY_PAYLOAD[4093:65537], False)
                             + frame(0, BINARY_PAYLOAD[65537:], True))
            self.wfile.flush()
        elif self.path == "/burst":
            self.wfile.write(b"".join(frame(1, str(i).encode()) for i in range(12))
                             + frame(8, struct.pack("!H", 1000) + b"burst-end"))
            self.wfile.flush()
        message_type = None
        message = bytearray()
        while True:
            first, second = exact(self.rfile, 2)
            opcode, size = first & 15, second & 127
            if size == 126:
                size = struct.unpack("!H", exact(self.rfile, 2))[0]
            elif size == 127:
                size = struct.unpack("!Q", exact(self.rfile, 8))[0]
            if size > 2_000_000:
                raise ValueError("unexpected fixture frame size")
            if not second & 128:
                raise ValueError("client frames must be masked")
            mask = exact(self.rfile, 4)
            data = exact(self.rfile, size)
            data = bytes(value ^ mask[i % 4] for i, value in enumerate(data))
            if opcode == 8:
                self.wfile.write(frame(8, data))
                self.wfile.flush()
                return
            if opcode == 9:
                self.wfile.write(frame(10, data) + frame(1, b"client-ping-seen"))
            elif opcode == 10:
                self.wfile.write(frame(1, b"server-pong-seen"))
            elif opcode in (0, 1, 2):
                # Browsers may fragment outgoing messages even when send() was
                # called once. Echo the message, not its first wire fragment.
                if opcode in (1, 2):
                    if self.path == "/binary-only" and opcode != 2:
                        self.wfile.write(frame(8, struct.pack("!H", 1003) + b"binary required"))
                        self.wfile.flush()
                        return
                    if message_type is not None:
                        raise ValueError("nested fixture message")
                    message_type = opcode
                elif message_type is None:
                    raise ValueError("unexpected fixture continuation")
                message.extend(data)
                if len(message) > 2_000_000:
                    raise ValueError("unexpected fixture message size")
                if first & 128:
                    self.wfile.write(frame(message_type, bytes(message)))
                    message_type = None
                    message.clear()
            self.wfile.flush()


class Server(ThreadingHTTPServer):
    daemon_threads = True

    def handle_error(self, request, client_address):
        # Expected when a failing/aborted test closes its connection.
        pass


def pack_script(source: bytes) -> bytes:
    def length(value: int) -> bytes:
        if not value:
            return b"\x00"
        if value < 123:
            return bytes((value + 5,))
        data = value.to_bytes((value.bit_length() + 7) // 8, "little")
        return bytes((len(data),)) + data

    def string(data: bytes) -> bytes:
        return b'"' + length(len(data)) + data

    return (b"\x04\x08[\x06[\x08i\x06" + string(b"Network integration")
            + string(zlib.compress(source)))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("build/network-integration"))
    parser.add_argument("--rgss", type=int, nargs="+", choices=(1, 2, 3), default=(1, 2, 3))
    parser.add_argument("--backend", default="opengl")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    if not 0 < args.timeout <= 300 or any(c in args.backend for c in "\r\n"):
        parser.error("invalid timeout or backend")
    engine = args.engine.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    api = json.loads((Path(__file__).resolve().parents[3] / "bind/api_reference.json")
                     .read_text(encoding="utf-8"))
    constants = []
    for group in ("classes", "modules"):
        entries = api.get(group, {})
        if isinstance(entries, list):
            entries = {entry["name"]: entry for entry in entries}
        for owner, entry in entries.items():
            for constant in entry.get("constants", []):
                constants.append(f"{owner}::{constant['name']}")
    server = Server(("127.0.0.1", 0), Fixture)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    port = server.server_port
    source = Path(__file__).with_name("probe.rb").read_text(encoding="utf-8")
    constant_tests = runpy.run_path(str(Path(__file__).resolve().parents[1] / "test_binding_constants.py"))
    source = source.replace("__PORT__", str(port)).replace(
        "__CONSTANT_CHECKS__", constant_tests["ruby_checks"](api))
    passed = True
    try:
        for version in dict.fromkeys(args.rgss):
            directory = Path(tempfile.mkdtemp(prefix=f"rgss{version}-", dir=root))
            target = directory / engine.name
            shutil.copy2(engine, target)
            for dll in engine.parent.glob("*.dll"):
                shutil.copy2(dll, directory / dll.name)
            (directory / "Data").mkdir()
            suffix = {1: "rxdata", 2: "rvdata", 3: "rvdata2"}[version]
            (directory / "Data" / f"Scripts.{suffix}").write_bytes(pack_script(source.encode()))
            (directory / "Game.ini").write_text(
                f"[Game]\nTitle=Network integration\nRGSS={version}\nScripts=Data/Scripts.{suffix}\n"
                f"RTP=\nRTP1=\nRTP2=\nRTP3=\n[GFX]\nBackend={args.backend}\n"
                "[Display]\nWidth=320\nHeight=240\nVSync=false\nBackgroundRunning=true\n",
                encoding="utf-8")
            (directory / "constants.txt").write_text("\n".join(constants), encoding="utf-8")
            env = {key: value for key, value in os.environ.items()
                   if not key.upper().startswith(("RUBY", "URGE_", "SDL_"))}
            env.update(SDL_AUDIO_DRIVER="dummy", URGE_CRASH_DIALOG="0", URGE_CRASH_DIR=str(directory))
            print(f"Running RGSS{version}: {directory}", flush=True)
            with (directory / "engine-output.txt").open("wb") as output:
                process = subprocess.Popen([str(target)], cwd=directory, env=env,
                                           stdin=subprocess.DEVNULL, stdout=output, stderr=output)
                try:
                    code = process.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                    code = -1
            result_file = directory / "network-results.txt"
            result = result_file.read_text(encoding="utf-8") if result_file.exists() else "FAIL no Ruby report\n"
            print(result, end="", flush=True)
            if code != 0 or "FAIL" in result or "ALL_OK" not in result:
                passed = False
                if code != 0:
                    print(f"FAIL engine exit code {code}", flush=True)
                else:
                    print(f"FAIL RGSS{version} Ruby assertions", flush=True)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    print(f"Artifacts: {root}", flush=True)
    return int(not passed)


if __name__ == "__main__":
    raise SystemExit(main())

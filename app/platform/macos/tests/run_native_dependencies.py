"""Integration checks for static codecs and the actual native Ruby networking API.

Uses loopback fixtures and the system compiler; no Python packages are required.
"""
import base64
import gzip
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os
from pathlib import Path
import shutil
import socketserver
import ssl
import struct
import subprocess
import tempfile
import threading
import zlib

REPO = Path(__file__).resolve().parents[4]
TESTS = Path(__file__).resolve().parent
BUILD = Path(os.environ.get("URGE_NATIVE_BUILD_DIR", REPO / "build/macos-native"))
CACHE = dict(line.split("=", 1) for line in (BUILD / "CMakeCache.txt").read_text().splitlines()
             if "=" in line and not line.startswith(("//", "#")))
PREFIX = Path(CACHE["URGE_MACOS_DEPS_PREFIX:PATH"])
TARGET = CACHE["CMAKE_OSX_DEPLOYMENT_TARGET:STRING"]


def run(args, **kwargs):
    result = subprocess.run(list(map(str, args)), stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, timeout=90, **kwargs)
    print(result.stdout, end="", flush=True)
    if result.returncode:
        raise RuntimeError(f"Command exited {result.returncode}: {args}")
    return result


def read_exact(stream, length):
    data = b""
    while len(data) < length:
        chunk = stream.read(length - len(data))
        if not chunk:
            raise EOFError("connection closed")
        data += chunk
    return data


def frame(kind, flags, stream, body):
    return len(body).to_bytes(3, "big") + bytes([kind, flags]) + struct.pack(">I", stream) + body


class H2Handler(socketserver.StreamRequestHandler):
    def handle(self):
        # Minimal HTTP/2 fixture: ALPN, SETTINGS, an indexed :status 200 and DATA.
        try:
            assert self.request.selected_alpn_protocol() == "h2"
            assert read_exact(self.rfile, 24) == b"PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
            self.wfile.write(frame(4, 0, 0, b""))
            while True:
                header = read_exact(self.rfile, 9)
                length = int.from_bytes(header[:3], "big")
                kind, flags = header[3:5]
                stream = int.from_bytes(header[5:9], "big") & 0x7fffffff
                read_exact(self.rfile, length)
                if kind == 4 and not flags & 1:
                    self.wfile.write(frame(4, 1, 0, b""))
                if kind == 1:
                    self.wfile.write(frame(1, 4, stream, b"\x88"))
                    self.wfile.write(frame(0, 1, stream, b"HTTP2_PASS"))
                    # Drain SETTINGS acknowledgements/WINDOW_UPDATE until the
                    # client closes; closing immediately can race its TLS write.
                if kind == 7:
                    return
        except (EOFError, ConnectionError):
            pass


def main():
    binary = BUILD / "app/Game"
    run(["python3", REPO / "app/platform/macos/audit-native.py", binary,
         "--target", TARGET, "--archives-root", PREFIX, "--archives-root", BUILD])
    with tempfile.TemporaryDirectory(prefix="urge-native-deps-游戏 ") as directory:
        game = Path(directory)
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
        probe = game / "probe"
        run(["/usr/bin/clang++", "-std=c++17", f"-mmacosx-version-min={TARGET}", "-isysroot", sdk,
             "-I" + str(REPO / "third_party/curl/include"), "-I" + str(PREFIX / "include"),
             TESTS / "dependency_probe.cc", BUILD / "third_party/curl/lib/libcurl.a",
             *[PREFIX / f"lib/lib{name}.a" for name in
               ("ssl", "crypto", "z", "brotlidec", "brotlienc", "brotlicommon", "zstd", "nghttp2")],
             "-framework", "Security", "-framework", "CoreFoundation", "-framework", "SystemConfiguration",
             "-licucore", "-liconv", "-o", probe])
        run([probe, "features"])
        payload = ("gzip/deflate 完整解压验证 🦀\n".encode() + bytes(range(256))) * 1024
        (game / "payload").write_bytes(payload)
        run([probe, "encode", game / "payload"])
        raw = zlib.compressobj(wbits=-15)
        bad_crc = bytearray(gzip.compress(payload))
        bad_crc[-8] ^= 255
        encoded = {
            "identity": (None, payload),
            "gzip": ("gzip", gzip.compress(payload)),
            "deflate": ("deflate", zlib.compress(payload)),
            "raw-deflate": ("deflate", raw.compress(payload) + raw.flush()),
            "br": ("br", (game / "payload.br").read_bytes()),
            "zstd": ("zstd", (game / "payload.zstd").read_bytes()),
            "stacked": ("gzip, deflate", zlib.compress(gzip.compress(payload))),
            "bad-gzip": ("gzip", b"invalid gzip stream"),
            "bad-deflate": ("deflate", b"invalid deflate stream"),
            "bad-crc": ("gzip", bytes(bad_crc)),
            "truncated-gzip": ("gzip", gzip.compress(payload)[:20]),
        }

        class Handler(BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *_):
                pass

            def handle(self):
                try:
                    super().handle()
                except (ConnectionResetError, BrokenPipeError, EOFError):
                    # Corrupt-response tests deliberately abandon connections.
                    pass

            def do_GET(self):
                if self.path == "/echo":
                    accept = base64.b64encode(hashlib.sha1((self.headers["Sec-WebSocket-Key"] +
                        "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
                    self.send_response(101)
                    self.send_header("Upgrade", "websocket")
                    self.send_header("Connection", "Upgrade")
                    self.send_header("Sec-WebSocket-Accept", accept)
                    self.end_headers()
                    while True:
                        first, length = read_exact(self.rfile, 2)
                        opcode = first & 15
                        assert length & 128
                        length &= 127
                        assert length < 126
                        mask = read_exact(self.rfile, 4)
                        body = read_exact(self.rfile, length)
                        body = bytes(byte ^ mask[i % 4] for i, byte in enumerate(body))
                        if opcode == 8:
                            self.wfile.write(bytes([0x88, length]) + body)
                            self.close_connection = True
                            return
                        assert opcode in (1, 9)
                        self.wfile.write(bytes([0x81 if opcode == 1 else 0x8a, length]) + body)
                if self.path == "/redirect":
                    self.send_response(302)
                    self.send_header("Location", "/gzip")
                    self.send_header("Content-Length", "0")
                    self.end_headers()
                    return
                if self.path == "/accept-encoding":
                    encoding, body = None, self.headers.get("Accept-Encoding", "").encode()
                else:
                    encoding, body = encoded.get(self.path[1:], (None, b"TLS_PASS"))
                self.send_response(200)
                if encoding:
                    self.send_header("Content-Encoding", encoding)
                # Advertise the original length, then interrupt the compressed
                # transfer. Fetch must preserve curl's transport failure.
                length = len(encoded["gzip"][1]) if self.path == "/truncated-gzip" else len(body)
                self.send_header("Content-Length", str(length))
                if self.path == "/truncated-gzip":
                    self.send_header("Connection", "close")
                    self.close_connection = True
                self.end_headers()
                self.wfile.write(body)

        openssl = PREFIX / "bin/openssl"
        cert, key = game / "cert.pem", game / "key.pem"
        run([openssl, "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:P-256", "-nodes", "-days", "1",
             "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1",
             "-keyout", key, "-out", cert, "-config", "/dev/null"])
        tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        tls.load_cert_chain(cert, key)
        http = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        https = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        https.socket = tls.wrap_socket(https.socket, server_side=True)
        h2tls = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        h2tls.load_cert_chain(cert, key)
        h2tls.set_alpn_protocols(["h2"])
        h2 = socketserver.ThreadingTCPServer(("127.0.0.1", 0), H2Handler)
        h2.socket = h2tls.wrap_socket(h2.socket, server_side=True)
        servers = (http, https, h2)
        for server in servers:
            threading.Thread(target=server.serve_forever, daemon=True).start()
        try:
            tls_url = f"https://localhost:{https.server_port}/tls"
            result = run([probe, "request", tls_url, cert, 0])
            assert result.stdout == "TLS_PASS"
            result = run([probe, "request", f"https://localhost:{h2.server_address[1]}/", cert, 3])
            assert result.stdout == "HTTP2_PASS"
            print("\nPASS trusted HTTPS and negotiated HTTP/2", flush=True)
            (game / "http_url").write_text(f"http://127.0.0.1:{http.server_port}")
            (game / "https_url").write_text(tls_url)
            shutil.copy2(binary, game / "Game")
            (game / "Data").mkdir()
            (game / "Graphics").mkdir()
            for extension in ("png", "jpg", "tif", "webp", "avif"):
                shutil.copy2(REPO / f"third_party/SDL_image/test/sample.{extension}", game / f"Graphics/sample.{extension}")
            ini = (REPO / "test/text_input/Game.ini").read_text()
            ini = ini.replace("[Display]", "[Display]\nBackgroundRunning=true").replace("Backend=", "Backend=metal")
            (game / "Game.ini").write_text(ini)
            run(["ruby", REPO / "test/text_input/pack_scripts.rb", TESTS / "native_dependencies.rb",
                 game / "Data/Scripts.rxdata"])
            # Prove that lazy decoder loads and network code work even when both
            # package-manager roots and the entire build prefix are inaccessible.
            profile = ('(version 1)(allow default)(deny file-read* '
                       '(subpath "/opt/homebrew")(subpath "/usr/local")'
                       f'(subpath "{PREFIX}"))')
            env = os.environ.copy()
            for name in list(env):
                if name.startswith("DYLD_") or name.lower().endswith("_proxy"):
                    env.pop(name)
            env["NO_PROXY"] = "*"
            result = run(["/usr/bin/sandbox-exec", "-p", profile, game / "Game"], cwd=game, env=env)
            assert "NATIVE_DEPENDENCIES_PASS" in result.stdout
            print("PASS: runtime loads, image codecs and networking without Homebrew or build-prefix access")
        finally:
            for server in servers:
                server.shutdown()
                server.server_close()


if __name__ == "__main__":
    main()

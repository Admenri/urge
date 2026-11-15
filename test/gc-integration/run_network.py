#!/usr/bin/env python3
"""Exercise native-root shutdown against local stalled HTTP/WebSocket peers."""

from __future__ import annotations

import argparse
from pathlib import Path
import socketserver
import subprocess
import threading


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    stop = threading.Event()

    class Stall(socketserver.BaseRequestHandler):
        def handle(self) -> None:
            self.request.settimeout(2)
            try:
                self.request.recv(8192)
            except OSError:
                return
            stop.wait(15)

    class Server(socketserver.ThreadingTCPServer):
        allow_reuse_address = True
        daemon_threads = True

    with Server(("127.0.0.1", 0), Stall) as server:
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        port = server.server_address[1]
        try:
            result = subprocess.run(
                [str(args.probe.resolve(strict=True)),
                 f"ws://127.0.0.1:{port}/socket", f"http://127.0.0.1:{port}/fetch"],
                timeout=30,
            )
        finally:
            stop.set()
            server.shutdown()
            thread.join()
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())

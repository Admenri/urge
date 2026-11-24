#!/usr/bin/env python3
"""Build the actual Web network backend and test it in a local headless browser.

Only the Python standard library, an Emscripten SDK and Chrome/Chromium/Edge
are required. Uses the same loopback binary fixture as the native Ruby tests.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile
import threading


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
fixture = runpy.run_path(str(HERE / "run.py"))

PAGE = b"""<!doctype html><meta charset="utf-8"><title>Network regression</title>
<pre id="output"></pre><script>
const lines = [];
let reported = false;
function report(code) {
  if (reported) return;
  reported = true;
  fetch('/results', {method: 'POST', headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({code, lines})});
}
var Module = {
  arguments: [location.origin],
  print(line) { lines.push(line); document.getElementById('output').textContent += line + '\\n'; },
  printErr(line) { lines.push('STDERR ' + line); },
  onExit: report,
  onAbort(reason) { lines.push('FAIL abort: ' + reason); report(1); }
};
window.addEventListener('error', (event) => { lines.push('FAIL JS: ' + event.message); report(1); });
window.addEventListener('unhandledrejection', (event) => { lines.push('FAIL JS: ' + event.reason); report(1); });
</script><script src="network.js"></script>
"""


class WebFixture(fixture["Fixture"]):
    def do_GET(self):
        if self.path == "/":
            self.respond(200, PAGE, content_type="text/html; charset=utf-8")
        elif self.path in ("/network.js", "/network.wasm"):
            path = self.server.artifacts / self.path[1:]
            self.respond(200, path.read_bytes(), content_type=(
                "application/wasm" if path.suffix == ".wasm" else "text/javascript"))
        else:
            super().do_GET()

    def do_POST(self):
        if self.path != "/results":
            return super().do_POST()
        data = fixture["exact"](self.rfile, int(self.headers["Content-Length"]))
        self.server.result = json.loads(data)
        self.respond(200, b"ok")
        self.server.finished.set()


def locate_browser() -> str | None:
    for name in ("chromium", "chromium-browser", "google-chrome", "msedge"):
        if path := shutil.which(name):
            return path
    for root, name in (("PROGRAMFILES", "Google/Chrome/Application/chrome.exe"),
                       ("PROGRAMFILES(X86)", "Microsoft/Edge/Application/msedge.exe")):
        path = Path(os.environ.get(root, "")) / name
        if path.is_file():
            return str(path)
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sdk = Path(os.environ.get("EMSDK", ".")) / "upstream/emscripten"
    default_compiler = shutil.which("em++") or next(
        (str(sdk / name) for name in ("em++", "em++.exe", "em++.bat")
         if (sdk / name).is_file()), "em++")
    parser.add_argument("--emxx", default=default_compiler)
    parser.add_argument("--browser", default=locate_browser())
    parser.add_argument("--output", type=Path, default=REPO / "build/network-web-integration")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    if not args.browser:
        parser.error("Chrome/Chromium/Edge not found; pass --browser")
    if not 0 < args.timeout <= 300:
        parser.error("timeout must be within (0, 300]")
    args.output.mkdir(parents=True, exist_ok=True)
    artifacts = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    command = [args.emxx, "-std=c++20", "-O1", "-fexceptions", "-DURGE_ENABLE_NETWORK=1",
               "-I", str(REPO), str(HERE / "web_probe.cc"),
               str(REPO / "core/network/network.cc"), str(REPO / "core/network/network_web.cc"),
               "-sASYNCIFY", "-sFETCH=1", "-lwebsocket.js", "-sEXIT_RUNTIME=1",
               "-sALLOW_MEMORY_GROWTH=1", "-sENVIRONMENT=web", "-o", str(artifacts / "network.js")]
    print("Building Web backend probe", flush=True)
    try:
        subprocess.run(command, cwd=REPO, check=True, timeout=300)
    except (OSError, subprocess.SubprocessError) as error:
        print(f"FAIL Web probe build: {error}\nArtifacts: {artifacts}", flush=True)
        return 1
    server = fixture["Server"](("127.0.0.1", 0), WebFixture)
    server.artifacts = artifacts
    server.finished = threading.Event()
    server.result = None
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        url = f"http://127.0.0.1:{server.server_port}/"
        print(f"Running headless browser: {url}", flush=True)
        with (artifacts / "browser-output.txt").open("wb") as output:
            process = subprocess.Popen(
                [args.browser, "--headless", "--no-first-run", "--no-default-browser-check",
                 "--disable-background-networking", "--disable-extensions", "--disable-sync",
                 f"--user-data-dir={artifacts / 'browser-profile'}", url],
                cwd=REPO, stdout=output, stderr=output, stdin=subprocess.DEVNULL)
            try:
                if not server.finished.wait(args.timeout):
                    print("FAIL browser timed out without reporting results", flush=True)
                    return 1
            finally:
                process.terminate()
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        result = server.result
        (artifacts / "results.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        for line in result["lines"]:
            print(line, flush=True)
        return int(result["code"] != 0 or "ALL_OK" not in result["lines"]
                   or any(line.startswith("FAIL") for line in result["lines"]))
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
        print(f"Artifacts: {artifacts}", flush=True)


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Exercise GPU safety and Ruby exports in an isolated, real engine process."""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import runpy
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
# Reuse the tested Marshal/zlib packer; this does not start its network fixture.
pack_script = runpy.run_path(str(ROOT / "tools/tests/network/run.py"))["pack_script"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("build/gpu-integration"))
    parser.add_argument("--rgss", type=int, nargs="+", choices=(1, 2, 3), default=(1, 2, 3))
    parser.add_argument("--backend", default="opengl")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    if not 0 < args.timeout <= 300 or any(c in args.backend for c in "\r\n"):
        parser.error("invalid timeout or backend")
    engine = args.engine.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    packed = pack_script(Path(__file__).with_name("probe.rb").read_bytes())
    passed = True
    for version in dict.fromkeys(args.rgss):
        directory = Path(tempfile.mkdtemp(prefix=f"rgss{version}-", dir=root))
        target = directory / engine.name
        shutil.copy2(engine, target)
        for dll in engine.parent.glob("*.dll"):
            shutil.copy2(dll, directory / dll.name)
        (directory / "Data").mkdir()
        suffix = {1: "rxdata", 2: "rvdata", 3: "rvdata2"}[version]
        (directory / "Data" / f"Scripts.{suffix}").write_bytes(packed)
        (directory / "Game.ini").write_text(
            f"[Game]\nTitle=GPU integration\nRGSS={version}\nScripts=Data/Scripts.{suffix}\n"
            f"RTP=\nRTP1=\nRTP2=\nRTP3=\n[GFX]\nBackend={args.backend}\n"
            "[Display]\nWidth=320\nHeight=240\nVSync=false\nBackgroundRunning=true\n",
            encoding="utf-8")
        env = {key: value for key, value in os.environ.items()
               if not key.upper().startswith(("RUBY", "URGE_", "SDL_"))}
        env.update(SDL_AUDIO_DRIVER="dummy", URGE_CRASH_DIALOG="0", URGE_CRASH_DIR=str(directory))
        print(f"Running RGSS{version}: {directory}", flush=True)
        with (directory / "engine-output.txt").open("wb") as output:
            child = subprocess.Popen([str(target)], cwd=directory, env=env,
                                     stdin=subprocess.DEVNULL, stdout=output, stderr=output)
            try:
                code = child.wait(timeout=args.timeout)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
                code = -1
        result_file = directory / "gpu-results.txt"
        result = result_file.read_text(encoding="utf-8") if result_file.exists() else "FAIL no Ruby report\n"
        log = directory / "Game.log"
        text = log.read_text(encoding="utf-8", errors="replace") if log.exists() else ""
        print(result, end="", flush=True)
        if "[GPU] validation/device error" not in text or not (directory / "validation-recovered.txt").exists():
            result += "FAIL GPU validation callback/recovery was not exercised\n"
            print("FAIL GPU validation callback/recovery was not exercised", flush=True)
        if code != 0 or "FAIL" in result or "ALL_OK" not in result:
            passed = False
            print(f"FAIL RGSS{version}; engine exit code {code}", flush=True)
    print(f"Artifacts: {root}", flush=True)
    return int(not passed)


if __name__ == "__main__":
    raise SystemExit(main())

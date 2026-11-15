#!/usr/bin/env python3
"""Pack and run the GC identity regression in a freshly built real engine."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--script", type=Path,
                        default=Path(__file__).resolve().with_name("identity.rb"))
    parser.add_argument("--ruby", default=shutil.which("ruby"))
    parser.add_argument("--output", type=Path, default=Path("build/gc-integration"))
    parser.add_argument("--rgss", type=int, nargs="+", choices=(1, 2, 3), default=(1, 3))
    parser.add_argument("--backend", default="opengl")
    parser.add_argument("--timeout", type=int, default=180)
    args = parser.parse_args()
    engine = args.engine.resolve(strict=True)
    if not args.ruby:
        parser.error("--ruby is required when Ruby is not on PATH")
    args.output.mkdir(parents=True, exist_ok=True)
    output = args.output.resolve()
    source = args.script.resolve(strict=True)
    pack_code = (
        "require 'zlib'; "
        "code = File.binread(ARGV.fetch(0)); "
        "File.binwrite(ARGV.fetch(1), Marshal.dump([[1, 'GC identity', "
        "Zlib::Deflate.deflate(code)]]))"
    )
    failures = 0
    for version in args.rgss:
        case = Path(tempfile.mkdtemp(prefix=f"rgss{version}-", dir=output))
        (case / "Data").mkdir()
        filename = "Scripts.rxdata" if version == 1 else "Scripts.rvdata" if version == 2 else "Scripts.rvdata2"
        target = case / ("Game.exe" if engine.suffix.lower() == ".exe" else "Game")
        shutil.copy2(engine, target)
        library = {1: "RGSS104E.dll", 2: "RGSS202E.dll", 3: "RGSS301.dll"}[version]
        (case / "Game.ini").write_text(
            f"[Game]\nTitle=GC identity regression\nRGSS={version}\n"
            f"Library={library}\nScripts=Data/{filename}\n"
            "RTP1=\nRTP2=\nRTP3=\n"
            f"[GFX]\nBackend={args.backend}\n"
            "[Display]\nWidth=320\nHeight=240\nVSync=false\n"
            "FrameSkip=false\nBackgroundRunning=true\n",
            encoding="utf-8",
        )
        subprocess.run(
            [str(args.ruby), "-e", pack_code, str(source), str(case / "Data" / filename)],
            check=True,
            timeout=30,
        )
        print(f"Running RGSS{version}: {case}", flush=True)
        try:
            result = subprocess.run(
                [str(target)], cwd=case, timeout=args.timeout,
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
            )
            text = result.stdout
            (case / "process.log").write_text(text, encoding="utf-8")
            results = case / "gc-results.txt"
            passed = (
                result.returncode == 0 and results.exists()
                and results.read_text(encoding="utf-8").strip() == "ALL_OK"
            )
        except subprocess.TimeoutExpired as error:
            text = error.stdout or b""
            if isinstance(text, bytes):
                text = text.decode("utf-8", errors="replace")
            (case / "process.log").write_text(text, encoding="utf-8")
            print(f"TIMEOUT RGSS{version} after {args.timeout}s", flush=True)
            passed = False
        print(text, end="" if text.endswith("\n") else "\n", flush=True)
        print(f"{'PASS' if passed else 'FAIL'} RGSS{version}", flush=True)
        failures += not passed
    return int(failures != 0)


if __name__ == "__main__":
    raise SystemExit(main())

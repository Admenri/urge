"""Run the actual native Metal executable twice, independently of WebKit."""
from pathlib import Path
import argparse
import os
import re
import shutil
import struct
import subprocess
import tempfile
import zlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", action="store_true", help="Test the signed app with an adjacent game")
    parser.add_argument("--hidpi", action="store_true", help="Check logical sizing in None/Free/Ratio modes")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[4]
    build = Path(os.environ.get("URGE_NATIVE_BUILD_DIR", repo / "build/macos-native"))
    binary = build / "app/Game"
    if not binary.exists():
        raise SystemExit("Build with ./app/platform/macos/build-native.sh first.")
    with tempfile.TemporaryDirectory(prefix="urge-native-游戏 ") as directory:
        game = Path(directory)
        (game / "Data").mkdir()
        (game / "Graphics").mkdir()
        executable = game / "Game"
        bundle = game / "URGE.app"
        if args.bundle:
            shutil.copytree(build / "URGE.app", bundle)
            executable = bundle / "Contents/MacOS/Game"
        else:
            shutil.copy2(binary, executable)
        ini = (repo / "test/text_input/Game.ini").read_text()
        ini = ini.replace("[Display]", "[Display]\nBackgroundRunning=true")
        ini = ini.replace("Backend=", "Backend=metal")
        (game / "Game.ini").write_text(ini)
        script = repo / "app/platform/macos/tests/smoke.rb"
        if args.hidpi:
            combined = game / "hidpi_smoke.rb"
            combined.write_text((repo / "app/platform/macos/tests/hidpi.rb").read_text() + "\n" + script.read_text())
            script = combined
        subprocess.run(["ruby", str(repo / "test/text_input/pack_scripts.rb"),
                        str(script), str(game / "Data/Scripts.rxdata")], check=True)

        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

        png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
               + chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\xff")) + chunk(b"IEND", b""))
        (game / "Graphics/check.png").write_bytes(png)
        modes = [mode for mode in ("None", "Free", "Ratio") for _ in (1, 2)] if args.hidpi else [None, None]
        for count, mode in enumerate(modes, start=1):
            if mode:
                (game / "Game.ini").write_text(ini + f"\n[Window]\nResizable={mode}\n")
                (game / "hidpi_mode").write_text(mode)
            try:
                result = subprocess.run([str(executable)], stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True, timeout=30)
            except subprocess.TimeoutExpired as error:
                print((error.stdout or b"").decode(errors="replace"), flush=True)
                raise
            print(result.stdout, flush=True)
            assert result.returncode == 0, f"Native app exited with {result.returncode}"
            assert "Backend: Metal" in result.stdout
            assert "SMOKE_PASS" in result.stdout and f"persisted_run={count}" in result.stdout
            assert (game / "Smoke.rvdata2").is_file()
            if mode:
                assert f"HIDPI_PASS mode={mode}" in result.stdout
                metrics = re.findall(r"window (\d+)x(\d+) points, drawable (\d+)x(\d+) pixels, density ([\d.]+)", result.stdout)
                assert metrics, "No drawable sizing diagnostics"
                expected_density = os.environ.get("URGE_TEST_PIXEL_DENSITY")
                for width, height, pixels_w, pixels_h, density in metrics:
                    density = float(density)
                    assert abs(int(pixels_w) - int(width) * density) <= 1
                    assert abs(int(pixels_h) - int(height) * density) <= 1
                    if expected_density:
                        assert abs(density - float(expected_density)) < 0.01
        if args.bundle:
            subprocess.run(["/usr/bin/codesign", "--verify", "--deep", "--strict", str(bundle)], check=True)
            print("PASS: adjacent game, saves outside bundle and intact code signature")
        if args.hidpi:
            print("PASS: logical window sizes, Retina drawable sizes and resize modes across six launches")
        print("PASS: native Metal, Ruby 4, GPU readback, PNG decoding and saves across two launches")


if __name__ == "__main__":
    main()

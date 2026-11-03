"""Run the actual native Metal executable twice, independently of WebKit."""
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib


def main():
    repo = Path(__file__).resolve().parents[4]
    binary = repo / "build/macos-native/app/Game"
    if not binary.exists():
        raise SystemExit("Build with ./app/platform/macos/build-native.sh first.")
    with tempfile.TemporaryDirectory(prefix="urge-native-游戏 ") as directory:
        game = Path(directory)
        (game / "Data").mkdir()
        (game / "Graphics").mkdir()
        shutil.copy2(binary, game / "Game")
        ini = (repo / "test/text_input/Game.ini").read_text()
        ini = ini.replace("[Display]", "[Display]\nBackgroundRunning=true")
        ini = ini.replace("Backend=", "Backend=metal")
        (game / "Game.ini").write_text(ini)
        subprocess.run(["ruby", str(repo / "test/text_input/pack_scripts.rb"),
                        str(repo / "app/platform/macos/tests/smoke.rb"), str(game / "Data/Scripts.rxdata")], check=True)

        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

        png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
               + chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\xff")) + chunk(b"IEND", b""))
        (game / "Graphics/check.png").write_bytes(png)
        for count in (1, 2):
            try:
                result = subprocess.run([str(game / "Game")], stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, text=True, timeout=30)
            except subprocess.TimeoutExpired as error:
                print((error.stdout or b"").decode(errors="replace"), flush=True)
                raise
            print(result.stdout, flush=True)
            assert result.returncode == 0, f"Native app exited with {result.returncode}"
            assert "Backend: Metal" in result.stdout
            assert "SMOKE_PASS" in result.stdout and f"persisted_run={count}" in result.stdout
            assert (game / "Smoke.rvdata2").is_file()
        print("PASS: native Metal, Ruby 4, GPU readback, PNG decoding and saves across two launches")


if __name__ == "__main__":
    main()

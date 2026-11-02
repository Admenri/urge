"""Run the real macOS app twice: WebGPU readback, lazy PNG, Ruby and saves."""
import hashlib
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib


def main():
    repo = Path(__file__).resolve().parents[4]
    binary = repo / "build/macos-webview/URGE.app/Contents/MacOS/URGE"
    if not binary.exists():
        raise SystemExit("Build the app with ./app/platform/macos/build.sh first.")
    with tempfile.TemporaryDirectory(prefix="urge-smoke-游戏 ") as directory:
        game = Path(directory).resolve()
        (game / "Data").mkdir()
        (game / "Graphics").mkdir()
        shutil.copy2(repo / "test/text_input/Game.ini", game / "Game.ini")
        ini = game / "Game.ini"
        ini.write_text(ini.read_text().replace("[Display]", "[Display]\nBackgroundRunning=true"))
        subprocess.run(["ruby", str(repo / "test/text_input/pack_scripts.rb"),
                        str(repo / "app/platform/macos/tests/smoke.rb"), str(game / "Data/Scripts.rxdata")], check=True)

        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

        png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 6, 0, 0, 0))
               + chunk(b"IDAT", zlib.compress(b"\0\xff\0\0\xff")) + chunk(b"IEND", b""))
        (game / "Graphics/check.png").write_bytes(png)
        # Foundation canonicalizes /private/var to /var when hashing game paths.
        identity = str(game)
        if identity.startswith("/private/var/"):
            identity = identity[len("/private"):]
        save_root = Path.home() / "Library/Application Support/URGE/Saves" / hashlib.sha256(identity.encode()).hexdigest()
        try:
            for count in (1, 2):
                try:
                    result = subprocess.run([str(binary), "--game", str(game), "--smoke-test"],
                                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=45)
                except subprocess.TimeoutExpired as error:
                    print((error.stdout or b"").decode(errors="replace"), flush=True)
                    raise
                print(result.stdout, flush=True)
                assert result.returncode == 0, f"App exited with {result.returncode}"
                assert "SMOKE_PASS" in result.stdout and f"persisted_run={count}" in result.stdout
                assert "[saved] /Smoke.rvdata2" in result.stdout
                assert not (save_root / "Graphics/check.png").exists(), "Lazy assets must not become saved overrides"
            print("PASS: two app launches; GPU readback, lazy assets, Ruby 4 and persistent saves", flush=True)
        finally:
            if save_root.exists():
                shutil.rmtree(save_root)


if __name__ == "__main__":
    main()

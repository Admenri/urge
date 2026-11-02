"""Integration checks for the local file boundary and save overlay.

Run: python3 -m unittest discover -s app/platform/macos/tests -v
"""
import http.client
import json
from pathlib import Path
import select
import subprocess
import tempfile
import unittest
from urllib.parse import urlsplit


class LocalServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        repo = Path(__file__).resolve().parents[4]
        cls.web, cls.game, cls.saves = [cls.root / name for name in ("web", "game", "saves")]
        for root in (cls.web, cls.game, cls.saves):
            root.mkdir()
        (cls.web / "Game.html").write_text("<script type='text/javascript' src=\"urge-helper.js\"></script>")
        (cls.web / "Game.wasm").write_bytes(b"\0asm")
        (cls.game / "Game.ini").write_text("[Game]\nTitle=Test")
        (cls.game / "Asset.png").write_bytes(b"image")
        (cls.root / "private.txt").write_text("must not be exposed")
        (cls.game / "escape").symlink_to(cls.root, target_is_directory=True)
        binary = cls.root / "server"
        subprocess.run(["swiftc", str(repo / "app/platform/macos/LocalServer.swift"),
                        str(repo / "app/platform/macos/tests/ServerHarness.swift"), "-o", str(binary)], check=True)
        cls.process = subprocess.Popen([str(binary), str(cls.web), str(cls.game), str(cls.saves)],
                                       stdout=subprocess.PIPE, text=True)
        if not select.select([cls.process.stdout], [], [], 10)[0]:
            cls.process.kill()
            raise RuntimeError("Server did not start")
        cls.url = urlsplit(cls.process.stdout.readline().strip())
        cls.base = cls.url.path.rsplit("/", 1)[0] + "/"
        cls.origin = f"http://{cls.url.netloc}"

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        cls.process.wait(timeout=10)
        cls.process.stdout.close()
        cls.temp.cleanup()

    def request(self, method, path, body=None, headers=None, raw=False):
        connection = http.client.HTTPConnection(self.url.hostname, self.url.port, timeout=5)
        connection.request(method, path if raw else self.base + path, body=body, headers=headers or {})
        response = connection.getresponse()
        result = response.status, dict(response.getheaders()), response.read()
        connection.close()
        return result

    def test_engine_mime_and_host_script(self):
        status, headers, body = self.request("GET", "Game.wasm")
        self.assertEqual((status, headers["Content-Type"], body), (200, "application/wasm", b"\0asm"))
        self.assertIn(b'<script src="host.js"></script>', self.request("GET", "Game.html")[2])
        self.assertEqual(self.request("HEAD", "Game.wasm")[2], b"")

    def test_manifest_and_source_assets(self):
        status, _, body = self.request("GET", "async_mapping.js")
        entries = json.loads(body.decode().removeprefix("const asyncMappingData = ").strip().removesuffix(";"))
        self.assertEqual(status, 200)
        self.assertIn("game.ini", entries, entries)
        self.assertTrue(entries["game.ini"]["preload"])
        self.assertFalse(entries["asset.png"]["preload"])
        self.assertEqual(self.request("GET", "assets/Asset.png")[2], b"image")

    def test_save_overlay_and_origin(self):
        data = b"saved-game-contents"
        self.assertEqual(self.request("PUT", "save/Slot01.rvdata2", data)[0], 403)
        self.assertEqual(self.request("PUT", "save/Slot01.rvdata2", data, {"Origin": "https://example.com"})[0], 403)
        self.assertEqual(self.request("PUT", "save/Slot01.rvdata2", data, {"Origin": self.origin})[0], 204)
        self.assertEqual((self.saves / "Slot01.rvdata2").read_bytes(), data)
        self.assertFalse((self.game / "Slot01.rvdata2").exists())
        self.assertEqual(self.request("GET", "assets/Slot01.rvdata2")[2], data)

    def test_traversal_symlink_and_host_are_rejected(self):
        for path in ("assets/../../private.txt", "assets/%2e%2e/private.txt", "assets/escape/private.txt", "../private.txt"):
            self.assertEqual(self.request("GET", path)[0], 403, path)
        self.assertEqual(self.request("GET", "/wrong-token/Game.wasm", raw=True)[0], 403)
        self.assertEqual(self.request("GET", "Game.wasm", headers={"Host": "attacker.example"})[0], 403)
        self.assertEqual(self.request("PUT", "save/../outside", b"bad", {"Origin": self.origin})[0], 403)


if __name__ == "__main__":
    unittest.main()

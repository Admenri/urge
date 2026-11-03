# macOS host (Wasm + WKWebView)

Requires macOS 26.0 or later with a usable WebGPU adapter. The host uses public
AppKit, WebKit and Network APIs; it does not enable experimental WebKit flags.

Build dependencies: Xcode Command Line Tools, CMake 3.30+, Ninja, Python 3,
Ruby (to pack the bundled demo), and Emscripten on `PATH`.

```sh
source /path/to/emsdk/emsdk_env.sh
./app/platform/macos/build.sh
open build/macos-webview/URGE.app
```

The first launch runs the bundled TextInput demo. Use **File → Open Game…**
(⌘O) to select a game directory containing `Game.ini`, or launch from a terminal:

```sh
build/macos-webview/URGE.app/Contents/MacOS/URGE --game /path/to/game
```

The host serves its bundled engine and the selected game over an ephemeral
loopback-only HTTP listener. It generates the existing asset manifest at runtime;
changing games does not require rebuilding the engine. A per-session URL token,
Host/Origin checks and canonical path checks limit access to those roots.
No external server or Python installation is needed to run the built app.

Writes to regular game files are persisted when the Wasm file is closed, under
`~/Library/Application Support/URGE/Saves/<game-directory-hash>/`. On the next
launch these files override the original assets and are preloaded. The source
game directory is left untouched. Reload and Quit wait for queued writes.
File deletion/renaming and writes to files left open by the game are not yet
persisted; avoid using this build as the only copy of an existing save.

The current Emscripten build disables the engine's native video module.
Windows DLLs/Win32API are unavailable. Actual game compatibility still needs
testing; the bundled demo is not a full RGSS compatibility suite.

After an initial build, `./app/platform/macos/build.sh --skip-wasm` rebuilds only the native
host and repackages the existing web files. Output is ad-hoc signed for local
use; distribution requires Developer ID signing and notarization.

Validation:

```sh
python3 -m unittest discover -s app/platform/macos/tests -v
python3 app/platform/macos/tests/run_smoke.py
```

The second command launches the actual app twice. It checks Ruby 4, 120 rendered
frames, GPU pixel readback, lazy PNG loading and save restoration after restart.
The smoke fixture allows background execution so switching to another app does
not pause the test. Interactive games retain their own background-pause setting.

## Experimental native Metal runner

The engine can also be built as a native arm64 executable using wgpu-native's
Metal backend and an SDL Cocoa window. This is independent of WKWebView and
does not require Emscripten. The deployment target remains macOS 26.0.

With Xcode, CMake, Ninja, Rust, Python 3, Ruby, pkg-config and the native
dependencies (including OpenSSL and libpng) available:

```sh
./app/platform/macos/build-native.sh
build/macos-native/demo/Game
python3 app/platform/macos/tests/run_native_smoke.py
```

The smoke test runs the native executable twice and checks the Metal backend,
Ruby 4, PNG decoding, 120 rendered frames, GPU pixel readback, save restoration
and clean exit. The executable reads `Game.ini` and game files from its own
directory. To run another game, copy `build/macos-native/app/Game` into that
game's directory.

This is a local development build: it links some Homebrew dynamic libraries,
which are not bundled for distribution. HTTP gzip/deflate decompression is
disabled (`CURL_ZLIB=OFF`) to avoid libcurl's configure checks referencing the
in-tree zlib target from a separate CMake test project. The engine's zlib and
RGSS script compression remain available. Native video and networking are
compiled, but are not covered by the graphics smoke test.

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
does not require Emscripten. Its deployment target is macOS 13.3; the
Wasm/WKWebView host above still requires macOS 26.0.

Build tools: Xcode, CMake 3.30+, Ninja, Rust, Python 3.12+ and Ruby.
Homebrew libraries and pkg-config are not required. Tools may be installed
by any method; they are not needed to run the resulting executable.

```sh
./app/platform/macos/build-native.sh
build/macos-native/demo/Game
python3 app/platform/macos/tests/run_native_smoke.py
python3 app/platform/macos/tests/run_native_dependencies.py
```

The default output directory is `build/macos-native`, targeting macOS 13.3.
Set `URGE_MACOS_DEPLOYMENT_TARGET` to select another target, or
`URGE_NATIVE_BUILD_DIR` to use a separate build directory; pass the same build
directory to the integration tests. Changing targets automatically discards
stale dependency probes. All dependency libraries and the Rust backend are
rebuilt with the selected target. Availability diagnostics
are errors, and the final audit also checks deployment records inside static
archives. The current arm64 build passes the graphics and dependency
integration tests on macOS 26.6.2. It has not yet been run on macOS 13.3; these
checks cannot replace testing on that system. The 13.3 floor comes from the
system libc++ floating-point formatting used by `std::format`.

The smoke test runs the native executable twice and checks the Metal backend,
Ruby 4, PNG decoding, 120 rendered frames, GPU pixel readback, save restoration
and clean exit. The executable reads `Game.ini` and game files from its own
directory. To run another game, copy `build/macos-native/app/Game` into that
game's directory.

The build prepares static image and network libraries in
`build/macos-deps/<architecture>-13.3/prefix`. Existing submodules supply zlib,
libpng, WebP, libavif and AOM; additional source versions and SHA-256 checksums
are pinned in `dependencies.json`. First builds download OpenSSL, Brotli, Zstd
and nghttp2; later builds reuse the verified archives and compiled libraries.
For a network requiring the local proxy, use:

```sh
HTTPS_PROXY=http://127.0.0.1:10801 HTTP_PROXY=http://127.0.0.1:10801 ./app/platform/macos/build-native.sh
```

The executable links only macOS system libraries and frameworks; the build
rejects other runtime library paths. No dependency prefix, Homebrew installation
or external CA bundle needs to accompany `Game`. Dependency license files are
copied to `build/macos-native/licenses/`; retain the applicable notices when
distributing a build. HTTPS/WSS use statically linked OpenSSL with Apple's
system certificate trust store; IDN uses Apple's ICU implementation. HTTP/2,
WebSockets, gzip, zlib/raw deflate, Brotli and Zstd remain enabled. Ruby's full
zlib extension (including gzip readers/writers) and RGSS script compression
also remain available. Fetch automatically advertises supported encodings and
returns the decoded body; decompression/transport errors leave `success?` false
even when the server returned HTTP 200. Native video is compiled but not covered
by these tests.

The dependency integration test uses local HTTP/TLS/HTTP2/WebSocket servers. It
checks binary response decompression, stacked encodings, redirects, compression
errors, certificate verification, WebSocket echo, Ruby zlib streams/files and
PNG/JPEG/TIFF/WebP/AVIF decoding. The engine runs with filesystem access to
Homebrew and the dependency prefix denied, also exercising lazy decoder loads.
The executable is built for the current architecture; Intel builds have not
been validated on hardware. Signing/notarization for distribution remains a
separate step.

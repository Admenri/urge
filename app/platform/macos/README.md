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
open build/macos-native/URGE.app
# Or: build/macos-native/demo/Game
python3 app/platform/macos/tests/run_native_smoke.py
python3 app/platform/macos/tests/run_native_smoke.py --bundle
python3 app/platform/macos/tests/run_native_dependencies.py
```

Both hosts use the existing URGE logo as their Finder/Dock application icon,
generated as a multi-resolution `.icns` from `app/resources/urge_favicon.png`.
The standalone native `Game` also embeds the 512-pixel logo for its default
runtime Dock icon, so it needs no external icon file. Games can still override
that icon with `URGE.set_window_icon`. The bare file's Finder icon remains the
system executable icon; the `.app` provides the portable Finder icon.

`build/macos-native/URGE.app` contains the native engine and demo, independent
of the WebView app. Use **File → Open Game…** (⌘O) in the native app to select a folder containing
`Game.ini`. The current game exits through normal teardown before the engine
restarts in the selected directory. The standalone `Game` executable keeps its
standard menu and executable-directory game layout.

The native app also accepts `--game /path/to/game` when its `Contents/MacOS/Game`
executable is launched from a terminal. An explicit selection takes precedence
over a game beside the app. If `Game.ini` is beside the native app, it runs that
game and writes logs/saves beside it. Otherwise it prepares its bundled demo under
`~/Library/Application Support/URGE/NativeDemo/`, preserving user preferences
and saves while refreshing the bundled script. Running the app does not write
to its signed contents. The bundle smoke test checks adjacent game discovery,
rendering/saves over two launches and signature validity afterward.

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

## Local signing and Gatekeeper

Both build scripts ad-hoc sign their `.app` output for local development. For a
standalone executable copied to a game directory, or a bundle whose contents you
have changed, sign the final copy and verify it:

```sh
# Standalone native executable (use the path of the copy you actually run).
codesign --force --sign - /path/to/game/Game
codesign --verify --strict --verbose=2 /path/to/game/Game

# Native app. For WebView, use build/macos-webview/URGE.app instead.
codesign --force --sign - build/macos-native/URGE.app
codesign --verify --deep --strict --verbose=2 build/macos-native/URGE.app
```

`--sign -` creates an **ad-hoc** signature; it requires no paid certificate.
Sign after changing the executable, `Info.plist`, icon or other bundle resources.
For these bundles, signing the `.app` signs its main executable and seals its
resources; `--deep` above is used for verification, not for signing. Rebuilds
replace the output and the build script signs it again.

A valid local signature does **not** establish a Developer ID identity or Apple
notarization, and it does not guarantee Gatekeeper approval of downloaded or
quarantined copies. For your own trusted build that macOS blocks, first try to
open it, then use **System Settings → Privacy & Security → Open Anyway** for that
specific app if the system offers the option. Do not disable Gatekeeper globally.
See Apple's [opening apps safely](https://support.apple.com/en-gb/102445).

You can inspect quarantine metadata and Gatekeeper's assessment without changing
system security settings:

```sh
xattr -p com.apple.quarantine build/macos-native/URGE.app
spctl --assess --type execute --verbose=2 build/macos-native/URGE.app
```

A missing quarantine attribute is normal for a local build. An ad-hoc build may
be rejected by `spctl` even though `codesign --verify` succeeds; these checks
answer different questions. Public distribution requires a Developer ID
Application signature and notarization, as described in Apple's
[distribution guide](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).

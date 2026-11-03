# macOS native Metal runner

The macOS app and standalone executable use wgpu-native’s Metal backend and
an SDL Cocoa window. The deployment target is macOS 13.3.

Build tools: Xcode, CMake 3.30+, Ninja, Rust, Python 3.12+.
Homebrew libraries and pkg-config are not required. Tools may be installed
by any method; they are not needed to run the resulting executable.

```sh
./app/platform/macos/build-native.sh
open build/macos-native/URGE.app
# Or: copy build/macos-native/app/Game into a game directory and run it there.
```

The app uses the existing URGE logo as its Finder/Dock application icon,
generated as a multi-resolution `.icns` from `app/resources/urge_favicon.png`.
The standalone native `Game` also embeds the 512-pixel logo for its default
runtime Dock icon, so it needs no external icon file. Games can still override
that icon with `URGE.set_window_icon`. The bare file's Finder icon remains the
system executable icon; the `.app` provides the portable Finder icon.

`build/macos-native/URGE.app` contains the native engine. Use
**File → Open Game…** (⌘O) in the native app to select a folder containing
`Game.ini`. The current game exits through normal teardown before the engine
restarts in the selected directory. The standalone `Game` executable keeps its
standard menu and executable-directory game layout.

The native app also accepts `--game /path/to/game` when its `Contents/MacOS/Game`
executable is launched from a terminal. An explicit selection takes precedence
over a game beside the app. If `Game.ini` is beside the native app, it runs that
game and writes logs/saves beside it. Otherwise the app opens a folder picker
at startup. Choose a directory containing `Game.ini`; canceling exits normally.
No demo is bundled, and the app does not write to its signed contents.

The default output directory is `build/macos-native`, targeting macOS 13.3.
Set `URGE_MACOS_DEPLOYMENT_TARGET` to select another target, or
`URGE_NATIVE_BUILD_DIR` to use a separate build directory. Changing targets
automatically discards stale dependency probes. All dependency libraries and the Rust backend are
rebuilt with the selected target. Availability diagnostics
are errors, and the final audit also checks deployment records inside static
archives. The current arm64 build passes the graphics and dependency
integration tests on macOS 26.6.2. It has not yet been run on macOS 13.3; these
checks cannot replace testing on that system. The 13.3 floor comes from the
system libc++ floating-point formatting used by `std::format`.

The executable reads `Game.ini` and game files from its own directory. To run
another game, copy `build/macos-native/app/Game` into that game's directory.

The build prepares static image and network libraries in
`build/macos-deps/<architecture>-13.3/prefix`. Existing submodules supply zlib,
libpng, WebP, libavif and AOM; additional source versions and SHA-256 checksums
are pinned in
[`third_party/macos/dependencies.json`](../../../third_party/macos/dependencies.json).
Dependency preparation and auditing live under `third_party/macos/`; the macOS
dependency policy is in `third_party/CMakeLists.txt` and its finders are under
`third_party/cmake/macos/`. The prefix takes priority over installed packages;
Homebrew prefixes and package registries are excluded, pkg-config is restricted to the
prefix, and shell include/library overrides are cleared by the build script.
First builds download OpenSSL, Brotli, Zstd and nghttp2; later builds reuse the
verified archives and compiled libraries.
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

The executable is built for the current architecture; Intel builds have not
been validated on hardware. Signing/notarization for distribution remains a
separate step.

## GitHub Actions artifacts

The `Build macOS` workflow runs on pushes to `master`, pushes of `v*` tags,
and manual dispatch, matching the other platform builds. It uses an arm64
`macos-26` runner and retains the macOS 13.3 deployment target.

Two archives are uploaded separately and retained for 14 days:

- `urge-macos-arm64-game.zip`: standalone `Game`, license and dependency notices.
- `urge-macos-arm64-app.zip`: `URGE.app`, license and dependency notices.

The workflow uses the native build script, including its dependency and minimum
OS audits. It verifies executable permissions and signatures after extracting
the archives. The app is ad-hoc signed; local signing details are below.

## Native HiDPI / Retina

Window sizes (`Game.ini`, `URGE.move_window` and `URGE.get_window_rect`) use
macOS logical points. The native Metal window requests a high-density backing
buffer; only presentation uses drawable pixels. A 720×240-point window on a 2×
display therefore presents to a 1440×480-pixel drawable. macOS applies the user's
scaled display mode. Mouse coordinates are mapped from window points into the
game's logical screen, and ImGui uses the framebuffer scale for its text atlas.

The game bitmap resolution (`Graphics.width/height`, `Bitmap` and snapshots)
remains independent of the drawable size. Text drawn into a small bitmap and
then magnified is still magnified bitmap text: enabling Retina presentation
does not re-rasterize those glyphs. For example, a 320×240 game displayed in a
640×480-point window on a 2× display magnifies its bitmap pixels by 4×. A 12-pixel
glyph in that bitmap will have rough edges; this is separate from native UI text
rendered at the backing resolution.

Moving between displays is handled by SDL's backing-size notifications and
the presentation layer's drawable-size checks.

## Local signing and Gatekeeper

The build script ad-hoc signs its `.app` output for local development. For a
standalone executable copied to a game directory, or a bundle whose contents you
have changed, sign the final copy and verify it:

```sh
# Standalone native executable (use the path of the copy you actually run).
codesign --force --sign - /path/to/game/Game
codesign --verify --strict --verbose=2 /path/to/game/Game

# Native app.
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

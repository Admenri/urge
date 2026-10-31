<img src="app/resources/urge_favicon_512.png" width="96" align="left" alt="URGE logo" />

# Universal Ruby Game Engine (URGE)

**A 2D / 3D hybrid game engine compatible with RGSS1 / RGSS2 / RGSS3**

[简体中文](README.md) | English

<br clear="left" />

---

## 1. Project Overview

**URGE (Universal Ruby Game Engine)** is an RGSS runtime built from scratch. It is compatible with RPG Maker XP (RGSS1), VX (RGSS2) and VX Ace (RGSS3). The engine implements the window, renderer, input, audio, font and filesystem layers in C++20, and exposes them to an embedded CRuby interpreter through automatically generated bindings. Existing projects for these three RPG Maker versions run without script changes in principle.

WebGPU is the single rendering backend. The 2D screen and 3D world share one device, one set of resources and one frame loop, and `World3D` can composite a 3D render into the 2D scene at a chosen Z order — so classic RGSS projects can add 3D scenes, models and skeletal animation directly.

### Highlights

| Area | Details |
| --- | --- |
| **RGSS compatibility** | RGSS1 / RGSS2 / RGSS3; the version is detected from the `RGSS` field in `Game.ini` or from the script archive extension (`.rxdata` / `.rvdata` / `.rvdata2`); `Window` / `Tilemap` map to the XP or VX implementation; the per-version `RPG` compatibility modules are built in |
| **Script runtime** | Embedded CRuby 4.0.0; executes Marshal + zlib script archives directly, with no external Ruby installation; provides `rgss_main` / `rgss_stop` / `RGSSError` / `RGSSReset` and the usual RGSS entry points |
| **Rendering** | WebGPU (wgpu-native on native platforms, the browser's WebGPU on the web); WGSL shaders and custom `Effect` passes; per-frame vertex batching and a uniform pool; logical resolution decoupled from physical pixels for high-DPI displays |
| **2D** | `Bitmap` (Blt, gradients, text, pixel access), `Sprite`, `Plane`, `Viewport`, `WindowXP` / `WindowVX`, `TilemapXP` / `TilemapVX`, `Geometry`, `Animation`, sprite batching, 2D cameras |
| **3D** | `Node3D` scene graph, perspective / orthographic cameras, `Light3D` with shadows, glTF model loading, skeletal skinning, animation stacks and state machines, PBR-style materials, `RenderGraph3D`, render targets and compute passes, `World3D` composited with 2D |
| **Direct GPU access** | The `GPU` module exposes WebGPU buffers, textures, samplers, shaders, bind groups, render / compute pipelines and command encoding directly to Ruby |
| **Audio** | BGM / BGS / ME / SE; WAV, Ogg Vorbis and Ogg Opus; MIDI with SoundFont; per-channel volume and output device selection; `AudioStream` for streaming playback and control |
| **Input** | Keyboard, mouse, gamepad (multiple devices, hot-plug, rumble, triggers / sticks) and multi-touch (up to 16 fingers); key bindings are stored in `Game.ini` |
| **Fonts** | Resolution order is `Fonts/` → system fonts → embedded font; file names and family names are supported, including CJK text; Windows defaults to Microsoft YaHei |
| **Assets** | PhysicsFS virtual filesystem; automatic RTP detection on Windows; optional `.acs` encrypted containers (requires an `admenri/` drop-in) |
| **Video** | WebM / AV1 playback (desktop, can be disabled); `Graphics.play_movie` provides a movie event with seek controls |
| **Networking** | HTTP `Fetch` and `WebSocket` (desktop and web, can be disabled) |
| **Diagnostics** | F1 settings panel, F2 frame-rate panel; runtime log; Windows crash dumps and symbolicated reports; optional PGO |

### Platform Support

| Platform | Output | Graphics backend | Notes |
| --- | --- | --- | --- |
| Windows 10 1809+ (x64) | `Game.exe` | D3D12 by default; Vulkan / OpenGL and others selectable | Full feature set; video and networking on by default |
| WebAssembly (WebGPU-capable browser) | `Game.html` + `Game.wasm` | Browser WebGPU | Video off by default; networking available |
| Android 9+ (API 28, arm64-v8a / x86_64) | APK | Vulkan 1.1 | Video and networking on by default; TLS via the vendored mbedTLS |
| Linux / macOS | — | — | No build presets yet |

### Ruby Script API

Bindings are generated from export markers in `core/**/*.h`. The generated API currently contains 84 classes and 9 modules.

| Category | Classes / modules |
| --- | --- |
| Runtime modules | `Graphics`, `Input`, `Mouse`, `Touch`, `Audio`, `Network`, `Config`, `URGE`, `GPU` |
| 2D | `Node`, `Sprite`, `Plane`, `Viewport`, `WindowXP`, `WindowVX`, `TilemapXP`, `TilemapVX`, `Geometry`, `Effect`, `Camera`, `OrthographicCamera`, `PerspectiveCamera` |
| 3D | `Node3D`, `Scene3D`, `World3D`, `Camera3D`, `Light3D`, `GLTFModel3D`, `Mesh3D`, `MeshRenderer3D`, `Skeleton3D`, `BoneMask3D`, `Animation3D`, `AnimationLayer3D`, `AnimationStack3D`, `AnimationStateMachine3D`, `Material3D`, `MaterialPass3D`, `RenderTarget3D`, `ComputePass3D`, `RenderQueue3D`, `RenderContext3D`, `RenderGraph3D`, `RenderGraphPass3D`, `ForwardPipeline3D` |
| GPU | `GPUObject`, `GPUBuffer`, `GPUTexture`, `GPUTextureView`, `GPUSampler`, `GPUShaderModule`, `GPUBindGroup`, `GPURenderPipeline`, `GPUComputePipeline`, `GPUCommandEncoder`, `GPURenderPassEncoder`, `GPUComputePassEncoder` and the matching `GPU*Descriptor` classes |
| Data and canvas | `Bitmap`, `Image`, `Font`, `Color`, `Rect`, `Tone`, `Table`, `Vector2` / `Vector3` / `Vector4`, `Animation` |
| Other | `Disposable`, `Gamepad`, `AudioStream`, `Video`, `WebSocket`, `Fetch`, `NetworkEvent` |

### Repository Layout

```
urge/
├── app/              Host program (Windows / Web / Android) and entry point
├── core/             Engine core: 2D, 3D, rendering, audio, input, fonts, network, video
├── bind/             CRuby bindings, generators and the Win32API bridge
├── android/          Android Gradle project
├── third_party/      Third-party dependencies (Git submodules and vendored sources)
└── CMakeLists.txt
```

---

## 2. Building

### 2.1 Requirements

Tools needed on every platform:

| Tool | Requirement |
| --- | --- |
| CMake | ≥ 3.30; the Windows preset uses the `Visual Studio 18 2026` generator, so it needs a CMake release that supports it (a recent 4.x) |
| Python | 3.x, with `python` / `python3` / `py` on `PATH` (generates the Ruby bindings at configure time; also indexes web assets) |
| Git | Needed to fetch the submodules |
| C++ compiler | C++20 support |
| Rust | Stable toolchain. Required for wgpu-native on desktop and Android; not needed for the web build (it uses the browser's WebGPU) |
| Network | The first configure downloads Corrosion and compiles the Rust dependencies |

Platform-specific requirements:

| Platform | Install |
| --- | --- |
| **Windows** | Visual Studio 2026 (generator `Visual Studio 18 2026`) with the "Desktop development with C++" workload (MSVC x64 + Windows SDK); Rust target `x86_64-pc-windows-msvc` |
| **Web** | Emscripten SDK (run `emsdk_env`, or set `EMSDK` / `EMSCRIPTEN`); Ninja |
| **Android** | Android SDK (Platform 37, Build-Tools 36.0.0) and NDK; JDK 17 or newer; Rust targets `aarch64-linux-android` and `x86_64-linux-android`; Ninja |

> Android devices need Vulkan 1.1; the minimum Android version is 9 (API 28).

### 2.2 Get the Source

```bash
git clone --recursive https://github.com/Admenri/urge.git
cd urge
```

If the clone is missing submodules:

```bash
git submodule update --init --recursive
```

### 2.3 Windows (x64)

```bash
cmake --preset win64-vendored-ruby
cmake --build --preset win64-vendored-ruby
```

Output:

- RelWithDebInfo: `build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe`
- Debug build: `cmake --build --preset win64-vendored-ruby-debug`, output in `build/win64-vendored-ruby/app/Debug/Game.exe`

Without the preset, you can supply your own generator:

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config RelWithDebInfo --target Game
```

With a different Visual Studio version, replace `-G` / `-A` with the generator that matches your environment.

### 2.4 Web (WebAssembly)

```bash
# Linux / macOS
source /path/to/emsdk/emsdk_env.sh
# Windows
# emsdk_env.bat

cmake --preset emscripten-wasm32 -DURGE_WEB_ASSETS="/path/to/game"
cmake --build --preset emscripten-wasm32
```

`URGE_WEB_ASSETS` points at the game directory (containing `Game.ini`, `Data/`, and so on). The build output lives in `build/emscripten-wasm32/app/`:

```
Game.html  Game.js  Game.wasm  async_mapping.js  urge-helper.js  assets/
```

The game is published beside the module as `assets/`. Files matching `*.ini`, `*.acs` and `Data/Scripts.*` are preloaded before the first frame (adjust with `URGE_WEB_PRELOAD`); everything else is fetched the first time it is requested. Pass `-DURGE_WEB_EMBED_ASSETS=ON` at configure time to pack the game into a single `Game.data` instead.

Serve the directory over HTTP (`localhost` or HTTPS; browsers generally expose WebGPU only in secure contexts):

```bash
python -m http.server 8000 -d build/emscripten-wasm32/app
```

Then open `http://localhost:8000/Game.html`.

### 2.5 Android (arm64-v8a / x86_64)

Add the Rust cross-compilation targets first:

```bash
rustup target add aarch64-linux-android x86_64-linux-android
```

Build the native libraries (choose the ABIs you need):

```bash
cmake --preset android-arm64
cmake --build --preset android-arm64

# Emulators or x86_64 devices
cmake --preset android-x86_64
cmake --build --preset android-x86_64
```

The build copies `libmain.so` and `libSDL3.so` into `android/app/src/main/jniLibs/<abi>/`; Gradle only packages them.

Mount the game directory into the APK and build a debug package:

```bash
cd android
# Use gradlew.bat on Windows
./gradlew assembleDebug -P urgeGameDir="/path/to/game"
```

The APK is written to `android/app/build/outputs/apk/debug/app-debug.apk`. The game files are placed in the APK's `assets/` and unpacked into the app's private directory on first launch.

Release builds need signing parameters (`urgeStoreFile`, `urgeStorePassword`, `urgeKeyAlias`, `urgeKeyPassword`, supplied through `local.properties`, `-P` or the environment); without them the APK is left unsigned.

### 2.6 Running a Game

**Windows**: put `Game.exe` in the game directory. The engine uses the executable's directory as the working directory and reads a configuration file with the same stem:

```
MyGame/
├── Game.exe
├── Game.ini
├── Game.acs          # optional: encrypted asset container, takes priority over loose files
├── Data/
│   └── Scripts.rvdata2
├── Graphics/
├── Audio/
└── Fonts/
```

- The configuration file is `<executable stem>.ini` (`Game.exe` → `Game.ini`).
- If `<executable stem>.acs` exists and the build includes `admenri/`, that container is mounted first.
- The runtime log is written to `<executable stem>.log`.

**Web / Android**: the executable name is fixed to `Game`, so `Game.ini`, `Data/` and the rest live at the site root or under `assets/` in the APK.

Example `Game.ini`:

```ini
[Game]
RGSS = 3
Scripts = Data/Scripts.rvdata2
Title = My Game
; RTP / RTP1 / RTP2 / RTP3 are optional; Windows detects installed runtimes automatically

[Display]
Width = 544
Height = 416
VSync = true
Fullscreen = false
FrameSkip = false

[Window]
Resizable = none

[Audio]
Soundfont = Fonts/Default.sf2
MasterVolume = 100

[GFX]
Backend =
SpriteBatch = true

[Input]
PadBindings = true

[KeyBinding]
C = Space,Return,pad:a
```

Configuration reference:

| Section | Key | Default | Notes |
| --- | --- | --- | --- |
| `Game` | `RGSS` | `0` (auto) | `1` = XP, `2` = VX, `3` = VX Ace; `0` infers it from the `Scripts` extension |
| `Game` | `Scripts` | `Data/Scripts.rxdata` | Script archive path |
| `Game` | `Title` | `(*^▽^*)` | Window title |
| `Game` | `RTP` / `RTP1`–`RTP3` | empty | RPG Maker runtime names; detected automatically on Windows |
| `Display` | `Width` / `Height` | XP: `640×480`; others: `544×416` | Logical resolution |
| `Display` | `VSync` / `Fullscreen` / `FrameSkip` | `false` | Vertical sync, fullscreen and frame skipping |
| `Window` | `Resizable` | `none` | `none` / `free` / `ratio` |
| `Audio` | `Soundfont` | `Fonts/Default.sf2` | MIDI SoundFont |
| `Audio` | `MasterVolume` / `BGMVolume` / `BGSVolume` / `MEVolume` / `SEVolume` | `100` | Volumes |
| `Audio` | `OutputDevice` | empty (system default) | Audio output device |
| `GFX` | `Backend` | empty (auto) | `d3d11` / `d3d12` / `vulkan` / `opengl` / `opengles` / `metal` |
| `GFX` | `SpriteBatch` | `true` | Sprite batching |
| `Input` | `PadBindings` | `false` | Whether the default gamepad bindings have been written |
| `KeyBinding` | — | — | One line per slot; gamepad entries use `pad:<token>` |

Additional notes:

- **F1 / F2**: F1 opens the settings panel (where new key bindings can be added); F2 toggles the frame-rate display.
- **Fonts**: resolved as `Fonts/` → system fonts → embedded font; both file names (extension optional) and family names work.
- **Gamepads**: keyboard and gamepad map to the same set of slots; gamepads are hot-pluggable, with support for multiple devices, rumble, sticks and triggers.
- **Profiling**: on Windows, enable sampling through the `URGE_PROFILER` / `URGE_PROFILER_INTERVAL` / `URGE_PROFILER_DIR` environment variables, or the `Profiler` / `ProfilerInterval` keys in `[Debug]`.

### 2.7 Build Options

| Option | Default | Description |
| --- | --- | --- |
| `URGE_RUNNER_MODEL` | Desktop / Android: `poll`; Web: `fiber` | Which side owns the frame loop: the engine's main loop (`poll`) or SDL's callback loop (`fiber`) |
| `URGE_ENABLE_VIDEO` | Desktop / Android `ON`; Web `OFF` | WebM / AV1 video playback |
| `URGE_ENABLE_NETWORK` | All platforms `ON` | HTTP `Fetch` and `WebSocket` (the web build uses the browser's own implementation) |
| `URGE_WIN7_COMPAT` | `OFF` | Link Windows 7 compatibility hooks: route Win8+ imports such as `WaitOnAddress`, `ProcessPrng` and `CreateFile2` to in-image fallbacks |
| `URGE_VIDEO_AV1_8BIT_ONLY` | `OFF` | Build 8-bit AV1 only, reducing size (drops 10 / 12-bit video) |
| `URGE_WEB_ASSETS` | empty | Game directory for the web build |
| `URGE_WEB_PRELOAD` | `*.ini,*.acs,Data/Scripts.*` | Files preloaded before the first web frame |
| `URGE_WEB_EMBED_ASSETS` | `OFF` | Pack the game into a single `Game.data` |
| `URGE_BUILD_ACS_PACK` | `ON` | Build the `.acs` container packer when `admenri/` is present |
| `URGE_PGO` | `off` | Two-pass profile-guided optimization: `generate` / `use` (MSVC) |
| `URGE_EMSDK` / `URGE_ANDROID_NDK` | auto-detected | Explicit Emscripten SDK / Android NDK paths |

> `URGE_WIN7_COMPAT` removes only the Win8+ API dependencies in the import table; Windows 7 has no D3D12, so the graphics backend still needs Vulkan / OpenGL drivers.

> The Android NDK has no OpenSSL to link against, so the Android network build uses the vendored mbedTLS as libcurl's TLS backend and reads the system certificate directory `/system/etc/security/cacerts`; Windows uses Schannel and other desktop platforms use the system OpenSSL. When the video and network subsystems are built out, the corresponding Ruby modules still exist and raise on use, so the script API stays the same on every platform.

### 2.8 Third-Party Dependencies

The following dependencies are Git submodules and must be fetched with the source (`git submodule update --init --recursive`; `mbedtls` carries its own `framework` submodule):

| Submodule | Purpose |
| --- | --- |
| `glm` | Vectors, matrices and projections |
| `SDL` | Window, input, audio and platform abstraction |
| `SDL_image` | Image decoding (including libwebp) |
| `SDL_ttf` | TrueType font rasterization (including FreeType) |
| `physfs` | Virtual filesystem |
| `wgpu-native` | Native WebGPU implementation |
| `zlib` | Script archive decompression |
| `curl` | HTTP and WebSocket |
| `mbedtls` | TLS backend for libcurl on Android |
| `ogg` / `vorbis` / `opus` / `opusfile` | Ogg Vorbis and Ogg Opus decoding |

`third_party/` also vendors `ruby` (CRuby 4.0.0), `miniaudio`, `imgui`, `dav1d`, `webm`, `libffi` and `webgpu-cpp`. The web build uses Emscripten's `emdawnwebgpu` port instead of wgpu-native.

The optional `admenri/` directory is not part of the open-source repository. When present, the build enables `.acs` encrypted containers and the `urge-acs-pack` packer; without it, games run from ordinary files.

---

## 3. License

This project is released under the [MIT License](LICENSE.txt).

```
Copyright (c) 2026 Admenri Adev
```

Third-party dependencies keep their own licenses. RPG Maker runtimes (RTP) and game assets are not part of this repository and remain the property of their respective authors.

---

<div align="center">
  <sub>Licensed under the <a href="LICENSE.txt">MIT License</a> · Copyright (c) 2026 Admenri Adev</sub>
</div>

<img src="app/resources/urge_favicon_512.png" width="100" align="left" alt="URGE logo" />

# URGE Core

**Universal Ruby Game Engine** · an RGSS runtime built on WebGPU

English | [简体中文](README.md)

<br clear="left" />

---

## 1. Overview

**URGE (Universal Ruby Game Engine)** is an RGSS (Ruby Game Scripting System) runtime
for Windows. It is written from scratch in modern C++ (C++20), uses WebGPU as its
hardware-accelerated graphics backend, and is compatible with the scripting APIs of
RPG Maker XP (RGSS1), VX (RGSS2) and VX Ace (RGSS3) — existing projects generally run
without any script changes.

Every runtime facility (window, rendering, input, audio, fonts, filesystem) is
implemented in C++ and exposed to an embedded CRuby interpreter through an
automatically generated binding layer. From the script's point of view the familiar
RGSS constants — `Bitmap`, `Sprite`, `Viewport`, `Graphics`, and so on — are still
what you work with.

### Highlights

| Feature | Description |
| --- | --- |
| RGSS1 / 2 / 3 compatible | The version is derived from the `RGSS` key in `Game.ini`, or inferred from the archive extension |
| WebGPU backend | Built on wgpu-native; every render target is `RGBA8Unorm` and the present pass compensates for sRGB |
| 3D transform node tree | A single `Node` base class with parent/child hierarchy, position/rotation/scale and Z ordering |
| Custom shaders | `Effect` attaches an arbitrary WGSL fragment shader to a node or to a subtree |
| Embedded CRuby | Runs RPG Maker script archives (Marshal + zlib) directly, with no external Ruby installation |
| Generated bindings | The CRuby glue is mechanically generated from the export blocks in `core/**/*.h`; signatures are the contract |
| High pixel density | Logical resolution is decoupled from physical pixels, so the window scales correctly on HiDPI displays |
| Virtual filesystem | Scripts, assets and RTP paths are mounted through PhysicsFS |

### Technology stack

| Component | Source | Purpose |
| --- | --- | --- |
| Language | C++20 | Engine and host |
| Window / input / audio | SDL3, SDL_image, SDL_ttf | Platform abstraction, image decoding, font rasterisation |
| Graphics API | WebGPU (wgpu-native) | Rendering backend |
| Shader toolchain | none | WGSL goes straight to wgpu; a small in-tree reflector derives the descriptor layout |
| Scripting runtime | CRuby 1.9.3 (`third_party/cruby`) | Executes RGSS scripts |
| Virtual filesystem | PhysicsFS (physfs) | Asset and RTP mounting |
| Math | glm | Vectors, matrices, projections |
| Compression | zlib | Script-archive decompression |

### Repository layout

```
urge/
├── app/              Host program
│   ├── app.cc        Entry point: initialises the singletons, then enters the script layer
│   ├── platform/     Platform-specific code (Win32 RTP path discovery, etc.)
│   └── resources/    Application icons and Windows version resource
├── core/             Engine core: all runtime facilities, independent of any script
├── bind/             CRuby bindings: generated glue plus a hand-written runtime layer
│   └── viewer/       Read-only API viewer built on the IR
├── third_party/      Third-party dependencies (mostly Git submodules)
├── test/             Tests
└── CMakeLists.txt
```

---

## 2. Building

### Requirements

| Item | Requirement |
| --- | --- |
| Operating system | **Windows 10 1809 or newer (x64)**. wgpu defaults to the DX12 backend on Windows, so Windows 7 / 8 are not supported |
| Compiler | Visual Studio 2022 or newer (MSVC, with C++20 support) |
| Build system | CMake ≥ 3.30 |
| Rust | A stable toolchain (wgpu-native is built from Cargo via Corrosion) |
| Version control | Git (dependencies are provided as submodules) |

### Clone and build

```bash
git clone --recursive https://github.com/Admenri/urge.git
cd urge
cmake -S . -B build
cmake --build build --config Release --target Game
```

> `--recursive` is required. Every third-party dependency is either a submodule or
> vendored in-tree; when they are missing, CMake fails during configuration.

The artifacts are written to:

- Release: `build/app/Release/Game.exe`
- Debug: `build/app/Debug/Game.exe`

### Incremental builds

Both `core/` and `bind/` collect their sources with `file(GLOB ...)`, and CMake freezes
that file list at **configure time**. Therefore:

- **After adding or removing a source file**, re-run `cmake -S . -B build`. Otherwise the
  new file is never compiled, or the link step fails with `LNK2019` / `LNK2001`.
- When only the *contents* of existing files change, a plain `cmake --build` is enough —
  no re-configure is needed.

### Running

Place `Game.exe` in the game project directory (the one containing `Game.ini` and
`Data/`) and launch it. The engine uses the executable's directory as its working
directory and reads configuration from `<executable-name>.ini`.

Example `Game.ini`:

```ini
[Game]
RGSS = 3
Scripts = Data/Scripts.rvdata2
Title = My Game
; RTP / RTP1 / RTP2 / RTP3 are optional; installed runtimes are auto-detected on Windows

[Window]
Width = 640
Height = 480

[Audio]
Soundfont = Fonts/Default.sf2

[GFX]
Backend =
```

Configuration keys:

| Section | Key | Default | Description |
| --- | --- | --- | --- |
| `Game` | `RGSS` | `0` (auto) | `1` = XP, `2` = VX, `3` = VX Ace; when `0`, inferred from the `Scripts` extension |
| `Game` | `Scripts` | `Data/Scripts.rxdata` | Path to the script archive |
| `Game` | `Title` | `(*^▽^*)` | Window title |
| `Game` | `RTP` / `RTP1`–`RTP3` | empty | RPG Maker runtime names |
| `Window` | `Width` / `Height` | XP: `640x480`, otherwise `544x416` | Logical resolution |
| `Audio` | `Soundfont` | `Fonts/Default.sf2` | MIDI soundfont |
| `GFX` | `Backend` | empty (auto) | wgpu backend selection |

### Fonts

The Ruby `Font` class and the debug overlay (F1 / F2) share one font service,
`core/font_context.*`, so both always draw from the same face. A name is resolved in this
order, first hit wins:

1. `Fonts/` — faces shipped with the game (read through the load paths, so an `.acs`
   archive can carry them);
2. system fonts — on Windows `%WINDIR%\Fonts` and
   `%LOCALAPPDATA%\Microsoft\Windows\Fonts`;
3. the face compiled into the executable (`core/resources/embed.ttf.bin`), always
   available as the last resort.

A face may be named by file (`msyh.ttc`, `simhei.ttf`, extension optional) or by family
(`Microsoft YaHei`, `微软雅黑`, `SimHei`); the family index is built only on the first
lookup that misses the file-name map, and reads nothing but each font's `name` table. On
Windows the default face is **Microsoft YaHei** (`msyh.ttc`), falling back through
`msyh.ttf`, `Deng.ttf`, `simhei.ttf` and `simsun.ttc` to the embedded face; other
platforms still default to the embedded face. A file of the same name under `Fonts/`
shadows the system one. Payloads are read on demand and kept for the lifetime of the
context, so the script side and the overlay share one copy.

### Input and gamepads

`core/input/` drives the keyboard and the controller into one set of slots (`A` / `B` / `C` /
`X` / `Y` / `Z` / `L` / `R` and `DOWN` / `LEFT` / `RIGHT` / `UP`). A pad works as soon as it is
plugged in -- hot-plug included -- and only one pad is taken over at a time.

| Pad button | Slot |
| --- | --- |
| A (south) / B (east) / X (west) / Y (north) | `C` / `B` / `A` / `X` |
| LB / RB | `L` / `R` |
| D-pad, left stick | `LEFT` / `RIGHT` / `UP` / `DOWN` |

The left stick drives the direction slots alongside the d-pad (50% deadzone), so both feel
native. Bindings live in the `[KeyBinding]` section of `Game.ini`, with pad entries written as
`pad:<token>` (for example `C=Space,Return,pad:a`); `[Input] PadBindings=true` marks a file
written by a build that knows about pads -- without it the pad defaults are added on load, which
is how an old save file is upgraded. In the F1 panel, press `Add` and then a pad button to bind
it to that slot.

> A Bluetooth Xbox controller needs a one-line local patch in `third_party/SDL`: upstream
> widened the "fake HID endpoint" test until it also rejected the real endpoint of a Bluetooth
> pad. The reason and the verification are in `third_party/PATCHES.md`.

Advanced use goes through the `Gamepad` class: one object is one controller, it owns its
`SDL_Gamepad`, and a script may hold several at once.

```ruby
Gamepad.available_devices       # => [{ "id" => 2, "name" => "Xbox Series X Controller",
                                #       "path" => "...", "player" => 0, "guid" => "..." }, ...]
pad = Gamepad.new(2)            # open the id reported above
pad.update                      # once per frame; button_trigger?/button_repeat? depend on it

pad.button_press?(Gamepad::BUTTON_SOUTH)
pad.button_trigger?(Gamepad::BUTTON_SOUTH)
pad.button_repeat?(Gamepad::BUTTON_SOUTH)
pad.get_axis_value(Gamepad::AXIS_LEFTX)     # raw SDL value: sticks -32768..32767, triggers 0..32767
pad.pressed_buttons             # => [button, ...]
pad.connect_state               # SDL connection state (0 = none, 1 = wired, 2 = wireless)
pad.connect                     # re-attach after a replug, matched by GUID
pad.player_index                # readable and writable
pad.rumble(0.5, 0.5, 200)       # engine extension: low, high, milliseconds
```

The constants are SDL's numbers: `Gamepad::AXIS_*` (`LEFTX`, `LEFTY`, `RIGHTX`, `RIGHTY`,
`LEFT_TRIGGER`, `RIGHT_TRIGGER`) and `Gamepad::BUTTON_*` (`SOUTH`, `EAST`, `WEST`, `NORTH`,
`BACK`, `GUIDE`, `START`, `LEFT_STICK`, `RIGHT_STICK`, `LEFT_SHOULDER`, `RIGHT_SHOULDER`,
`DPAD_*`, `MISC1..6`, `RIGHT_PADDLE1/2`, `LEFT_PADDLE1/2`, `TOUCHPAD`). `Gamepad.new` raises
`RGSSError` when the device is already gone.

### Third-party dependencies

These dependencies are provided as Git submodules and must be fetched while cloning:

| Submodule | Purpose |
| --- | --- |
| `glm` | Math library |
| `SDL` | Window, input, audio and platform abstraction |
| `SDL_image` | Image decoding |
| `SDL_ttf` | TrueType rasterisation (bundles FreeType / HarfBuzz / PlutoSVG) |
| `physfs` | Virtual filesystem |
| `wgpu-native` | WebGPU implementation (Rust) |
| `zlib` | Script-archive decompression |

In addition, `third_party/` contains several **vendored** dependencies (not submodules):
`cruby` (CRuby 1.9.3), `libffi` (the foreign-function interface the `Win32API` binding
uses to make ABI-correct calls on x64), `webgpu-cpp` (the WebGPU C++ header wrapper),
`miniaudio`, `imgui`, `dav1d` and `webm`.

The decoding libraries travel with their only consumer: `libwebp` lives in
`third_party/SDL_image/external/`, and `ogg` / `vorbis` / `opus` / `opusfile` live in
`third_party/miniaudio/external/`.

`libffi` takes part in the build on Windows only (see `third_party/CMakeLists.txt`):
its sole consumer is `bind/binding_win32api.cc`, and that binding compiles to an empty
implementation elsewhere. Its build script is a line-for-line CMake translation of the
upstream autotools project (`configure.ac` / `configure.host` / `Makefile.am`) rather
than a redesign of it.

---

## 3. Architecture

### Layers

The project is organised into four layers with a strictly top-down dependency
direction:

```mermaid
graph TD
  A["app/<br/>Host · Game.exe"] --> B["bind/<br/>CRuby bindings · urge-binding"]
  B --> C["core/<br/>Engine core · urge-core"]
  C --> D["third_party/<br/>Dependencies"]
  B --> D
```

- **`app/`** — the host program. `app.cc` is the only entry point: it resolves the
  executable path and `Game.ini`, initialises the `IOService`, `Config`, `Input`,
  `Mouse`, `FontContext` and `Graphics` singletons in dependency order, then enters
  `binding::BindingMain`; on shutdown it releases them in reverse order.
- **`bind/`** — the CRuby binding static library. It depends on `urge-core` but contains
  no entry point; the host calls `binding::InitBindings()` after `ruby_init()`.
- **`core/`** — the engine core, built as the static library `urge-core`, with no
  dependency on Ruby of any kind.
- **`third_party/`** — all external dependencies, as submodules or vendored copies.

### Rendering pipeline

The graphics backend is WebGPU (wgpu-native). The engine uses exactly one render-target
format, `RGBA8Unorm`. When the swapchain is an `*Srgb` format, the present pass runs a
fragment shader that performs an **exact piecewise sRGB inverse encoding**, which cancels
the sRGB target's own encode and reproduces the authored colour on any swapchain format.

Shaders are authored in **WGSL** and handed to wgpu as they are -- both backends carry
their own WGSL front end, so nothing is translated at run time. The descriptor layout the
pipeline needs is read back out of the source by `core/wgsl_reflect.cc`. The bind-group
convention is:

| Set | Contents |
| --- | --- |
| 0 | Scene-level uniforms (screen size, projection, …) |
| 1 | Per-drawable uniforms (dynamic offset, from a shared pool) |
| 2 | Texture and sampler |
| 3 | Per-node parameters (e.g. a sprite's colour / tone / opacity) |

To cut per-frame CPU cost, the engine batches in two places:

- **Uniform pools** (`core/render/uniform.h`): the per-drawable data of a frame is packed into a
  few large buffers and addressed with a `dynamic offset`, instead of one buffer per
  drawable.
- **Frame vertex batch** (`core/primitive.{h,cc}`): the whole frame shares a single vertex
  buffer; each drawable addresses its own range with `firstVertex`, collapsing hundreds of
  `WriteBuffer` calls per frame into one.

The per-frame sequence is:

```
SDL_AppIterate
  └─ Graphics::Update
       ├─ (prepare stage: allocate uniform slots, append vertices, flush the upload)
       ├─ Node::Render (a render pass over the screen texture)
       ├─ FPS control (FrameSkip / Delay)
       └─ PresentInternal (draw the screen texture to the swapchain)
```

### Node tree

Every visible object derives from a single `Node` base class. Parent/child relationships
are held in an intrusive doubly-linked list (`DrawableSet`), and each node carries a 3D
transform (position, quaternion rotation, scale). `Node::Render` drives **three stages**
in a fixed order:

| Stage | Responsibility | Gating |
| --- | --- | --- |
| `Prepare` | Append geometry to the frame batch, write uniforms | Returning `true` is required for `DoDraw` |
| `DoDraw` | Bind pipeline/resources and issue draws | Returning `true` is required for `PostDraw` |
| `PostDraw` | Post-draw work (post-processing, popping the scissor stack) | — |

> Note: **geometry must be emitted entirely in `Prepare`** — the frame batch is uploaded
> between the two stages, so vertices appended in `DoDraw` are never submitted.

How the node types map onto RGSS concepts:

```mermaid
graph TD
  N["Node (transform · hierarchy · three-stage render)"] --> S["Sprite"]
  N --> P["Plane (tiled)"]
  N --> V["Viewport (clipping / offscreen effects)"]
  N --> G["Geometry (triangle mesh)"]
  N --> W["WindowVX / WindowXP"]
  N --> T["TilemapVX / TilemapXP"]
  N --> R["ScreenRootNode (brightness overlay)"]
```

`Tilemap` and `Window` are bound to `TilemapVX` / `TilemapXP` and `WindowVX` / `WindowXP`
respectively, according to the RGSS version detected from `Game.ini`, so scripts see the
constant that matches their target version.

### Binding generation

Classes in `core/` marked with `/*-export.begin-*/` … `/*-export.end-*/` are exported to
Ruby. The pipeline reads only `core/**/*.h`; it neither modifies the engine sources nor
depends on CRuby VM internals:

```mermaid
flowchart LR
  H["core/**/*.h<br/>export blocks"] --> G["gen_api_json.py"]
  G --> IR["bind/api_reference.json"]
  IR --> B["generate_binding.py"]
  B --> C["bind/binding_*.{h,cc}"]
  IR --> V["bind/viewer<br/>read-only API viewer"]
```

This pipeline runs automatically during the **CMake configure step**
(`bind/gen_binding.cmake`), and both generators follow a **"compare first, write second"**
rule: when the content is unchanged they never touch the disk. The generated files keep
their mtime, so no spurious recompilation is triggered. Adding an exported class therefore
takes a single registration in `gen_api_json.py`'s `CLASS_ORDER`, followed by
`cmake -S . -B build`.

### Current Ruby API surface

| Category | Count | Names |
| --- | --- | --- |
| Classes | 23 | `Disposable`, `Node`, `Gamepad`, `Geometry`, `Bitmap`, `Color`, `Font`, `Image`, `Animation`, `Plane`, `Rect`, `Sprite`, `Table`, `Tone`, `Vector2`, `Vector3`, `Vector4`, `TilemapVX`, `TilemapXP`, `Viewport`, `WindowVX`, `WindowXP`, `Effect` |
| Modules | 4 | `Graphics`, `Input`, `Audio`, `Mouse` |

### Core module map

`core/` is grouped by function, and the include path mirrors the directory
(`core/<group>/<file>`):

| Directory | Contents |
| --- | --- |
| `common/` | Infrastructure and data classes: `definition.h` (`URGE_BINDING` / `ATTR` / `MARSHAL_*` / `KLASS_EQUAL` macros), `object.h`, `refptr.h`, `disposable.h`, `utility.{h,cc}` (`Rect` / `Color` / `Tone` / `Vector2/3/4`), `table.{h,cc}` (`Table`), `exception.h`, `logger.h`, `inirw.h` |
| `system/` | Host facilities: `config.{h,cc}` (`Game.ini`), `filesystem.{h,cc}` (PhysicsFS and RTP paths), `archive.{h,cc}` (`.acs` container) |
| `graphics/` | **Graphics classes exported to Ruby**: `graphics.{h,cc}` (the `Graphics` module: main loop, window and swapchain, present, transitions, snapshots, frame-rate control), `gpu.{h,cc}` (the `GPU` module and every `GPU*` descriptor/object), `effect.{h,cc}` (`Effect`, custom shaders) |
| `render/` | **Internal rendering (not exported)**: `device.{h,cc}` (wgpu device and queue), `pipeline.{h,cc}`, `shader.{h,cc}`, `gpu_utils.h` (bind group/layout helpers), `primitive.{h,cc}` (frame vertex batch), `uniform.{h,cc}` (per-frame uniform pools), `render_stats.h`, `imgui_manager.{h,cc}` (the F1/F2 overlay) |
| `scene/` | **Scene nodes**: `node` (three-stage render dispatch), `drawable` (drawable list), `sprite` / `sprite_batch`, `plane`, `viewport`, `window_vx` / `window_xp`, `tilemap_vx` / `tilemap_xp`, `geometry`, `camera` |
| `canvas/` | **The canvas and what hangs off it**: `bitmap.{h,cc}` (bitmaps, GPU textures, `Blt`/gradients/text), `bitmap_queue.{h,cc}` (draw command queue), `image.{h,cc}` (palette), `animation.{h,cc}` (frame list) |
| `font/` | `font{,_context,_renderer}.{h,cc}`: face resolution (`Fonts/` → system → embedded), SDL_ttf rendering and text metrics |
| `audio/` | `audio.{h,cc}` (BGM / BGS / SE), `audio_service.{h,cc}`, `audio_stream.{h,cc}` |
| `input/` | `input.{h,cc}` (keyboard and pad into one slot table), `gamepad.{h,cc}` (SDL3 pad, hot-plug, triggers/sticks/rumble, multiple pads, and the script-side `Gamepad` class), `mouse.{h,cc}`, `key_sym.h` (binding entry and the pressed/triggered/repeated machine) |
| `network/` | `network.{h,cc}` (front end), `network_backend.h`, `network_curl.cc` / `network_web.cc` (one backend per platform) |
| `video/` | `video{,_decoder,_demuxer,_player}.{h,cc}`: WebM/AV1 playback |

---

<div align="center">
  <sub>Licensed under the <a href="LICENSE.txt">MIT License</a> · Copyright (c) 2026 Admenri Adev</sub>
</div>

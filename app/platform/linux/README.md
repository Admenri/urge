# Linux amd64 native port

## Run a release

The release asset `urge-linux-amd64.tar.gz` contains a stripped ELF executable,
this guide and license notices. Extract it with `tar -xzf` (not by copying a
loose GitHub artifact, which loses executable permissions). Put `Game` beside
`Game.ini` and the game's `Data/`, `Graphics/`, `Audio/` and other resources, then
run `./Game`. The executable's actual directory is also the working/save
folder, including when launched through `PATH` from another directory. It must
be writable for saves and logs. No external Ruby installation is needed.

Official amd64 CI builds use the AlmaLinux 8 based `manylinux_2_28` container
and GCC 14. CI checks that the executable requires no glibc symbols newer than
2.28; this environment applies only to CI, not to local source builds. These
are not musl/Alpine binaries. SDL, Ruby, wgpu, FreeType, the media codecs,
OpenSSL (including its providers), zlib, JPEG, TIFF and the GCC C++/unwind
runtimes are linked statically. glibc and the system's `libcrypt.so.1` remain
dynamic dependencies; graphics/audio drivers are loaded from the target system.
Use `ldd ./Game` to check the actual runtime dependencies.

On AlmaLinux 8, typical runtime packages are:

```sh
sudo dnf install ca-certificates libxcrypt \
  libX11 libXcursor libXext libXi libXrandr libXScrnSaver libXtst \
  wayland-libs libxkbcommon alsa-lib pulseaudio-libs mesa-libEGL mesa-libGL \
  vulkan-loader mesa-vulkan-drivers
```

Other distributions use different package names and may need compatibility
packages for the listed library ABIs. Their local source builds continue to
use the dependencies and glibc supplied by that distribution.

Install the appropriate vendor GPU driver as well. Both X11 and Wayland windows
are supported. wgpu-native prefers Vulkan; `[GFX] Backend=opengl` or
`Backend=opengles` selects the GL backend when Vulkan is unavailable. Headless
SDL drivers such as `dummy` cannot create a presentation surface. ALSA and
PulseAudio provide native sound. HTTPS/WSS verify peers using OpenSSL's system
trust store (EL8's default `/etc/pki/tls`). On distributions with a different CA
layout, use e.g. `SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt ./Game`.
No external OpenSSL provider modules or system OpenSSL configuration are needed.

CI compiles its static image libraries from the pinned repository sources and
OpenSSL from the same checked source pin as macOS. TIFF supports its built-in
codecs, JPEG/Old JPEG, Deflate and PixarLog; optional external TIFF codecs
(JBIG, LERC, LZMA, ZSTD, WebP) are not included in this release build.
`libcrypt` stays system-linked to retain Ruby's full password-hash support
without adding a static LGPL library to the executable.

Fonts are resolved from the game's `Fonts/`, the system/XDG font directories,
and finally the embedded fallback. Bundle the game's required fonts for
consistent text rendering. Windows RTP registry discovery and native DLL
loading are not available on Linux; ship assets or configure load paths. The
Win32API shim currently supports desktop queries; unsupported exports return
its existing no-op/error behavior. MIDI requires a SoundFont configured through
`[Audio] MIDISoundFont`. Asset directory names must match case on Linux.

## Build from source

Initialize recursive Git submodules first. Required tools: CMake >= 3.30,
GCC >= 13 (including C++20 `std::format`), stable Rust >= 1.87, Python 3 with
venv support, Ninja, NASM and libclang. No system Ruby or FreeType is needed:
CRuby, FreeType, SDL and the audio/video codecs are built from the tree.

Ubuntu build dependencies (use GCC 14 explicitly on Ubuntu 24.04):

```sh
sudo apt-get update
sudo apt-get install --no-install-recommends build-essential gcc-14 g++-14 \
  ninja-build nasm pkg-config libclang-dev python3-venv libssl-dev zlib1g-dev \
  libasound2-dev libpulse-dev libx11-dev libxcursor-dev libxext-dev libxi-dev \
  libxrandr-dev libxss-dev libxtst-dev libwayland-dev libxkbcommon-dev \
  libdecor-0-dev libdrm-dev libgbm-dev libegl1-mesa-dev libpng-dev libtiff-dev \
  libavif-dev
# Install CMake >= 3.30 and a stable Rust toolchain separately if necessary.
python3 -m venv build/linux-tools
. build/linux-tools/bin/activate
python -m pip install -r requirements.txt
CC=gcc-14 CXX=g++-14 cmake --preset linux-amd64
cmake --build --preset linux-amd64 --parallel 4
```

The local output is `build/linux-amd64/app/Game`, including debug information.
CI strips only its distribution copy and packages it with the license notices
as `urge-linux-amd64.tar.gz`, preserving executable permissions. No additional
build or packaging shell scripts are required.

Use `CMAKE_BUILD_PARALLEL_LEVEL` or `--parallel` to limit compiler processes.
To relocate the build, configure with `cmake --preset linux-amd64 -B <dir>` and
then build with `cmake --build <dir> --target Game`.

## WSL2

Run the Linux tools **inside** an existing WSL2 distribution, not from the
Windows CMake/Rust toolchain. From PowerShell, for example:

```powershell
wsl -d Ubuntu -- bash -lc 'cd /mnt/j/Desktop/urge && . build/linux-tools/bin/activate && cmake --preset linux-amd64 && cmake --build --preset linux-amd64 --parallel 4'
```

For faster builds, place the source/build on WSL's ext4 filesystem rather than
`/mnt/<drive>`. WSLg supplies the X11/Wayland display; keep Windows GPU drivers
and WSL current. If the Vulkan driver does not work, try the GL backend. The
Linux executable still needs Linux runtime libraries, not Windows DLLs.

## CI and local regression checks

The `linux-amd64` job in `.github/workflows/build.yml` uses an Ubuntu runner
with a `manylinux_2_28_x86_64` container. It verifies glibc 2.28, prepares its
CI-only static dependencies, packages the binary, checks the ELF symbol
versions and shared-library allowlist, extracts it, and tests the packaged executable using
Xvfb and Mesa's software drivers (`RH_SW_VULKAN=1` enables EL8's lavapipe).
It reuses the existing GPU regression suite
for both Vulkan and OpenGL, plus loopback Fetch/WebSocket tests for RGSS1,
RGSS2 and RGSS3. The reusable build workflow is called by
`.github/workflows/release.yml` for `v*` tags; the tarball is attached alongside
the existing platform assets.

For local headless checks, install `xvfb xauth mesa-vulkan-drivers`, then:

```sh
xvfb-run -a python3 tools/tests/gpu/run.py --engine build/linux-amd64/app/Game --backend vulkan
xvfb-run -a python3 tools/tests/gpu/run.py --engine build/linux-amd64/app/Game --backend opengl
xvfb-run -a python3 tools/tests/network/run.py --engine build/linux-amd64/app/Game --backend vulkan
```

To run a real game in an existing Wayland desktop/WSLg session, launch its
`Game` executable with `SDL_VIDEO_DRIVER=wayland`. Software rendering in CI
does not replace testing a real GPU or audio device.

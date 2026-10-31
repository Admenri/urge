<img src="app/resources/urge_favicon_512.png" width="96" align="left" alt="URGE logo" />

# Universal Ruby Game Engine (URGE)

**兼容 RGSS1 / RGSS2 / RGSS3 的 2D / 3D 混合游戏引擎**

简体中文 | [English](README_EN.md)

<br clear="left" />

---

## 1. 项目概览

**URGE（Universal Ruby Game Engine）** 是一个从零实现的 RGSS 运行时，兼容 RPG Maker XP（RGSS1）、VX（RGSS2）与 VX Ace（RGSS3）。引擎以 C++20 实现窗口、渲染、输入、音频、字体与文件系统，并通过自动生成的绑定层暴露给内嵌的 CRuby 解释器；面向这三代 RPG Maker 的既有工程原则上无需修改脚本即可运行。

渲染后端统一为 WebGPU。2D 画面与 3D 世界共享同一套设备、资源与帧循环，并可通过 `World3D` 按 Z 序把 3D 渲染结果合成进 2D 场景，因此可以在传统 RGSS 工程中直接叠加 3D 场景、模型与骨骼动画。

### 主要特性

| 领域 | 说明 |
| --- | --- |
| **RGSS 兼容** | 支持 RGSS1 / RGSS2 / RGSS3；按 `Game.ini` 的 `RGSS` 字段或脚本归档扩展名（`.rxdata` / `.rvdata` / `.rvdata2`）自动判定版本；`Window` / `Tilemap` 按版本映射到 XP 或 VX 实现；内置各版本所需的 `RPG` 兼容模块 |
| **脚本运行时** | 内嵌 CRuby 4.0.0，直接执行 Marshal + zlib 脚本归档，无需安装外部 Ruby；提供 `rgss_main` / `rgss_stop` / `RGSSError` / `RGSSReset` 等入口 |
| **渲染** | WebGPU（原生端 wgpu-native，Web 端浏览器 WebGPU）；WGSL 着色器与自定义 `Effect`；帧顶点批与 uniform 池；逻辑分辨率与物理像素解耦，适配高分屏 |
| **2D** | `Bitmap`（Blt、渐变、文字、像素读写）、`Sprite`、`Plane`、`Viewport`、`WindowXP` / `WindowVX`、`TilemapXP` / `TilemapVX`、`Geometry`、`Animation`、精灵批处理、2D 相机 |
| **3D** | `Node3D` 场景树、透视 / 正交相机、`Light3D`（含阴影）、glTF 模型加载、骨骼蒙皮、动画栈与状态机、PBR 风格材质、`RenderGraph3D`、渲染目标与计算通道、`World3D` 与 2D 合成 |
| **GPU 直通** | `GPU` 模块把 WebGPU 的缓冲区、纹理、采样器、着色器、绑定组、渲染 / 计算管线与命令编码直接暴露给 Ruby |
| **音频** | BGM / BGS / ME / SE；WAV、Ogg Vorbis、Ogg Opus；MIDI + SoundFont；独立音量与输出设备选择；`AudioStream` 支持流式播放与控制 |
| **输入** | 键盘、鼠标、手柄（多设备、热插拔、震动、扳机 / 摇杆）、多点触摸（最多 16 指）；键位绑定保存在 `Game.ini` |
| **字体** | `Fonts/` → 系统字库 → 内嵌字体逐级回退；支持文件名与族名、CJK 文本；Windows 默认使用微软雅黑 |
| **资源** | PhysicsFS 虚拟文件系统；Windows 下自动探测已安装的 RTP；可选 `.acs` 加密容器（需放入 `admenri/` 目录） |
| **视频** | WebM / AV1 播放（桌面端，可关闭）；`Graphics.play_movie` 提供带进度控制的影片事件 |
| **网络** | HTTP `Fetch` 与 `WebSocket`（桌面与 Web，可关闭） |
| **调试** | F1 设置面板、F2 帧率面板；运行日志；Windows 崩溃转储与符号化报告；可选 PGO |

### 平台支持

| 平台 | 产物 | 图形后端 | 说明 |
| --- | --- | --- | --- |
| Windows 10 1809+（x64） | `Game.exe` | D3D12（默认，可切换 Vulkan / OpenGL 等） | 功能最完整；视频与网络默认开启 |
| WebAssembly（支持 WebGPU 的浏览器） | `Game.html` + `Game.wasm` | 浏览器 WebGPU | 视频默认关闭；网络可用 |
| Android 9+（API 28，arm64-v8a / x86_64） | APK | Vulkan 1.1 | 视频与网络默认开启；TLS 使用内置 mbedTLS |
| Linux / macOS | — | — | 尚未提供构建预设 |

### Ruby 脚本 API

绑定由 `core/**/*.h` 中的导出标记自动生成，目前导出 84 个类与 9 个模块。

| 类别 | 类 / 模块 |
| --- | --- |
| 运行时模块 | `Graphics`、`Input`、`Mouse`、`Touch`、`Audio`、`Network`、`Config`、`URGE`、`GPU` |
| 2D | `Node`、`Sprite`、`Plane`、`Viewport`、`WindowXP`、`WindowVX`、`TilemapXP`、`TilemapVX`、`Geometry`、`Effect`、`Camera`、`OrthographicCamera`、`PerspectiveCamera` |
| 3D | `Node3D`、`Scene3D`、`World3D`、`Camera3D`、`Light3D`、`GLTFModel3D`、`Mesh3D`、`MeshRenderer3D`、`Skeleton3D`、`BoneMask3D`、`Animation3D`、`AnimationLayer3D`、`AnimationStack3D`、`AnimationStateMachine3D`、`Material3D`、`MaterialPass3D`、`RenderTarget3D`、`ComputePass3D`、`RenderQueue3D`、`RenderContext3D`、`RenderGraph3D`、`RenderGraphPass3D`、`ForwardPipeline3D` |
| GPU | `GPUObject`、`GPUBuffer`、`GPUTexture`、`GPUTextureView`、`GPUSampler`、`GPUShaderModule`、`GPUBindGroup`、`GPURenderPipeline`、`GPUComputePipeline`、`GPUCommandEncoder`、`GPURenderPassEncoder`、`GPUComputePassEncoder` 及对应的 `GPU*Descriptor` |
| 数据与画布 | `Bitmap`、`Image`、`Font`、`Color`、`Rect`、`Tone`、`Table`、`Vector2` / `Vector3` / `Vector4`、`Animation` |
| 其他 | `Disposable`、`Gamepad`、`AudioStream`、`Video`、`WebSocket`、`Fetch`、`NetworkEvent` |

### 目录结构

```
urge/
├── app/              宿主程序（Windows / Web / Android）与入口
├── core/             引擎核心：2D、3D、渲染、音频、输入、字体、网络、视频
├── bind/             CRuby 绑定、自动生成器与 Win32API 桥接
├── android/          Android Gradle 工程
├── third_party/      第三方依赖（Git 子模块与内嵌源码）
└── CMakeLists.txt
```

---

## 2. 构建方法

### 2.1 环境要求

所有平台都需要的工具：

| 工具 | 要求 |
| --- | --- |
| CMake | ≥ 3.30；Windows 预设使用 `Visual Studio 18 2026` 生成器，需要支持该生成器的 CMake（较新的 4.x） |
| Python | 3.x，且 `python` / `python3` / `py` 位于 `PATH` 中（配置期生成 Ruby 绑定；Web 端还用于资源索引） |
| Git | 克隆时需拉取子模块 |
| C++ 编译器 | 支持 C++20 |
| Rust | stable 工具链。桌面端与 Android 构建 wgpu-native 必需；Web 端使用浏览器 WebGPU，不需要 Rust |
| 网络 | 首次配置需要下载 Corrosion 并编译 Rust 依赖 |

各平台的额外要求：

| 平台 | 需要安装 |
| --- | --- |
| **Windows** | Visual Studio 2026（生成器 `Visual Studio 18 2026`），安装“使用 C++ 的桌面开发”工作负载（MSVC x64 + Windows SDK）；Rust 的 `x86_64-pc-windows-msvc` 目标 |
| **Web** | Emscripten SDK（先运行 `emsdk_env`，或设置 `EMSDK` / `EMSCRIPTEN`）；Ninja |
| **Android** | Android SDK（Platform 37、Build-Tools 36.0.0）与 NDK；JDK 17 或更新；Rust 目标 `aarch64-linux-android`、`x86_64-linux-android`；Ninja |

> Android 设备需要 Vulkan 1.1；应用最低为 Android 9（API 28）。

### 2.2 获取源码

```bash
git clone --recursive https://github.com/Admenri/urge.git
cd urge
```

已有克隆但依赖不完整时：

```bash
git submodule update --init --recursive
```

### 2.3 Windows（x64）

```bash
cmake --preset win64-vendored-ruby
cmake --build --preset win64-vendored-ruby
```

产物：

- Release 配置（RelWithDebInfo）：`build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe`
- 调试构建：`cmake --build --preset win64-vendored-ruby-debug`，产物位于 `build/win64-vendored-ruby/app/Debug/Game.exe`

不使用预设时，也可以自行指定生成器：

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config RelWithDebInfo --target Game
```

使用其他 Visual Studio 版本时，把 `-G` / `-A` 换成与你的环境对应的生成器即可。

### 2.4 Web（WebAssembly）

```bash
# Linux / macOS
source /path/to/emsdk/emsdk_env.sh
# Windows
# emsdk_env.bat

cmake --preset emscripten-wasm32 -DURGE_WEB_ASSETS="/path/to/game"
cmake --build --preset emscripten-wasm32
```

`URGE_WEB_ASSETS` 指向游戏目录（含 `Game.ini`、`Data/` 等）。构建产物在 `build/emscripten-wasm32/app/`：

```
Game.html  Game.js  Game.wasm  async_mapping.js  urge-helper.js  assets/
```

游戏文件以 `assets/` 形式随模块发布：`*.ini`、`*.acs` 与 `Data/Scripts.*` 在首帧前预加载（可用 `URGE_WEB_PRELOAD` 调整），其余文件在首次访问时按需下载。若希望把游戏打包成单个 `Game.data`，配置时加上 `-DURGE_WEB_EMBED_ASSETS=ON`。

通过 HTTP 访问（`localhost` 或 HTTPS；浏览器普遍只在安全上下文中提供 WebGPU）：

```bash
python -m http.server 8000 -d build/emscripten-wasm32/app
```

然后打开 `http://localhost:8000/Game.html`。

### 2.5 Android（arm64-v8a / x86_64）

先添加 Rust 交叉编译目标：

```bash
rustup target add aarch64-linux-android x86_64-linux-android
```

编译原生库（两个 ABI 可按需选择）：

```bash
cmake --preset android-arm64
cmake --build --preset android-arm64

# 模拟器或 x86_64 设备
cmake --preset android-x86_64
cmake --build --preset android-x86_64
```

构建会把 `libmain.so` 与 `libSDL3.so` 复制到 `android/app/src/main/jniLibs/<abi>/`，Gradle 只负责打包。

将游戏目录挂载进 APK 并构建调试包：

```bash
cd android
# Windows 使用 gradlew.bat
./gradlew assembleDebug -P urgeGameDir="/path/to/game"
```

产物位于 `android/app/build/outputs/apk/debug/app-debug.apk`。游戏文件会进入 APK 的 `assets/`，并在首次启动时解包到应用的私有目录。

发布构建需要签名参数（`urgeStoreFile`、`urgeStorePassword`、`urgeKeyAlias`、`urgeKeyPassword`，可写在 `local.properties` 或通过 `-P` / 环境变量提供）；缺失时 APK 保持未签名。

### 2.6 运行游戏

**Windows**：把 `Game.exe` 放进游戏目录即可。引擎以可执行文件所在目录为工作目录，并读取与可执行文件同名的配置：

```
MyGame/
├── Game.exe
├── Game.ini
├── Game.acs          # 可选：加密资源容器，优先于散装文件
├── Data/
│   └── Scripts.rvdata2
├── Graphics/
├── Audio/
└── Fonts/
```

- 配置文件为 `<可执行文件名>.ini`（`Game.exe` → `Game.ini`）。
- 若存在 `<可执行文件名>.acs` 且构建包含 `admenri/`，该容器会被优先挂载。
- 运行日志写入 `<可执行文件名>.log`。

**Web / Android**：可执行名固定为 `Game`，因此配置与资源分别位于站点根目录或 APK 的 `assets/` 下的 `Game.ini`、`Data/` 等路径。

`Game.ini` 示例：

```ini
[Game]
RGSS = 3
Scripts = Data/Scripts.rvdata2
Title = My Game
; RTP / RTP1 / RTP2 / RTP3 可选；Windows 下会自动探测已安装的运行库

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

配置项说明：

| 段 | 键 | 默认值 | 说明 |
| --- | --- | --- | --- |
| `Game` | `RGSS` | `0`（自动） | `1` = XP、`2` = VX、`3` = VX Ace；为 `0` 时按 `Scripts` 扩展名推断 |
| `Game` | `Scripts` | `Data/Scripts.rxdata` | 脚本归档路径 |
| `Game` | `Title` | `(*^▽^*)` | 窗口标题 |
| `Game` | `RTP` / `RTP1`–`RTP3` | 空 | RPG Maker 运行库名称；Windows 下自动探测 |
| `Display` | `Width` / `Height` | XP：`640×480`；其余：`544×416` | 逻辑分辨率 |
| `Display` | `VSync` / `Fullscreen` / `FrameSkip` | `false` | 垂直同步、全屏与跳帧 |
| `Window` | `Resizable` | `none` | `none` / `free` / `ratio` |
| `Audio` | `Soundfont` | `Fonts/Default.sf2` | MIDI 音色库 |
| `Audio` | `MasterVolume` / `BGMVolume` / `BGSVolume` / `MEVolume` / `SEVolume` | `100` | 音量 |
| `Audio` | `OutputDevice` | 空（系统默认） | 音频输出设备 |
| `GFX` | `Backend` | 空（自动） | `d3d11` / `d3d12` / `vulkan` / `opengl` / `opengles` / `metal` |
| `GFX` | `SpriteBatch` | `true` | 精灵批处理 |
| `Input` | `PadBindings` | `false` | 是否已写入手柄默认绑定 |
| `KeyBinding` | — | — | 每个 slot 一行；手柄项写成 `pad:<token>` |

补充说明：

- **F1 / F2**：F1 打开设置面板（可在此添加键位绑定），F2 切换帧率显示。
- **字体**：按 `Fonts/` → 系统字库 → 内嵌字体的顺序解析；文件名（可省略扩展名）与族名均可使用。
- **手柄**：键盘与手柄映射到同一套 slot；手柄热插拔即用，支持多设备、震动与摇杆 / 扳机。
- **性能分析**：Windows 下可通过环境变量 `URGE_PROFILER` / `URGE_PROFILER_INTERVAL` / `URGE_PROFILER_DIR`，或 `[Debug]` 段的 `Profiler` / `ProfilerInterval` 开启采样分析。

### 2.7 常用构建选项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `URGE_RUNNER_MODEL` | 桌面 / Android：`poll`；Web：`fiber` | 帧循环归属：引擎主循环（`poll`）或 SDL 回调循环（`fiber`） |
| `URGE_ENABLE_VIDEO` | 桌面 / Android `ON`；Web `OFF` | WebM / AV1 视频播放 |
| `URGE_ENABLE_NETWORK` | 全平台 `ON` | HTTP `Fetch` 与 `WebSocket`（Web 端使用浏览器自带实现） |
| `URGE_WIN7_COMPAT` | `OFF` | 链接 Windows 7 兼容钩子：把 `WaitOnAddress`、`ProcessPrng`、`CreateFile2` 等 Win8+ 导入转到内置回退实现 |
| `URGE_VIDEO_AV1_8BIT_ONLY` | `OFF` | 仅编译 8-bit AV1，减小体积（会失去 10 / 12-bit 视频） |
| `URGE_WEB_ASSETS` | 空 | Web 端游戏目录 |
| `URGE_WEB_PRELOAD` | `*.ini,*.acs,Data/Scripts.*` | Web 端首帧前预加载的文件 |
| `URGE_WEB_EMBED_ASSETS` | `OFF` | 将游戏打包为单个 `Game.data` |
| `URGE_BUILD_ACS_PACK` | `ON` | 存在 `admenri/` 时构建 `.acs` 容器打包工具 |
| `URGE_PGO` | `off` | `generate` / `use` 两阶段 PGO（MSVC） |
| `URGE_EMSDK` / `URGE_ANDROID_NDK` | 自动探测 | 显式指定 Emscripten SDK / Android NDK 路径 |

> `URGE_WIN7_COMPAT` 只消除导入表里的 Win8+ API 依赖；Windows 7 没有 D3D12，图形后端仍需 Vulkan / OpenGL 驱动支持。

> Android NDK 不能链接 OpenSSL，因此 Android 端的网络构建使用随源码内置的 mbedTLS 作为 libcurl 的 TLS 后端，并读取系统证书目录 `/system/etc/security/cacerts`；Windows 使用 Schannel，其余桌面平台使用系统 OpenSSL。视频与网络子系统关闭时，Ruby 侧对应的模块仍然存在，调用会抛出异常，因此脚本 API 在各平台上保持一致。

### 2.8 第三方依赖

以下依赖以 Git 子模块提供，需随源码一并拉取（`git submodule update --init --recursive`，`mbedtls` 自带 `framework` 子模块）：

| 子模块 | 用途 |
| --- | --- |
| `glm` | 向量、矩阵与投影 |
| `SDL` | 窗口、输入、音频与平台抽象 |
| `SDL_image` | 位图解码（含 libwebp） |
| `SDL_ttf` | TrueType 字体栅格化（内含 FreeType） |
| `physfs` | 虚拟文件系统 |
| `wgpu-native` | 原生 WebGPU 实现 |
| `zlib` | 脚本归档解压 |
| `curl` | HTTP 与 WebSocket |
| `mbedtls` | Android 端 libcurl 的 TLS 后端 |
| `ogg` / `vorbis` / `opus` / `opusfile` | Ogg Vorbis 与 Ogg Opus 解码 |

此外，`third_party/` 中还内嵌了 `ruby`（CRuby 4.0.0）、`miniaudio`、`imgui`、`dav1d`、`webm`、`libffi` 与 `webgpu-cpp`。Web 端使用 Emscripten 的 `emdawnwebgpu` 端口，而不是 wgpu-native。

可选的 `admenri/` 目录不在开源仓库中：放入该目录后，构建会自动启用 `.acs` 加密容器与 `urge-acs-pack` 打包工具；没有它时引擎以普通文件方式运行。

---

## 3. 开源协议

本项目以 [MIT License](LICENSE.txt) 开源。

```
Copyright (c) 2026 Admenri Adev
```

第三方依赖遵循各自的许可证；RPG Maker 运行库（RTP）与游戏素材不包含在本仓库中，其版权归各自作者所有。

---

<div align="center">
  <sub>Licensed under the <a href="LICENSE.txt">MIT License</a> · Copyright (c) 2026 Admenri Adev</sub>
</div>

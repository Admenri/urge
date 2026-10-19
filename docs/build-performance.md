# URGE 构建体积 / 构建时间分析

> 基于当前树（`build/win64-vendored-ruby` 的配置结果）实测数据，生成器为 **Visual Studio 18 2026**，
> 配置 **RelWithDebInfo**，目标 `Game`。
> 本文只分析原因与方案，不改动构建。

---

## 0. 结论速览

| 问题 | 头号原因 | 预估收益 |
|---|---|---|
| 编译慢 | **构建是单线程的**：CMake 的 VS 生成器默认 `msbuild` 不带 `/m`，且 1334 个工程里只有 libcurl 两个开了 `/MP`。1211 个 C/C++ TU 排队串行编译，外加一个串行的 Cargo(wgpu-native) 步骤 | 8–16 核下 **总时长降到 1/5 ~ 1/8** |
| 二进制 40MB | **RelWithDebInfo 的 `/debug /INCREMENTAL` 静默关掉了 `/OPT:REF,ICF`**，而且全树没有开 `/Gy`（函数级链接）。于是链接器把所有被拉进来的 .obj 完整保留 | 单这一项 **-20% ~ -40%** |
| 二进制 40MB | wgpu-native（Rust）用默认 feature 全集 + cargo 默认 release profile（无 LTO、无 strip、16 codegen units） | 可能是**最大单项**，-10% ~ -30% |
| 编译慢 | 第三方库源码总量 **~230 万行**，其中 wgpu-native（Rust）与 SDL3 / curl / opus / cruby / glslang 是大头 | 见分级方案 |

---

## 1. 实测数据

### 1.1 编译单元规模（`--target Game` 的依赖闭包）

`--target Game` 实际会构建 **44 个工程 / 1211 个 C/C++ 编译单元**（不含 Rust）：

| TU 数 | 工程 |
|---:|---|
| 277 | SDL3-static |
| 196 | libcurl_object |
| 152 | opus |
| 85 | ruby-static（自带 CRuby 1.9.3） |
| 71 | urge-binding（**代码生成产物**） |
| 46 | urge-core |
| 42 | glslang |
| 42 | freetype |
| 34 | physfs-static |
| 31 | webpdsp |
| 27 + 13 + 13 | dav1d（含 8bit / 16bit 模板展开） |
| 26 | SDL3_image-static |
| 23 | webpencode |
| 21 | vorbis |
| 15 | zlibstatic |
| 12+10+7+4+2 | webputils / webpdecode / sharpyuv / libwebpmux / webpdemux |
| 11+1+1 | plutovg / harfbuzz(单文件 unity) / plutosvg |
| 8 | miniaudio |
| 7 | libwebm / libffi / SDL3_ttf-static |
| 6 | imgui-docking |
| 4 | opusfile |
| 2 | ogg / Game / glslang-default-resource-limits |
| 1 | vorbisfile / spirv-reflect / SPIRV |

另有：

* **1 个 Rust Cargo 构建**（`wgpu-native`），通过 Corrosion 以 `cargo rustc --release` 形式跑，是一个不可分割的串行步骤。
* 顺带一提：`ALL_BUILD` 一共挂了 **67 个工程**，含 `cwebp / dwebp / img2webp / webpinfo / webpmux / test_physfs / minigzip / zlib_example / physfs-shared / zlib(shared) / vorbisenc / SDL3_test` 等无用目标。
  当前工作流用 `--target Game`，这些不会编译——但一旦有人 `cmake --build build` 不带 `--target`，就全建了。

### 1.2 第三方源码体量（行数）

| 库 | 文件 | 行数 |
|---|---:|---:|
| cruby 1.9.3 | 170 | 489k |
| SDL | 794 | 468k |
| SDL_ttf（含 freetype/harfbuzz/plutovg/plutosvg） | 451 | 268k |
| curl | 768 | 263k |
| glslang（含 SPIRV） | 79 | 120k |
| miniaudio（单头文件 89k） | 9 | 89k |
| libwebp | 161 | 80k |
| opus | 255 | 69k |
| libffi | 274 | 57k |
| dav1d | 82 | 43k |
| wgpu-cmake（Rust，另有 500+ crate 依赖） | 78 | 41k |
| glm | 211 | 33k |
| zlib / imgui / vorbis / physfs / opusfile / ogg / spirv-reflect / webm | — | ~150k |
| **本仓库自身**（core 25k + bind 21k + admenri 4.6k + app 0.3k） | — | **~51k** |

> **本仓库自己只占编译量的 2%。** 慢的 98% 都在第三方与解释器上。

---

## 2. 编译慢的原因（按影响排序）

### C1. 构建完全没有并行度 ⭐⭐⭐⭐⭐

* `cmake --build` 对 VS 生成器默认调用 `msbuild` **不带 `/maxcpucount`**，CMake 4 也不会自动加 `/m`
  （除非 `--parallel N` 或环境变量 `CMAKE_BUILD_PARALLEL_LEVEL`）。
  现有 preset 里没有 `"jobs"`/parallel 设置（`CMakePresets.json` 的 build preset 也没有该字段）。
* 工程内部同样串行：`urge-core.vcxproj` 的 RelWithDebInfo 配置里 **没有 `MultiProcessorCompilation`**，
  即 **没有 `/MP`**。全树 1334 个 vcxproj 中**只有 `libcurl_object` / `libcurl_static` 两个**设了 `/MP`。
* 结果：277 个 SDL 源文件、196 个 curl 源文件、152 个 opus 源文件……各自排一条队，一个核跑到底。

### C2. wgpu-native 的 Cargo 构建是一个不可切的串行块 ⭐⭐⭐⭐

* `third_party/wgpu-cmake/CMakeLists.txt` 用 Corrosion `corrosion_import_crate`，
  MSBuild 里体现为 `_cargo-build_wgpu_native` 一个 NMake 工程。
* 命令是 `cargo rustc --lib --crate-type=cdylib --crate-type=staticlib --release`。
  wgpu 29 + naga + wgpu-hal(dx12/vulkan/metal/gles) + ash + windows-rs，crate 数量 500+。
* Cargo 自身会吃满所有核，但它**前面挡着整个 C/C++ 依赖图**：`urg-core` 依赖它，
  所以在它完成前，MSBuild 无法推进到链接；而它自己不参与 MSBuild 的调度。

### C3. 第三方库全量编译，没有裁剪 feature ⭐⭐⭐⭐

* `wgpu-native/Cargo.toml` 的 `default = ["wgsl","spirv","glsl","dx12","metal","vulkan","gles"]`。
  **Windows 上 `metal` 与 `gles` 完全是死代码**，但 naga 的 GLES/Metal 后端照样编译。
* `dav1d` 编译了 8bit **和** 16bit 两套位深模板（`dav1d_depth_8` / `dav1d_depth_16`）。
* `libwebp` 编了**编码器**（`webpencode` 23 TU、`imageenc`），SDL_image 只需要解码。
* `SDL3-static` 277 TU 里含默认开启的全部子系统。
* `opus` 152 TU：含大量 ARM/NEON 与 x86 SIMD 变体、浮点点与定点两套实现。
* `miniaudio` 89k 行的单头文件被 **8 个 TU 各解析一遍** = 71 万行 C。

### C4. 没有 PCH、没有 unity build、增量粒度差 ⭐⭐⭐

* 全树 `PrecompiledHeader = NotUsing`。
* 71 个 `bind/binding_*.cc` 每个都要重新解析 `ruby.h` + 若干 `core/*.h` + `webgpu_cpp.hpp`（**252 KB / 5161 行**，50 个 TU 会拉到它）。
* `core/*.h` 一改 → 46 个 core TU + 71 个 bind TU 全量重编（117 TU），这是日常迭代最痛的点。
* `bind/CMakeLists.txt` 用了 `CONFIGURE_DEPENDS` → 每次 `cmake --build` 都会跑 `ZERO_CHECK` 重新检查 glob，
  必要时重新 configure（VS 生成器额外开销）。

### C5. 生成器本身偏慢 ⭐⭐

VS/MSBuild 相比 Ninja：调度粒度是"工程"而不是"TU"，有 ZERO_CHECK、有 `.vcxproj` 加载开销、
单配置目录下 4 套配置目录都要建。改用 Ninja 后并行度天然覆盖全部 TU。

---

## 3. 二进制 40MB 的原因（按影响排序）

### S1. RelWithDebInfo 静默关掉了链接器优化 ⭐⭐⭐⭐⭐

来自 `build/win64-vendored-ruby/CMakeCache.txt`：

```
CMAKE_CXX_FLAGS_RELWITHDEBINFO  = /O2 /Ob1 /DNDEBUG
CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO = /debug /INCREMENTAL
```

MSVC 文档（/OPT）原文：

> When **/DEBUG** is specified, the default for /OPT is **NOREF**, and all functions are preserved in the image.
> … When used at the command line, the linker defaults to **/OPT:REF,ICF,LBR**.
> **If /DEBUG is specified, the default is /OPT:NOREF,NOICF,NOLBR.**
> The **/OPT:REF** option also **disables incremental linking**.

也就是说当前链接同时吃到三个负面效果：

1. `/DEBUG` → 默认 `/OPT:NOREF`：所有**未被引用的 COMDAT 全部保留**。
2. `/DEBUG` → 默认 `/OPT:NOICF`：**相同函数不折叠**。bind 层有大量模板胶水（`RB_FUNC` / `RB_DEF_TYPE_INHERIT`），
   是 ICF 的肥肉。
3. `/INCREMENTAL` → 与 `/OPT:REF` 互斥，且会插入 ILT 跳转桩与填充。

再叠加：**全树没有 `/Gy`（FunctionLevelLinking）**。`urge-core.vcxproj` 里
`FunctionLevelLinking` 为空。没有 `/Gy` 时，一个 `.obj` 只贡献一个 `.text` 段，
只要这个 obj 里有一个符号被引用，**整个 obj 的代码全部进 exe**——
即便打开 `/OPT:REF` 也基本无从下手。

### S2. wgpu-native 用默认 feature 全集 + 默认 release profile ⭐⭐⭐⭐⭐

* `Cargo.toml` 无 `[profile.release]` 段 → 用 cargo 默认：
  `opt-level=3, lto=false, codegen-units=16, debug=false, panic=unwind, strip=none`。
  没有 LTO、没有 strip、16 个并行 codegen unit（阻碍跨单元内联与死代码消除）。
* Metal + GLES 后端在 Windows 上是死代码，但被编译进 `wgpu_native.lib`。
* `wgpu_native.lib` 被直接列在 `Game.vcxproj` 的 `AdditionalDependencies` 里。
* 这是 Rust 静态库进 MSVC 链接，符号粒度粗，配合 S1 的 `NOREF` 会把大量无关代码留在 exe 里。

### S3. glslang + SPIRV + spirv-reflect 在运行时存在 ⭐⭐⭐⭐

* `core/shader.cc` 在**运行时**调用 `glslang::InitializeProcess()` → `TShader/TProgram` → `GlslangToSpv()`，
  再用 `spirv_reflect` 推导 bind group 布局。
* 于是 12 万行的 glslang + SPIRV 全部进了 exe（估计 3–5 MB），并且**每次启动都要现编着色器**。
* 注意：wgpu-native 自己就带了 naga（且已启用 `wgsl`/`spirv`/`glsl` 三个输入前端）——
  **树里有两套着色器编译器**。

### S4. 全静态链接 20+ 个库，无法按需裁剪 ⭐⭐⭐

`Game.vcxproj` 的 `AdditionalDependencies` 实际链了这些静态库：

```
urge-core urge-binding SDL3-static SDL3_ttf-static SDL3_image-static
libwebpdemux libwebpmux libwebp libsharpyuv physfs-static
SPIRV glslang glslang-default-resource-limits spirv-reflect
wgpu_native imgui-docking miniaudio vorbisfile vorbis opusfile opus ogg
dav1d libwebm libcurl libzs(zlib) ruby-static libffi
+ /MT 静态 CRT
```

其中明显冗余/可裁的：

* `libwebpmux` + `webpencode`/`imageenc`：**编码器**，运行时只解码。
* `libcurl`：HTTP + WebSocket。若不做联网，可 `URGE_ENABLE_NETWORK=OFF`。
* `dav1d` + `libwebm`：AV1 视频。可 `URGE_ENABLE_VIDEO=OFF`。
* `vorbisfile` + `opusfile` + `opus` + `vorbis` + `ogg`：三套音频解码同时存在。
* `ruby-static`：自带 CRuby 1.9.3 全量（85 TU / 489k 行，含 `transcode.c` 的大表、`parse.c`）。

### S5. 40MB 的构成估算（未实测，按库体量排序，供定位用）

| 贡献者 | 量级估计 | 置信度 |
|---|---|---|
| wgpu-native（Rust） | 12–20 MB | 中 |
| CRuby 1.9.3 静态 | 3–5 MB | 中 |
| glslang + SPIRV | 3–5 MB | 中 |
| SDL3 | 2–4 MB | 中 |
| freetype + harfbuzz + plutovg/plutosvg | 2–3 MB | 中 |
| curl + Schannel 胶水 | 1.5–2.5 MB | 中 |
| dav1d（8bit+16bit 表） | 1.5–2.5 MB | 中 |
| libwebp（编+解） | 1–1.5 MB | 中 |
| opus | 1–1.5 MB | 中 |
| urge-core + urge-binding | 2–4 MB | 低 |
| vorbis/ogg/opusfile/zlib/physfs/libffi/imgui/spirv-reflect/miniaudio/webm | ~2 MB | 低 |
| 静态 CRT + 异常展开表 | ~1 MB | 中 |

**要拿准确数字，见第 5 节的测量方法**（`/MAP` 的 `Lib:Object` 列可以直接归属到库）。

### S6. 附带：PDB 也会非常大

`/Zi`（`DebugInformationFormat = ProgramDatabase`）→ 调试信息在 `Game.pdb` 而非 exe，
所以 40MB 基本是纯代码+数据。但 `Game.pdb` 大概率是 **300MB 以上**，分发时注意。

---

## 4. 解决方案（按性价比分级）

### P0 — 零风险，纯配置，立竿见影

#### P0-1 打开并行

命令行（立刻可用，无需改文件）：

```bash
cmake --build build/win64-vendored-ruby --config RelWithDebInfo --target Game --parallel %NUMBER_OF_PROCESSORS%
# 或
set CMAKE_BUILD_PARALLEL_LEVEL=%NUMBER_OF_PROCESSORS%
```

工程内（推荐，永久生效）——在顶层 `CMakeLists.txt`：

```cmake
if(MSVC)
  # /Zi 配 /MP 必须再加 /FS，否则多个 cl 抢写 PDB 会报 C1041 / LNK1201
  add_compile_options($<$<COMPILE_LANG:CXX>:/MP /FS> $<$<COMPILE_LANG:C>:/MP /FS>)
endif()
```

> 注意 `/MP` 要加在 `add_subdirectory(third_party)` **之前**（`add_compile_options` 只对之后
> 创建的目录生效），或者干脆放最上面。

#### P0-2 修掉链接器优化（体积头号）

顶层 `CMakeLists.txt`：

```cmake
if(MSVC)
  # 函数级链接：让每个函数独立成 COMDAT，/OPT:REF 才有东西可删
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:/Gy> $<$<NOT:$<CONFIG:Debug>>:/Gw>)
  # 增量链接与 /OPT:REF 互斥，且会塞 ILT 桩
  set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO /OPT:REF,ICF")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE        "/INCREMENTAL:NO /OPT:REF,ICF")
  # 静态库不需要链接步骤，用 add_link_options 会作用到 exe 与 dll
  add_link_options($<$<NOT:$<CONFIG:Debug>>:/OPT:REF,ICF>)
endif()
```

保守估计 **-20% ~ -40%**（模板密集的 bind 层受 ICF 收益最大）。

#### P0-3 给 wgpu-native 一个正经的 release profile

`third_party/wgpu-cmake/wgpu-native/Cargo.toml`：

```toml
[profile.release]
opt-level  = "z"        # 先要体积；要跑分就改回 3
lto        = true       # fat LTO，跨 crate 死代码消除
codegen-units = 1       # 与 lto 配合，代价是 Rust 编译变慢
panic      = "abort"    # 去掉 unwind 表与 landing pads
strip      = true       # 去掉符号
# debug    = false      # 已是默认
# overflow-checks = false
```

#### P0-4 砍掉 Windows 上用不到的 wgpu feature

`third_party/wgpu-cmake/CMakeLists.txt`：

```cmake
corrosion_import_crate(
  MANIFEST_PATH wgpu-native/Cargo.toml
  NO_DEFAULT_FEATURES
  FEATURES wgsl spirv dx12 vulkan        # 去掉 metal / gles / glsl
)
```

> 前提：着色器路径不变（仍然喂 SPIR-V → 需要 `spirv`）。
> 若改用 WGSL 直喂，可以连 `spirv` 一起去掉，但那是 P2。

---

### P1 — 有收益，需要一点验证

#### P1-1 换 Ninja 生成器（推荐做成一个新 preset）

```jsonc
// CMakePresets.json
{
  "name": "win64-ninja",
  "hidden": true,
  "generator": "Ninja",
  "binaryDir": "${sourceDir}/build/${presetName}",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "URGE_RUBY_INTERPRETER": "bundled"
  },
  "environment": { "CC": "cl", "CXX": "cl" }   // 需在 vcvars64 环境里跑
}
```

Ninja 的调度粒度是 TU 而非工程，天然吃满所有核，还省掉 ZERO_CHECK。
**注意**：Ninja 是单配置生成器，`CMAKE_BUILD_TYPE` 终于生效（VS 生成器会忽略它，
现在 `CMakeCache.txt` 里那个 `CMAKE_BUILD_TYPE=Release` 是无效的，全靠 preset 的 `"configuration"`）。

#### P1-2 给 core/bind 加预编译头

`core/CMakeLists.txt` / `bind/CMakeLists.txt`：

```cmake
target_precompile_headers(urge-binding PRIVATE
  <algorithm> <memory> <string> <unordered_map> <vector>
  "$<$<BOOL:1>:ruby.h>"                 # 顺序很重要，见下
  "webgpu/webgpu_cpp.hpp"
)
```

> **必须遵守本项目的既有铁律**：`<unordered_map>` 等 STL 头要排在 `ruby.h` **之前**
> （`ruby.h` 会拉进 `windows.h`，之后 include STL 会报 `C2061: bidirectional_iterator_tag`）。
> PCH 里正好把"STL → ruby.h"这个顺序一次性固化下来。

日常改 `core/*.h` 时 117 个 TU 的重编成本预计降 30%–50%。

#### P1-3 选择性 unity build

只对体量大、无符号冲突的第三方开：

```cmake
set_target_properties(opus PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
set_target_properties(zlibstatic PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
set_target_properties(physfs-static PROPERTIES UNITY_BUILD ON UNITY_BUILD_BATCH_SIZE 16)
```

**不要**对 SDL3 / curl / libwebp 开（上游明确依赖文件级 static 符号与宏隔离）。
对 `urge-core` 开要小心匿名 namespace 与宏泄漏，建议先试 `urge-binding`（生成代码，风格统一）。

#### P1-4 关掉 ALL_BUILD 里的无用目标

`third_party/CMakeLists.txt` 增补：

```cmake
set(PHYSFS_BUILD_SHARED OFF)     # 引擎链的是 physfs-static
set(PHYSFS_BUILD_TEST OFF)
set(ZLIB_BUILD_TESTING OFF)      # 去掉 minigzip / example
set(SDL_TESTS OFF)               # 去掉 SDL3_test
set(SDLTTF_SAMPLES OFF)          # 已有
```

仅在不带 `--target` 全量构建时体现，但顺手做掉。

#### P1-5 按开关裁掉整块功能

```bash
cmake --preset win64-vendored-ruby -DURGE_ENABLE_VIDEO=OFF     # dav1d + libwebm ≈ 60 TU / 2–3MB
cmake --preset win64-vendored-ruby -DURGE_ENABLE_NETWORK=OFF   # curl ≈ 196 TU / 2MB
```

---

### P2 — 结构性改造，收益最大但工作量也最大

#### P2-1 把 glslang 挪出运行时（强烈推荐）

现状：GLSL 450 以字符串常量写在 `core/pipeline.cc` 顶部，**运行时** glslang→SPIR-V→spirv-reflect。

改成构建期预编译：

1. 开 `ENABLE_GLSLANG_BINARIES ON`，但把它作为 **host tool**（`add_executable(... IMPORTED)` 或用
   `ExternalProject`），产物是 `.spv` 文件；
2. `.spv` 用 `xxd -i` / CMake `#embed`(C23) / `file(READ ... HEX)` 编进二进制；
3. `core/shader.cc` 只读 SPIR-V + `spirv_reflect`，**不再链 glslang / SPIRV / glslang-default-resource-limits**。

收益：**-3 ~ -5 MB**，**-42 个 TU**，**且省掉每次启动的着色器编译**。

#### P2-2 更进一步：让 naga 直接吃 GLSL / WGSL

wgpu-native 已经编了 naga 的 `wgsl`、`glsl`、`spirv` 三个前端。
若把 `core/pipeline.cc` 的着色器改成 WGSL 直喂，可以连 `spirv-reflect` 一起去掉——
但 bind group 布局目前靠 `spirv_reflect` 自动推导（`core/shader.cc:110-180`），
去掉就得改为显式声明布局，改造面较大。

#### P2-3 拆静态库 / 变 DLL

把 `ruby-static`、`wgpu-native`、`dav1d`、`curl` 改成 DLL 随包分发：
好处是 exe 直接降到 10MB 以内、且这些库在多份工具间共享；
坏处是要管 DLL 部署（现在的 `admenri`/`bind/external` 已经在拷 DLL，基础设施现成）。

#### P2-4 缓存

```cmake
# 顶层 CMakeLists.txt（在 project() 之后）
find_program(SCCACHE sccache)
if(SCCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER   ${SCCACHE})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${SCCACHE})
endif()
```

配合 `RUSTC_WRAPPER=sccache` 一起给 Rust 用。
对"切 preset / 切分支 / CI"场景收益极大；首次冷构建无收益。

---

## 5. 怎么拿到准确归因（强烈建议先做这一步）

### 5.1 链接期按库归属

给 Game 加 `/MAP`：

```cmake
target_link_options(Game PRIVATE $<$<NOT:$<CONFIG:Debug>>:/MAP:$(OutDir)Game.map /MAPINFO:EXPORTS>)
```

生成的 `.map` 里 "Public by Value" 段落形如：

```
 Address         Public by Value              Rva+Base     Lib:Object
 0001:00001230   ?Foo@@YAXH@Z                00011230 f   urge-core:bitmap.obj
```

最后一列就是 **`Lib:Object`**。写个脚本按 `Lib` 聚合出现次数/字节跨度即可得到精确排行。
示例：

```python
import re, collections
size = collections.Counter()
for line in open('Game.map', errors='ignore'):
    m = re.match(r'\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]+)\s+[f ]\s+([^:]+):', line)
    if m:
        size[m.group(3)] += 1
for lib, n in size.most_common(30):
    print(f'{n:8d}  {lib}')
```

### 5.2 直接看段大小

```bash
dumpbin /headers build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe
```

`.text` / `.rdata` / `.data` 各自多大一眼可见；`.rdata` 偏大说明是**表**（dav1d / cruby transcode /
glslang 语法表 / opus 码本），`.text` 偏大说明是**代码**（wgpu / glslang / bind 模板）。

### 5.3 逐个关掉做差

最快也最可靠：

```bash
-DURGE_ENABLE_VIDEO=OFF      →  差值 = dav1d + libwebm
-DURGE_ENABLE_NETWORK=OFF    →  差值 = curl
把 corosion features 从 7 个降到 4 个  →  差值 = metal+gles+glsl
```

### 5.4 编译耗时画像

MSVC:

```bash
cl /Bt ...          # 输出每个 TU 的 front/back end 耗时
```

或 MSBuild:

```bash
msbuild /m /clp:PerformanceSummary
```

Ninja:

```bash
ninja -t commands | ...   # 或 ninja -d explain
ninja -j N   # 结束后看 .ninja_log / `ninja -t recompact`
```

---

## 6. 建议的实施顺序

1. **先测量**：按 5.1 打一次 `/MAP`，拿到真实的体积排行（30 分钟，一次性）。
2. **P0 全做**：并行 + `/Gy /Gw /OPT:REF,ICF` + cargo profile + wgpu feature。
   预计：编译时间 **÷5 ~ ÷8**，体积 **-25% ~ -45%**，都不碰业务代码。
3. **P1-1 / P1-2**：Ninja preset + PCH。日常迭代再快一截。
4. **再测量一次**，如果 `wgpu_native` 仍是大头，上 **P2-3（改 DLL）**；
   如果 `glslang` 明显，上 **P2-1（构建期预编译着色器）**——这个还顺带把启动时间砍掉。
5. **P2-4 sccache** 放 CI / 多 preset 场景。

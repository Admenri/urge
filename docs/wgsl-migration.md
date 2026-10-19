# 移除 glslang + spirv-reflect：改用 WGSL + 自研反射

> 改动评估。基于当前树取证，涉及文件、风险点、工作量都列在下面。
>
> **状态：已实施**（2026-10-17）。落地结果与本文的差异记在第 8 节。

---

## 0. 一句话结论

**可行，且收益不小**，但有两处**破坏性公开 API 变更**（`Effect` 的着色器语言、`GPU::SHADER_LANGUAGE_GLSL`），
以及一处**真正的难点**：自研反射必须正确算出 **WGSL uniform/storage 结构体的字节大小** ——
这个数字 (`min_binding_size`) 是承重字段，算错会导致 wgpu 校验失败或运行时越界读。

---

## 1. 现状：glslang 与 spirv-reflect 在哪些地方被用到

我全树 grep 过，接触面比预想的小，只有 **4 个文件 + 3 处 CMake**：

| 位置 | 用途 |
|---|---|
| `core/shader.cc:32-34` | `#include SPIRV/GlslangToSpv.h`、`glslang/Public/*` |
| `core/shader.cc:36` | `#include "spirv_reflect.h"` |
| `core/shader.cc:73-107` | `CompileToSpirv()`：运行时 `TShader/TProgram/GlslangToSpv` |
| `core/shader.cc:109-184` | `ReflectSpirv()`：`spvReflectCreateShaderModule` → `ShaderReflection` |
| `core/shader.cc:186-198` | `CreateModule()`：`wgpu::ShaderSourceSPIRV` |
| `core/gpu.cc:420-440` | `GPU::CreateShaderModule`，`language == ShaderLanguageGlsl` 分支走 glslang |
| `core/device.cc:117-123` | instance 请求 `wgpu::InstanceFeatureName::ShaderSourceSPIRV` |
| `core/CMakeLists.txt:61` | 链 `glslang::glslang glslang::SPIRV glslang::glslang-default-resource-limits spirv-reflect` |
| `third_party/CMakeLists.txt:17-23, 94-100, 169-170` | glslang 开关 + `add_subdirectory(glslang/spirv-reflect)` |
| `third_party/glslang/`、`third_party/spirv-reflect/` | 源码（12 万行 + 6 千行） |

> 顺带发现：`third_party/CMakeLists.txt:94-100` 那组 `SPIRV_*` 变量和它的注释（"The Tint SPIR-V
> reader builds its IR with SPIRV-Tools"）**本来就是空转的** —— `third_party/glslang/External/`
> 下只有一个 `CMakeLists.txt`，`.gitmodules` 里声明的 `third_party/SPIRV-Tools` 子模块在树里不存在。
> 删掉不会有任何副作用。

---

## 2. 反射产物被谁消费（决定"自研反射"必须输出什么）

`core/shader.h:35-55` 定义：

```cpp
struct ShaderBinding {
  uint32_t binding = 0;
  WGPUBufferBindingType buffer = WGPUBufferBindingType_Undefined;
  bool texture = false;
  bool sampler = false;
  uint64_t min_binding_size = 0;      // ← 承重字段
};
struct ShaderGroup   { std::vector<ShaderBinding> bindings; };
struct ShaderAttribute { uint32_t location; WGPUVertexFormat format; };
struct ShaderReflection {
  std::vector<ShaderGroup> groups;     // 索引 = set
  std::vector<ShaderAttribute> attributes;
};
```

消费点：

| 消费者 | 用了什么 | 依赖强度 |
|---|---|---|
| `Pipeline` 构造 → `CreateGroupLayouts` (`shader.cc:220-272`) | `groups` 全量、`binding`、类型、`min_binding_size` | 强 |
| `Pipeline` 构造 → 顶点布局 (`shader.cc:350-383`) | `attributes` 的 `location` + `format`；`format` 还用来算 `stride`（`FormatSize`） | 强 |
| `Effect::CreateInternal` (`effect.cc:234-241`) | `group_bindings(0)` 必须恰好 1 个 uniform 且 `min_binding_size >= sizeof(SceneData)`；set 1 同理对 `ObjectData` | **强，且是校验门槛** |
| `Effect::CreateInternal` (`effect.cc:251`) | `group_bindings(2)` → 用户自定义 slot → `custom_bindings` | 强 |
| `Effect::SetBufferBytes` (`effect.cc:272-274`) | `AlignUniformSize(max(size, min_binding_size))` 决定 buffer 字节数 | **强，算小了会越界写** |
| `Effect::RebuildBindGroup` (`effect.cc:326`) | `zero_buffer(min_binding_size)` | 中 |
| `MakeEntry` (`shader.cc:213-215`) | `entry.buffer.minBindingSize` → 交给 wgpu 校验 | 强 |

**结论**：自研反射不能只解析"有几个 binding、是什么类型"，**必须把 uniform/storage 结构体的
WGSL 布局尺寸算准**。

---

## 3. 改动清单

### A. 重写 11 段内置着色器 → WGSL（`core/pipeline.cc:35-293`）

| 常量 | 阶段 |
|---|---|
| `kVS_TransformBase` | vertex |
| `kFS_TextureBase` / `kFS_TextureMaskBase` / `kFS_PresentBase` / `kFS_ColorBase` / `kFS_TintBase` | fragment |
| `kVS_SpriteBatch` | vertex |
| `kFS_SpriteBatch` | fragment |
| `kFS_PlaneBase` / `kFS_TransitionAlpha` / `kFS_TransitionMap` | fragment |

GLSL → WGSL 对照（本树会用到的全部语法）：

| GLSL 450 | WGSL |
|---|---|
| `#version 450` | 无 |
| `layout(location=N) in vec4 x;` | `@location(N) x: vec4<f32>`（函数参数或 struct 成员） |
| `layout(location=N) out vec2 v;` | 输出 struct 成员 `@location(N) v: vec2<f32>` |
| `layout(location=N) flat out vec4 v;` | 输出 struct 成员 `@location(N) @interpolate(flat) v: vec4<f32>` |
| `layout(set=S,binding=B) uniform SceneData{mat4 m;} u;` | `struct SceneData{m: mat4x4<f32>}` + `@group(S) @binding(B) var<uniform> u: SceneData;` |
| `layout(std430,set=1,binding=0) readonly buffer P{SpriteParam p[];} u;` | `@group(1) @binding(0) var<storage, read> u: array<SpriteParam>;` |
| `uniform texture2D u_tex;` | `@group(S) @binding(B) var u_tex: texture_2d<f32>;` |
| `uniform sampler u_smp;` | `@group(S) @binding(B) var u_smp: sampler;` |
| `texture(sampler2D(t,s), uv)` | `textureSample(t, s, uv)` |
| `gl_Position` | 输出 struct 的 `@builtin(position) position: vec4<f32>` |
| `gl_VertexIndex` | 入口参数 `@builtin(vertex_index) vi: u32`（整型，注意 `/ 6u`） |
| `mix/pow/step/clamp/dot/fract` | 同名 |
| `mat4 / vec4 / vec3 / vec2 / float` | `mat4x4<f32> / vec4<f32> / vec3<f32> / vec2<f32> / f32` |

当前用到的 5 个结构体（**这些就是反射要算尺寸的对象**）：

| 结构体 | 字段 | 地址空间 | 期望字节数 |
|---|---|---|---|
| `SceneData` | `mat4` | uniform | 64 |
| `ObjectData` | `mat4` | uniform | 64 |
| `TintParam` | `vec4, vec4` | uniform | 32 |
| `PlaneParam` | `vec4, vec4` | uniform | 32 |
| `SpriteParam` | `mat4, vec4, vec4, f32, f32` | storage | 104 → 数组元素跨距按 16 对齐 |

### B. 实现自研反射（替换 `core/shader.cc` 的 `CompileToSpirv` + `ReflectSpirv`）

需要产出的三类信息：

**B1. 绑定** —— 扫模块级 `var` 及其前导属性：
- `@group(g) @binding(b) var<uniform> x: T` → `buffer = Uniform`，`min_binding_size = SizeOf(T, uniform)`
- `var<storage, read>` → `ReadOnlyStorage`；`var<storage, read_write>` → `Storage`
- `var x: texture_2d<f32>` → `texture = true`
- `var x: sampler` → `sampler = true`

**B2. 顶点属性** —— 扫 vertex 入口的 `@location(l)` 参数（直参或 struct 成员两种写法都要支持）：
- `vec4<f32>` → `Float32x4`，`vec3<f32>` → `Float32x3`，`vec2<f32>` → `Float32x2`，`f32` → `Float32`

**B3. 类型尺寸**（难点）—— WGSL 布局规则的最小子集：

```
f32 / i32 / u32           align 4   size 4
vec2<f32>                 align 8   size 8
vec3<f32>                 align 16  size 12
vec4<f32>                 align 16  size 16
mat4x4<f32>               align 16  size 64
struct S                  align = max(成员 align)；uniform 地址空间下再向上取到 ≥16
                          size  = roundUp(align, last_offset + last_size)
array<T, N>               N * roundUp(align(T), size(T))
array<T>（runtime）       只出现在 storage 尾部
```

> 注意 uniform 与 storage 的规则差别：**uniform 地址空间要求 struct 的 align 至少 16**，
> storage 没有这条。混用会算错。

**需要处理的词法噪声**：`//` 行注释、`/* */`、字符串字面量、`alias` 类型别名、
`@group(0) @binding(0)` 与 `var` 之间隔多行、struct 嵌套。
**不需要**做表达式/语句解析 —— 规模估计 **300–600 行**。

### C. `core/shader.cc` 的模块创建

```cpp
// 现在
wgpu::ShaderSourceSPIRV spirv;
spirv.code = shader.spirv().data();
spirv.codeSize = shader.spirv().size();

// 改为
wgpu::ShaderSourceWGSL source;
source.code = View(shader.source());   // const char*，不是 uint32_t 流
```

`Shader` 类成员 `std::vector<uint32_t> spirv_` → `std::string source_`（`shader.h:63, 69`）。

### D. `core/gpu.cc` + `core/gpu.h`：决定 GLSL 公开 API 怎么处置 ⚠️

`gpu.cc:420-440` 现在按 `language` 分两条路。`gpu.h:1020-1024`：

```cpp
enum ShaderLanguage { ShaderLanguageWgsl = 0, ShaderLanguageGlsl = 1 };
```

`bind/binding_gpu.cc:256-259` 把它导出成 Ruby 常量 `GPU::SHADER_LANGUAGE_WGSL / _GLSL`。

三个选项：

1. **删掉 `ShaderLanguageGlsl`** —— 干净，但脚本里写 `language = GPU::SHADER_LANGUAGE_GLSL` 会 `NameError`。
2. **保留枚举值，一律按 WGSL 处理** —— 兼容旧脚本不崩，但传 GLSL 源码会得到 naga 的语法报错（信息不友好）。
3. **保留枚举值，传 GLSL 时抛 `RGSSError`** —— 明确告知"GLSL 支持已移除，请迁移到 WGSL"。

另外 `descriptor->stage` 现在只被 glslang 分支用（WGSL 不需要 stage 信息），可保留做兼容。

> **最终决定（2026-10-18）：整个语言维度删掉，三个选项都不取。**
> 既然只有一种语言，"选语言"这件事本身就没有意义，留一个恒等于 WGSL 的枚举只是把
> 已经消失的分支留在 API 上。实际删除：
>
> | 删掉的东西 | 位置 |
> |---|---|
> | `enum ShaderLanguage { ... }` | `core/gpu.h` |
> | `enum ShaderStage { ... }`（只为 glslang 分支存在） | `core/gpu.h` |
> | Ruby 常量 `GPU::SHADER_LANGUAGE_WGSL` / `_GLSL` / `SHADER_STAGE_VERTEX` / `_FRAGMENT` | 生成物 |
> | `GPUShaderModuleDescriptor#language` / `#stage`（读写对） | `core/gpu.h` 的 `ATTR` |
> | 字段 `language` / `stage` | `GPUShaderModuleDescriptor` |
>
> `CreateShaderModule` 现在只做一件事：把 `code` 当 WGSL 交给设备（空串直接抛错）。
> 阶段由 `@vertex` / `@fragment` 属性决定，模块自己说，描述符不必再说一遍。
> 代价：脚本里写 `GPU::SHADER_LANGUAGE_*` 会 `NameError`（这是有意为之的破坏性变更）。
> GLSL 的友好报错仍然保留，但只在内置管线与 `Effect` 那条路上（`ReflectWgsl` 嗅 `#version`）。

### E. `core/effect.h/.cc`：破坏性变更 ⚠️

`Effect.new(vs_glsl, fs_glsl, blend_states)` 现在收 GLSL。改 WGSL 后**所有用 Effect 的用户脚本都要重写着色器**。
需要同步改：
- 参数名 `vs_glsl` / `fs_glsl` → `vs_wgsl` / `fs_wgsl`（`URGE_BINDING()` 标记 → **改完必须重跑 configure**）
- 校验文案 `"effect: the stages have to declare the scene uniform at set 0 binding 0..."`（`effect.cc:240-244`）
- `SetBlock(slot, string)` 传原始字节，语义不变

### F. `core/device.cc:117-123`：去掉 instance feature

```cpp
const std::vector<wgpu::InstanceFeatureName> instance_exts = {
    wgpu::InstanceFeatureName::ShaderSourceSPIRV,   // ← 删
};
```

**这是额外收益**：按项目既有记录，Emscripten/emdawnwebgpu 上请求 `ShaderSourceSPIRV`
会让 `WGPUInstanceImpl::Create` 直接 `return {}`（instance 为空）。去掉它，web 构建才可能走通。

### G. wgpu-native features

`third_party/wgpu-cmake/CMakeLists.txt` 现在无参数调用 `corrosion_import_crate`，走
`Cargo.toml` 的 default（`wgsl spirv glsl dx12 metal vulkan gles`）。建议改成：

```cmake
corrosion_import_crate(
  MANIFEST_PATH wgpu-native/Cargo.toml
  NO_DEFAULT_FEATURES
  FEATURES wgsl dx12 vulkan
)
```

> ⚠️ 参数名以 Corrosion **v0.6** 为准（本树 FetchContent 拉的是 v0.6），实施前先核对
> `_deps/corrosion-src/cmake/Corrosion.cmake` 里的签名。

### H. CMake

| 文件 | 改动 |
|---|---|
| `core/CMakeLists.txt:61` | 改为只留 `webgpu-cpp-header` |
| `third_party/CMakeLists.txt:169` | 删 `add_subdirectory(glslang)` |
| `third_party/CMakeLists.txt:170` | 删 `add_subdirectory(spirv-reflect)` |
| `third_party/CMakeLists.txt:17-23` | 删 `ENABLE_GLSLANG_BINARIES / ENABLE_GLSLANG_JS / GLSLANG_TESTS / GLSLANG_ENABLE_INSTALL / ENABLE_HLSL / ENABLE_OPT / ENABLE_SPIRV` |
| `third_party/CMakeLists.txt:94-100` | 删 `SPIRV_*` 那组（含那条 Tint 注释，本来就是空转的） |
| `third_party/glslang/`、`third_party/spirv-reflect/` | 整目录删 |
| `.gitmodules` | 删 `third_party/glslang` 与 `third_party/SPIRV-Tools` 两条 |

改完**必须重新 configure**（项目铁律：删/加目标不是 `--build` 能解决的）。

### I. 文档

- `README.md`：第 31、44、154、155、160、198 行
- `README_EN.md`：第 34、47、159、160、166、209-210 行
- set 约定表格（0/1/2/3）**保留不变**，只改"GLSL → glslang → SPIR-V → spirv-reflect"那段的措辞

---

## 4. 风险点（按严重度）

1. **`min_binding_size` 算错**（最高）
   - 算小 → `Effect::SetBufferBytes` 分配过小 buffer → wgpu 校验失败或 GPU 越界读
   - 算大 → 浪费，且 `zero_buffer` 缓存膨胀
   - **缓解**：给内置 5 个结构体写死期望值并加单元断言；对算不出的类型置 0（wgpu 视为不校验），
     让 `Effect` 的 set0/set1 门槛给出**明确报错**而不是静默通过。

2. **用户脚本的 Effect 着色器全部失效**
   - 是否给兼容期？建议：检测到源码含 `#version` 就抛 "GLSL 已移除" 的明确错误，别让 naga 报语法错。

3. **WGSL 属性写法有两处**（函数参数 vs struct 成员），反射两种都要支持，否则用户换个写法就失效。

4. **storage 与 uniform 布局规则不同**（struct align 16 下限只在 uniform），容易漏。

5. **验证成本**：改动后必须跑通完整渲染路径（Bitmap / Sprite / Plane / Viewport tint / Window 模板裁剪 /
   Transition / Tilemap / Effect），而不是只看"能启动"。

---

## 5. 收益

| 项 | 数字 |
|---|---|
| 编译单元 | **-43 TU**（glslang 42 + SPIRV + spirv-reflect 1） |
| 源码量 | **-12.6 万行** |
| exe 体积 | **-3 ~ -5 MB**（glslang 语法表 + SPIRV 生成器 + 反射器） |
| 启动时间 | 省掉 `ShaderSet` 构造时 11 段着色器的**运行时** GLSL→SPIR-V 编译 |
| Rust 侧 | 少 `spirv` + `glsl` 两个 naga 前端 → wgpu-native 编译更快、`.lib` 更小 |
| 跨平台 | 去掉 `ShaderSourceSPIRV` instance feature → 扫清 Emscripten 构建的一个前提障碍 |
| 依赖简化 | 少两个子模块、两套构建开关 |

---

## 6. 工作量估计

| 阶段 | 估计 |
|---|---|
| 11 段着色器重写为 WGSL | 0.5–1 天 |
| 自研反射（词法扫描 + 布局计算） | 1–2 天 |
| 接线（shader.cc / gpu.cc / device.cc / CMake / 文档） | 0.5 天 |
| 验证（全渲染路径 + Effect） | 0.5–1 天 |
| **合计** | **2.5–4.5 天** |

---

## 7. 建议的实施顺序

1. 先写反射的**布局计算**并加断言（SceneData 64 / ObjectData 64 / TintParam 32 / PlaneParam 32），跑通单测。
2. 再重写 1 段最简单的着色器（`kFS_ColorBase` + `kVS_TransformBase`），确认 `ColorBase` 能出图。
3. 铺开其余 9 段。
4. 最后处理公开 API 处置（第 D/E 节那两个决策点）。
5. 收尾：CMake / 文档 / 子模块。

---

## 8. 实施记录（2026-10-17）

### 与本文预估的差异

| 项 | 预估 | 实际 |
|---|---|---|
| 反射器规模 | 300–600 行 | `core/wgsl_reflect.cc` 约 690 行（含注释） |
| 着色器段数 | 11 | 11（`kFS_TextureBase` 被 `TextureBase` 与 `TextureBaseDynamic` 复用，所以是 12 个使用点） |
| 类型定义位置 | 留在 `shader.h` | 搬到 `wgsl_reflect.h`，`shader.h` 只 `#include` 它 |
| `Shader` 成员 | `std::string source_` | `std::string wgsl_` + `ShaderReflection reflection_`（入口名在反射结果里） |
| 入口函数名 | 未定 | `vs_main` / `fs_main`，但由 `@vertex`/`@fragment` 属性**查出来**，不写死 |
| 第 D 节 | 三选一 | **三个都没取**：2026-10-18 按用户要求把语言维度整个删了（见 D 节的表格） |
| 第 G 节（wgpu-native features） | 建议 `NO_DEFAULT_FEATURES` | **没做**：corrosion 的 `FEATURES` 参数名与 `Cargo.toml` 的 feature 集合要先核对，留作后续 |
| `@align`/`@size` | 未讨论 | 有意不实现（树里没有使用者），在头文件里写明 |

### 风险点的处置

1. **`min_binding_size`**：反射器按 WGSL 规范算。用临时 harness 断言了四个关键数字
   （`SceneData`/`ObjectData` = 64、`TintParam`/`PlaneParam` = 32、`array<SpriteParam,4>` = 448
   即元素 112，与 `sprite_batch.cc` 的 `static_assert` 一致）。runtime-sized array 给 0。
2. **Effect 的 GLSL 脚本**：反射器见到 `#version` 或 `gl_Position` 直接抛
   "the source is GLSL ... glslang is no longer part of the build"。
   `GPU::CreateShaderModule` 那条路不做这个嗅探——那里没有语言选项，源码原样交给 wgpu。
3. **顶点属性两种写法**：都支持（入口参数直接 `@location`，或 struct 成员）。
4. **uniform / storage 布局差异**：`LayoutOf(type, uniform, depth)` 里显式区分，
   uniform 下 struct 与 array 的 align 向上取到 16。
5. **验证**：11 段着色器用 **naga 29.0.4**（wgpu 自己的 WGSL 前端，cargo 缓存里现成）离线
   `parse_str` + `Validator::validate` 全部通过。**运行期全渲染路径（Bitmap / Sprite / Plane /
   Viewport tint / Window 模板裁剪 / Transition / Tilemap / Effect）还没跑**，需要带 `Data/` 的游戏目录。

---

## 9. 回归：sprite 半透明（2026-10-18 修复）

### 现象

Sprite 与 Plane 等节点整体发虚 / 半透明。

### 根因：`select()` 的参数顺序与 GLSL 三元运算符相反

`kFS_SpriteBatch` 里那一行灌木裁剪，GLSL 原文是：

```glsl
float under_bush = v_texcoord.y > v_bush_depth ? 0.0 : 1.0;   // cond ? 假值 : 真值
```

WGSL 初版写成了：

```wgsl
let under_bush = select(0.0, 1.0, input.v_texcoord.y > input.v_bush_depth);
```

**`select(f, t, cond)` 返回的是 `t`（第二个参数）当 `cond` 为真** —— 与 `cond ? a : b` 的
"真值在前"正好相反。于是 `under_bush` 被整个取反。

为什么这一位翻转能让**每个** sprite 都变半透明：

- `core/sprite.h:118` 的默认值是 `bush_opacity_ = 128`（即 0.502）；
- `core/sprite.h:119` 的默认值是 `bush_depth_ = 0`，于是
  `Sprite::MakeParamInternal` 算出 `bush_depth = (src.y + src.height) / texture_height`，
  也就是**源矩形的下边界**；而 `v_texcoord` 是按整张纹理归一化的，永远不超过这条线。
- 所以 `v_texcoord.y > v_bush_depth` 恒为 **假**：
  - GLSL：`under_bush = 1.0` → `clamp(0.502 + 1.0) = 1.0` → 不透明（正确）；
  - 初版 WGSL：`select(0.0, 1.0, false) = 0.0` → `clamp(0.502 + 0.0) = 0.502` → **全部 sprite 50% 透明**。

只有 `bush_depth_ > 0`（真用灌木效果）时两条式子才会重合，所以这个 bug 在默认场景里 100% 命中。

### 修复

改成显式 `if`，把顺序问题从语法层面消掉：

```wgsl
var under_bush = 1.0;
if (input.v_texcoord.y > input.v_bush_depth) {
  under_bush = 0.0;
}
```

### Plane 为什么也"看着"不对

Plane 的片元着色器与 GLSL 逐行等价（`apply_tint` + `fract` + `* v_color`），管线布局、
顶点布局、uniform 大小（32）都与改动前一致，**Plane 自身的 alpha 只由位图的 alpha 决定**，
不经过 bush 那一条。看到的现象是 sprite 以 50% 覆盖在 Plane 上，Plane 透了出来。

### 教训

- WGSL 的 `select(f, t, cond)` 是本轮唯一一处"看起来能直译、实际参数顺序相反"的 API。
  凡是 GLSL 的 `?:`，要么写成 `if`，要么记牢 **`select(假值, 真值, 条件)`**。
- 这类语义差异 naga 不会报错（类型完全合法），`naga` 离线校验和反射自检都过不了这一关 ——
  只能靠"逐行对照原文 + 代入默认值算一遍"。

---

## 10. 反射器也删了：布局改手写，Effect 走 auto layout（2026-10-19）

自研反射器 `core/wgsl_reflect.{h,cc}` 只活了一天就被删掉了。原因很直接：反射出来的三样东西里，

- 顶点属性 —— 引擎里**每一个**管线都是同一套（loc0 vec4 / loc1 vec2 / loc2 vec4）；
- 绑定条目 —— 内置 11 段着色器的绑定是**固定的、写死的**，反射只是在运行时把它们重新读一遍；
- `min_binding_size` —— 唯一"算"出来的东西，而且算错了就是难查的语义错误（第 9 节的 `select`）。

为了这点收益维护一个 690 行的 WGSL 解析器不划算。现在：

| 以前 | 现在 |
|---|---|
| `core/wgsl_reflect.cc` 扫 `@group`/`@binding`/`@location` | 删掉，改用 `core/gpu_utils.h` 的 `util::CreateBindLayout` |
| `Shader::Compile(stage, wgsl)` | `ShaderSource{ wgsl, entry_point, groups }`，groups 手写 |
| 顶点布局由反射推导 | `QuadVertexLayout()`（`core/shader.h`），所有管线共用 |
| `Pipeline(vs, fs, {{0,1,2}}, {1}, 3)` | `Pipeline(vs, fs, QuadVertexLayout(), {1})` |
| Effect 用反射出的 `group_bindings(0/1/2)` 校验 + 建组 | `Pipeline::AutoLayout`：layout 传 null，再从 `pipeline.GetBindGroupLayout(2)` 取回 |

### 手写布局长什么样

```cpp
const ShaderSource kFS_TintBase = {
    R"( ...WGSL... )",
    "fs_main",
    {
        {2, {Texture(0, wgpu::ShaderStage::Fragment),
            Sampler(1, wgpu::ShaderStage::Fragment)}},
        {3, {Uniform(0, wgpu::ShaderStage::Fragment,
                     sizeof(TintBase::TintParam))}},
    },
};
```

每对是「组号 → 该组条目」，组号跟着条目走，所以只在 `@group(2)` 采样的片元阶段
不必在前面补两个空组。`Uniform`/`Texture`/`Sampler`/`ReadOnlyStorage` 是 `pipeline.cc`
里的四行小工具，只是把 `util::BufferLayout::Uniform(...)` 之类包了一下。

组号按数字合并（`Pipeline` 里 `std::map<uint32_t, ...>`），`dynamic_sets` 里的组会把
该组所有 buffer 条目的 `hasDynamicOffset` 置上——所以同一个 VS 既能喂池化绘制也能喂独立绘制。

### Effect 的代价

- 入口名不再能查源码，约定为 **`vs_main` / `fs_main`**（与内置一致）。
- 槽位是纹理、采样器还是 uniform，由**调用哪个 setter** 决定，没有反射可查。
- 因此：shader 在 `@group(2)` 声明的每个 binding，**脚本必须在首帧前设过**。以前没设的会兜底成
  白纹理 / 默认采样器 / 零 buffer，现在没有这个值——`minBindingSize` 只有设备知道。
- 组 0 / 组 1 的校验退化成"`GetBindGroupLayout(0/1)` 是否为 null"。

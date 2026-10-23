# 用 CMake 构建 Ruby 4.0

本仓库的构建系统已从 GNU make（`common.mk` / `defs/gmake.mk` / `template/Makefile.in` /
`enc/Makefile.in` / `win32/Makefile.sub`）翻译为 CMake。构建过程不再需要 `make`、
`configure`、shell 脚本或任何外部 Ruby 解释器。

已验证：

| 平台 | 编译器 | 状态 |
|------|--------|------|
| Linux (WSL2 / Ubuntu 26.04) | GCC 15.2 | ✅ 构建 + 冒烟测试通过 |
| Windows 11 x64 | MSVC 19.51 (VS 2026) | ✅ 构建 + 冒烟测试通过 |
| Windows x64 | MinGW-w64 | ⚠️ 代码路径已就绪，未实测 |
| macOS | AppleClang | ⚠️ 代码路径已就绪，未实测 |
| WebAssembly | Emscripten | ⚠️ 代码路径已就绪，未实测（`wasm/` 分支） |

> YJIT / ZJIT 不参与构建（配置时显式关闭，且选项开启会直接报错）。
> 本构建是"核心解释器"构建：不编译 `ext/` 下的任何 C 扩展，也不打包 bundled gems。

---

## 1. 快速开始

### Linux / macOS / WSL2

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ruby -I lib smoke_test.rb
ctest --test-dir build --output-on-failure
```

推荐使用 Ninja（`-G Ninja`），没有也可用默认的 Unix Makefiles 生成器——那只是
CMake 的后端，和原来手写的 Makefile 无关。

安装：

```bash
cmake --install build --prefix /opt/ruby-4.0
```

### Windows（MSVC）

```bat
cmake -S . -B build-msvc -G "Visual Studio 18 2026" -A x64
cmake --build build-msvc --config Release
build-msvc\Release\ruby.exe -I. -Ilib smoke_test.rb
```

### Windows（MinGW-w64）

```bash
cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build-mingw -j
```

MinGW 走 POSIX 特性探测路径，并链接 `win32/win32.c`、`win32/file.c` 与 Win32 线程实现。

### WebAssembly（Emscripten，未实测）

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j
```

---

## 2. 选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `RUBY_ENABLE_YJIT` | `OFF` | 打开即报错：本构建不支持 YJIT |
| `RUBY_ENABLE_ZJIT` | `OFF` | 同上 |
| `RUBY_USE_GMP` | `OFF` | 找到 GMP 时启用（Bignum 加速） |
| `RUBY_DISABLE_RUBYGEMS` | `ON` | 核心构建不加载 RubyGems（它依赖 default gems 的扩展） |
| `RUBY_BUILD_SMOKE_TEST` | `ON` | 注册唯一的测试脚本到 CTest |
| `RUBY_INSTALL_RULES` | `ON` | 生成 install 规则 |
| `RUBY_PREFIX` | `CMAKE_INSTALL_PREFIX` | 编译进解释器的安装前缀 |

---

## 3. 文件结构

```
CMakeLists.txt              顶层：目标定义、选项、安装/测试
cmake/
  RubyPlatform.cmake        架构命名（x86_64-linux / x64-mswin64 / …）、编译链接辅助
  RubySources.cmake         源文件清单（对应 common.mk 的 COMMONOBJS/PRISM_FILES）
  RubyUnix.cmake            POSIX 特性探测 -> ruby/config.h（GCC/Clang/MinGW/Emscripten）
  RubyWindows.cmake         Windows 公共部分（win32 层、Win32 线程、协程后端）
  RubyMSVC.cmake            MSVC 专用配置 -> ruby/config.h
  RubyGenerators.cmake      rbconfig.rb / verconf.h / builtin_binary.rbbin / encdb.h
  config.h.in               POSIX 配置头模板
  config_win32.h.in         MSVC 配置头模板（移植自 win32/Makefile.sub）
  rbconfig.rb.in            RbConfig 模板
  verconf.h.in              编译进 loadpath.c 的安装路径
  coroutine_prefix.h        协程汇编需要的符号前缀宏（-include 注入）
  dummy_probes.h            DTrace 关闭时的空 probes.h
  encdb.h                   内置编码清单（由 enc/*.c 生成一次，平台无关）
  encinit_builtin.c         静态内置编码的 Init_enc
smoke_test.rb               唯一的测试脚本
tool/generic_erb.rb         构建期唯一的 Ruby 工具（生成 builtin_binary.rbbin）
tool/lib/{output,vpath,colorize}.rb   generic_erb.rb 的依赖
template/builtin_binary.rbbin.tmpl    builtin_binary.rbbin 的 ERB 模板
```

---

## 4. 构建流程

与原构建一致的两阶段 bootstrap，只是驱动换成了 CMake：

```
configure 阶段
  ├─ 特性探测（头文件 / 函数 / 类型大小 / 结构体成员 / 编译器属性）
  ├─ 生成 include/ruby/config.h
  ├─ 生成 rbconfig.rb、verconf.h、encdb.h、probes.h
  └─ 探测线程实现（pthread / win32 / none）与协程后端

build 阶段
  ├─ rubycore（OBJECT）：COMMONOBJS + Prism + 内置编码 + 协程 + missing/
  ├─ miniruby  = rubycore + main.c + miniinit.c + dmydln.c
  ├─ miniruby 生成 builtin_binary.rbbin（tool/generic_erb.rb + ERB）
  ├─ ruby-static = rubycore + dln.c + localeinit.c + loadpath.c + builtin.c
  └─ ruby      = ruby-static + main.c + dmyext.c + encinit_builtin.c
```

要点：

* `builtin.c` 通过 `#include "builtin_binary.rbbin"` 使用生成文件，CMake 用
  `OBJECT_DEPENDS` 保证先生成再编译（并行构建安全）。
* `parse.c`、`miniprelude.c`、`*.rbinc`、`enc/unicode/17.0.0/*.h`、`enc/trans/*.c`
  在源码树里已经是生成好的，所以**配置阶段不需要任何 Ruby 解释器**。
* 协程汇编通过 `-include cmake/coroutine_prefix.h` 注入 `PREFIXED_SYMBOL()`，
  因为 CMake 无法传递带括号的宏定义。

---

## 5. 平台无关性设计

原则：**只探测能力，不枚举操作系统。**

* 头文件、函数、类型、结构体成员、编译器属性全部用 `check_*` 试探；
  缺失的函数自动回落到 `missing/*.c`（与原 autoconf 的 `AC_REPLACE_FUNCS` 一致）。
* `SIZEOF_*`、`SIGNEDNESS_OF_*`、`*2NUM`、`PRI_*_PREFIX` 由类型实际大小和符号性
  推导（见 `ruby_typedef_conversions`），因此 32/64 位、LP64/LLP64 都成立。
* 线程实现按 `pthread.h` + `Threads::Threads` 是否可用选择，否则退化为 `thread_none`。
* 协程按 CPU 选择 `coroutine/<arch>/Context.S`；没有对应实现时退回 `ucontext`，
  再退回 `pthread`。
* 未知系统（例如某个只有 C 标准库的类 Unix 系统）会得到
  `<cpu>-<osname>` 的 arch 字符串，并按探测结果生成 `config.h`；缺什么就用
  `missing/` 或 Ruby 自带的 `#else` 分支。若编译器没有 GNU 属性，
  `config.h` 会退化成空宏定义。

新增平台通常只需要：在 `RubyPlatform.cmake` 里补 arch 命名，必要时在
`RubyUnix.cmake` 里补一个协程/线程分支。

---

## 6. 与原 make 构建的差异

| 项目 | 原构建 | CMake 构建 |
|------|--------|-----------|
| 驱动 | make + autoconf + shell | CMake |
| 配置头 | `configure` 生成 | CMake 特性探测生成 |
| `rbconfig.rb` | `mkconfig.rb` 从 Makefile 提取 | 直接由 CMake 模板生成 |
| 扩展 | `extconf.rb` + `extmk.rb` | 不构建（核心构建） |
| bundled gems | 安装 `gems/*.gem` | 已删除 |
| 编码 | 内置 4 个 + 其余为 `.so` | 内置 4 个 + `encdb.c`，无 `.so` |
| YJIT/ZJIT | 可选 | 不支持 |
| DTrace | 可选 | 关闭（空 `probes.h`） |

---

## 7. 源码树精简

源码树只保留"能构建出解释器"的最小集合（约 19 MB / 823 个文件）：

| 已删除 | 原因 |
|--------|------|
| `ext/` | 不构建扩展 |
| `gems/`、`doc/`、`test/`、`spec/`、`sample/`、`benchmark/`、`bootstraptest/`、`basictest/` | 文档与测试（只留 `smoke_test.rb`） |
| `yjit/`、`zjit/`、`jit/`、`Cargo.*`、`yjit.c`、`zjit.c`、`jit.c` | YJIT/ZJIT 不编译 |
| `enc/*.c`（除 4 个内置编码）、`enc/trans/*.c`（除 `newline.c`）、`enc/trans/*.trans`、`*-tbl.rb` | 这些是动态编码/转码表，本构建不生成 `.so` |
| `tool/` 除 `generic_erb.rb` + `tool/lib/{output,vpath,colorize}.rb` | 构建只用这三个文件 |
| `template/` 除 `builtin_binary.rbbin.tmpl` | 其余模板属于旧构建系统 |
| `parse.y`、`insns.def`、`lex.c.blt`、`probes.d`、`configure`、`configure.ac`、`common.mk`、`defs/`、`win32/Makefile.sub` 等 | 生成器与旧 make 构建系统；`parse.c`/`insns.inc`/`lex.c` 等生成结果已保留 |
| `man/`、`misc/`、`libexec/`、`bin/`、`cygwin/`、`.bundle/`、`ChangeLog` 等 | 与构建无关 |
| `lib/rubygems*`、`lib/bundler*`、`lib/prism*`（Ruby 侧 API） | RubyGems 已禁用；Prism 的 C 解析器仍内置，仅删掉 Ruby 包装层 |

保留的多平台文件：`coroutine/`（全部后端）、`win32/`、`wasm/`、`missing/`、
`thread_{pthread,win32,none}.*`、`enc/unicode/17.0.0/`。

> 删除 `parse.y`/`insns.def` 后无法再修改语法或指令集（生成结果 `parse.c`、
> `insns.inc` 等已保留，构建不受影响）。如需回退，见仓库外的
> `ruby-4.0.0-prune-backup.tar.gz`（16 MB）。

---

## 8. 已知限制

1. **没有 C 扩展**：`require "json"`、`socket`、`zlib`、`ripper` 等会失败；
   `lib/` 下的纯 Ruby 标准库（`fileutils`、`optparse`、`erb`、`tmpdir`…）可用。
2. **只有内置编码**：ASCII-8BIT / UTF-8 / US-ASCII / UTF-16/32 系列及其
   `encdb.h` 中的单字节别名，`String#encode("Shift_JIS")` 会抛
   `Encoding::ConverterNotFoundError`（没有 `enc/trans/*.so`）。
3. **RubyGems 默认关闭**：`RUBY_DISABLE_RUBYGEMS=ON`，否则启动时会尝试加载
   依赖扩展的 default gems。`lib/rubygems*`、`lib/bundler*`、`lib/prism*` 已删除，
   所以 `require "rubygems"` / `"bundler"` / `"prism"` 会 `LoadError`
   （Prism 的 C 解析器仍然内置，`ruby -e` 的解析不受影响）。
4. **MSVC 必须用 Release**：Debug 配置下 MSVC 不生成 `static inline` 函数，
   会出现 `rb_set_keys` 之类的未解析符号；这与原 `win32/Makefile.sub`
   始终带优化编译的做法一致。
5. **MSVC 不能加 `/std:c11`**：MSVC 会把枚举位域当有符号数并在读取时符号扩展，
   导致 Ruby 4 位的 `rb_method_type_t` 位域读成负数（`VM_METHOD_TYPE_OPTIMIZED`
   变成 `-7`，表现为 `Proc#call` 崩溃）。构建脚本刻意让 MSVC 保持传统 C 模式。
6. MinGW / macOS / WASM 分支未实测。

---

## 9. 测试

```bash
ctest --test-dir build --output-on-failure
# 或直接
./build/ruby -I build -I lib smoke_test.rb
```

`smoke_test.rb` 覆盖 63 项检查：整数/大整数、字符串与编码、正则（含 Unicode 属性）、
数组/哈希/Enumerable、异常、Proc/关键字参数、Fiber、Rational/Complex、Time、
Marshal、文件 IO、GC、eval。

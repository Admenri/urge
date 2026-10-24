# 第三方依赖的本地补丁

`third_party/` 下的依赖以子模块形式引入，正常情况下**不修改**。下面每一项都是上游缺陷的
临时修补：升级子模块时要么把同等改动带过去，要么先确认上游已经修好。

## `SDL` —— 蓝牙 Xbox 手柄在 Windows 上不可见

- 文件：`src/joystick/hidapi/SDL_hidapi_xboxone.c`，`HIDAPI_DriverXboxOne_IsSupportedDevice()`
- 引入问题的上游提交：`d5fca9628` *"Don't use the fake HID endpoint for Xbox controllers on Windows"*
- 现象：手柄在系统里正常（设备管理器里是 `HID-compliant game controller`，VID `045E` /
  PID `0B13`），但引擎完全看不到它，`SDL_GetJoysticks()` 返回 0，HIDAPI 的调试日志里是
  `driver = NONE (DISABLED)`。
- 原因：那个检查为了跳过 **XGIP（USB）** 手柄的"假 HID 端点"，把所有以 `\\?\HID#` 开头的
  设备路径都拒掉了。**蓝牙** Xbox 手柄的真实端点路径同样以 `\\?\HID#` 开头
  （`\\?\HID#{00001812-...}_Dev_VID&02045e_PID&0b13...`），于是没有任何驱动认领它：
  HIDAPI 的 Xbox 驱动拒绝它，GIP 驱动又明确声明"蓝牙手柄走 HID 而不是 GIP"。
- 补丁：把判定限定在非蓝牙设备上。

  ```c
  -    if (device && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0) {
  +    if (device && !device->is_bluetooth && SDL_strncmp(device->path, "\\\\?\\HID#", 8) == 0) {
  ```

- 验证：打补丁后同一行日志变成 `driver = SDL_JOYSTICK_HIDAPI_XBOX_ONE (ENABLED)`，
  引擎输出 `[Gamepad] 1 joystick(s), 1 gamepad(s)` 与
  `[Gamepad] connected: Xbox Series X Controller`。

## `imgui` —— Emscripten + wgpu-native 被当成已废弃的 `-sUSE_WEBGPU`

- 文件：`backends/imgui_impl_wgpu.cpp`，文件顶部
- 现象：`#if defined(__EMSCRIPTEN__) && defined(IMGUI_IMPL_WEBGPU_BACKEND_WGPU)` 直接
  `#error Emscripten <4.0.10 with '-sUSE_WEBGPU=1' is not supported anymore.`，
  Emscripten 上根本编不过。
- 原因：上游写这条判断时，wgpu 在 Emscripten 上的唯一形态是那个已删除的 `-sUSE_WEBGPU`
  运行时；而当时 wgpu-native 自己就能编到 `wasm32-unknown-emscripten`（GLES / WebGL 2 后端），
  用的还是这个 backend 本来就在写的那份 `webgpu.h`。
- 补丁：删掉那条 `#error`（保留三种 backend 宏必须选一个的那条判断）。

  ```c
  -#if defined(__EMSCRIPTEN__) && defined(IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
  -#error Emscripten <4.0.10 with '-sUSE_WEBGPU=1' is not supported anymore.
  -#endif
  ```

- **现状**：web 构建已经不走这条路了——它现在用 emdawnwebgpu，并定义
  `IMGUI_IMPL_WEBGPU_BACKEND_DAWN`（见 `third_party/imgui/CMakeLists.txt`），
  上游本来就在 Emscripten 上要求这个。补丁保留着无害，只是 web 端不再触发。

## `ruby` —— Emscripten 上 `rb_execarg_commandline` 只有声明没有定义

- 文件：`process.c`，`rb_execarg_commandline()` 的定义
- 现象：wasm 构建在 `process.c` 直接编不过：

  ```
  process.c:4561:21: error: call to undeclared function 'rb_execarg_commandline';
  ISO C99 and later do not support implicit function declarations
  ```

- 原因：`rb_spawn` 里这个函数的**调用点**在 `#if defined HAVE_SPAWNV ... #else` 的
  `#else` 分支里，没有任何平台判断；而它的**定义**多了个 `&& !defined __EMSCRIPTEN__`。
  Emscripten 既没有 `fork` 也没有 `posix_spawn`（`spawn.h` 只有声明，链接时
  `undefined symbol: posix_spawnp`），`HAVE_WORKING_FORK` / `HAVE_SPAWNV` 都不成立，
  于是调用点在、定义不在。
- 补丁：把 `__EMSCRIPTEN__` 从定义的判断里去掉，让定义与调用点对同一个条件。

  ```c
  -#if !defined HAVE_WORKING_FORK && !defined HAVE_SPAWNV && !defined __EMSCRIPTEN__
  +#if !defined HAVE_WORKING_FORK && !defined HAVE_SPAWNV
  ```

- 验证：`cmake --build --preset emscripten-wasm32` 通过；`spawn` 在浏览器里本来就没有
  意义，`system()` 是 Emscripten 的空实现，RGSS 也不调用它。

## `ruby` —— Emscripten 上 `ruby_stack_overflowed_p` 没有定义

- 文件：`wasm/missing.c`
- 现象：

  ```
  wasm-ld: error: signal.c.o: undefined symbol: ruby_stack_overflowed_p
  ```

- 原因：`check_stack_overflow()` 是"有致命信号但没有 `sigaltstack`"时的兜底。Emscripten 的
  `signal.h` 定义了 `SIGSEGV` 等编号（只是永远不会真的送来，wasm 里出错是 trap 不是信号），
  而 `USE_SIGALTSTACK` 需要 `SA_SIGINFO` + `sigaltstack`，两者都没有，于是走这条兜底分支。
  它调用的 `ruby_stack_overflowed_p()` 只在 `thread_pthread.c` / `thread_win32.c` 里定义，
  而单线程 wasm 构建两个都不编译。
- 补丁：在 wasm 端补一个弱符号，和其它 `WASM_MISSING_LIBC_FUNC` 一样——这条分支在浏览器里
  永远走不到，返回 0 就够了。**不要**改成"把这段代码裁掉"：`SIGSEGV` 是定义了的，裁剪的
  守卫条件与调用点对不上。

## `ruby` —— 功能探测把只有声明没有实现的函数当成可用

- 文件：`cmake/RubyUnix.cmake`，`# --- WebAssembly (Emscripten) ---` 段
- 现象：`box.c` 引用 `copy_file_range`、`file.c` 引用 `lutimes`、`process.c` 引用
  `initgroups`，链接时 `undefined symbol`。
- 原因：探测是 `int main(void) { (void)copy_file_range; return 0; }`——取个地址就完事，
  编译器把它优化掉，不需要符号就能链接通过；而 Emscripten 的 sysroot 里这几个函数**只有
  声明**。配置脚本因此写下 `HAVE_COPY_FILE_RANGE` / `HAVE_LUTIMES` / `HAVE_INITGROUPS`，
  Ruby 就走"系统有就用系统的"那条路。
- 补丁：在 Emscripten 段里把这三项从 `RUBY_FEATURE_DEFINES` 里删掉，让 Ruby 用它自带的
  可移植实现（`Process.initgroups` 因此不存在，浏览器里也没有用户库可查）。同一段里还补上了
  `wasm/machine_core.S`：`rb_wasm_get_stack_pointer` 只能用 WebAssembly 汇编读
  `__stack_pointer`，CMake 之前没有把这份源码编进去。

## `ruby` —— Emscripten 上的 miniruby 是构建工具，不是目标程序

- 文件：`CMakeLists.txt`（miniruby 的链接选项 + 目录级 flag）、`cmake/RubyGenerators.cmake`、
  `wasm/miniruby_pre.js`
- 现象：交叉编译时 `builtin_binary.rbbin` 生成不出来——`$<TARGET_FILE:miniruby>` 在
  Emscripten 下是个 `.js`，不能直接执行。改用 node 跑之后连撞五个坑，最后一个尤其难查：

  1. `Aborted(OOM)`：默认 16 MiB 堆装不下解析 builtin Ruby 文件。
  2. `TypeError: Cannot read properties of undefined (reading 'node_ops')`：
     `NODERAWFS` 的 `fstat` 遇到 stdio 就崩（tty 没有后备 node），而 `ruby_sysinit`
     一上来就 `fstat(0/1/2)`。
  3. `getcwd` 返回 `ENOENT`：带盘符的路径（`J:/...`）Emscripten 的 C 库走不通。
  4. 加载了 `erb` 之后 `Aborted(Please compile your program with async support ...)`：
     保守 GC 用 `emscripten_scan_stack` / `emscripten_scan_registers` 扫栈和 wasm 局部变量，
     这两个都是 Asyncify 操作，miniruby 没开 `-sASYNCIFY`。
  5. **`RuntimeError: divide by zero`，栈顶停在 `heap_page_allocate_and_initialize`**：
     看起来像 GC 或分配器的 bug，实际是 `-sSTACK_SIZE` 还是默认的 64 KiB —— Prism 递归
     编译嵌套的 Ruby 源码时把栈走穿，wasm 的栈溢出**不 trap**，而是直接踩到堆上，
     之后在分配器里以除零的形式暴露（有时是 `memory access out of bounds`）。
     引擎那边早就设了 `-sSTACK_SIZE=16777216`，构建工具漏了。

- 补丁：
  - miniruby 加
    `-sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728
     -sSTACK_SIZE=16777216 -sASYNCIFY -sASYNCIFY_STACK_SIZE=8388608`
    和 `--pre-js=wasm/miniruby_pre.js`；
  - `miniruby_pre.js` 给三个标准流补一个字符设备 `fstat`，并让 `FS.cwd()` 去掉盘符
    （node 会把无盘符的路径解析到当前盘，落点不变）；
  - `RubyGenerators.cmake` 在 Emscripten 下用 `node` 启动 miniruby，并把交给它的路径
    去掉盘符（`ruby_host_tool_path()`）；
  - 整个 `third_party/ruby` 目录在 Emscripten 下去掉 `-fwasm-exceptions`，改加
    `-sSUPPORT_LONGJMP=wasm`。Asyncify 和 wasm 异常不能混用（emcc 会警告
    "Parts of the program that mix ASYNCIFY and exceptions will not compile"），
    而 Ruby 是 C、不跨引擎的 C++/Rust 边界抛异常；`-fwasm-exceptions` 原本隐含了
    wasm 版 setjmp，去掉后要显式要回来（`__wasm_longjmp` / `__c_longjmp` 只有它提供）。
- 注意：`--pre-js` 的内容变化不会让 ninja 重链 miniruby（它只在命令行里以路径出现），
  改完要删掉 `miniruby.js` 再编。




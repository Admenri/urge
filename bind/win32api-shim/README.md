# 非 Windows 平台的 Win32API 兼容层

该兼容层保留 RGSS 脚本的 `Win32API.new(library, function, import, export, calltype)`、`call` 和 `Call` 调用形式，用 URGE/SDL 或目标平台接口实现已注册的功能。它不加载 Windows DLL，也不模拟 Windows 进程、ABI 或原版 RGSS 的对象内存布局。

| 平台 | 当前范围 |
| --- | --- |
| Windows | 原生 DLL 调用 |
| macOS | 已启用兼容接口 |
| WASM / Web | 已启用，部分为空操作 |
| Linux / Android | 基础框架及空操作 |

下文按功能列出 macOS 和 WASM 的已注册接口及平台差异。非 Windows 平台的未知库、未注册函数统一降级为空操作。

## 调用约定

`[A]` 表示同时注册无后缀和 `A` 后缀名称；字符串按 UTF-8 处理，未实现 `W` 版本及完整 Windows ANSI 代码页语义。DLL 名忽略 ASCII 大小写、路径前缀和 `.dll` 后缀；例如 `kernel32`、`kernel32.dll` 和 `KERNEL32.DLL` 都解析到同一组接口。函数名保持大小写敏感，不匹配 `.exe` 等其他扩展名。这是兼容层内的名称匹配，不会查找或加载磁盘上的 DLL。

原版 CRuby 1.8 Win32API 将库名交给 `LoadLibrary`，后者在省略扩展名时默认补 `.dll`；参见 [CRuby 1.8 源码](https://github.com/ruby/ruby/blob/v1_8_7_374/ext/Win32API/Win32API.c) 和 [LoadLibrary 文档](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya)。

各平台均提供 `Win32API.supported?(library, function)`，检查原生导出或显式注册项：

- Windows 沿用原生构造函数的解析规则，通过 `LoadLibraryA` / `GetProcAddress` 探测 DLL 和导出；先匹配原名，再尝试追加 `A` 后缀。DLL 无法加载或函数不存在时返回 `false`，不因此抛出异常。探测结束释放本次加载的模块引用，并恢复调用前的 Win32 错误码。加载 DLL 可能执行其初始化代码，探测不会调用目标函数。
- 非 Windows 平台查询兼容层注册表；显式注册的 WASM 空操作返回 `true`，未注册的通用降级项返回 `false`。能力探测本身不打印警告，也不会注册新接口。返回 `true` 不保证给定参数、运行状态和平台权限下的调用一定成功。

非 Windows 绑定的参数签名支持字符串或单字符数组，接受 `N/L/I/P/V` 及其小写形式，并验证参数数量。可选调用约定不参与语义分发。函数对象不支持 `dup`/`clone`。

### 未实现 API 的降级与日志

未知 DLL、未注册函数和显式注册的平台空操作均可创建和调用。未注册函数使用脚本声明的导入签名确定参数数量；已注册项继续检查既定参数数量。签名格式、最多 32 个参数的限制和实际调用数量检查仍然有效。

空操作不转换参数值、不读取或写入缓冲区、不执行传入的 Ruby 回调或数值函数指针。数值及 `V` 返回类型得到 0，`P` 返回类型得到空字符串；调用将 `GetLastError` 设置为 120（`ERROR_CALL_NOT_IMPLEMENTED`）。例如：

```ruby
Win32API.supported?('SomePlugin.dll', 'Unavailable') # => false
fn = Win32API.new('SomePlugin.dll', 'Unavailable', 'LP', 'L')
fn.call(0, "buffer") # => 0；缓冲区保持原样
```

当前进程内首次成功创建某个空操作 API 时通过 URGE 日志打印一次 WARN，包含 DLL/API 名称和空操作提示。DLL 名按现有规则归一化后去重，函数名区分大小写；相同接口的不同实例、重复调用、线程及 Ruby VM 重置不会重复打印。日志中的控制字符会转义，过长名称会截断显示；完整名称仍用于去重。

此降级仅处理非 Windows 兼容层中的未实现接口，不捕获或吞掉已实现接口的错误。越界、释放后访问、无效宿主地址、冻结输出缓冲区和窗口接口的主线程限制继续执行原有检查。Windows 原生绑定继续保留 DLL 加载和函数查找失败时的错误行为。

## 内存操作

| 库 | 接口 |
| --- | --- |
| `msvcrt` / `ucrtbase` | `malloc`、`calloc`、`realloc`、`free` |
| `msvcrt` / `ucrtbase` | `memcpy`、`memmove`、`memset`、`memcmp`、`strlen`、`strcpy` |
| `kernel32` | `RtlMoveMemory`、`MoveMemory` |
| `kernel32` | `GlobalAlloc`、`GlobalReAlloc`、`GlobalFree`、`GlobalLock`、`GlobalUnlock`、`GlobalSize`、`GlobalFlags`、`GlobalHandle` |

### 内存保护机制

数值地址只可能是兼容层自己的句柄或虚拟堆地址，绝不会被直接转换为宿主指针。macOS 和 WASM 共用以下保护机制；WASM 线性内存中的任意地址同样不能直接访问。内存函数只能访问：

- 兼容层分配的、尚未释放的虚拟内存范围，允许经过范围检查的指针偏移。
- 当前调用明确传入的 Ruby 字符串缓冲区，访问长度不得超过其实际字节数。

虚拟堆以 32 位正数表示地址，与宿主 32/64 位 ABI 解耦。单次分配和内存操作上限为 32 MiB，总分配上限为 128 MiB、同时存活的分配最多 65,536 个；分配内存初始清零。分配失败返回 0，`realloc` 失败保留原块。地址在进程内不复用，32 位虚拟地址空间耗尽时也会返回分配失败；释放后访问、重复释放、释放内部偏移地址、越界访问和无终止符的堆字符串读取都会被拒绝。`memcpy` 拒绝重叠范围，重叠复制应使用 `memmove`。

读取 `Bitmap.object_id * 2 + offset` 等原版 RGSS 内部地址的脚本会得到可诊断的 Ruby 异常，而不是读取本 Runtime 的 C/C++ 对象。**这不等于支持旧 RGSS 的私有位图结构或截图存档格式。** 不安装进程级信号处理器，也不依赖捕获 segmentation fault 来恢复。

Ruby 绑定在借用缓冲区前完成参数转换和写权限检查，输出缓冲区遵循 Ruby 的冻结与写时复制规则。调用期间禁止同实例重入或重新初始化，并通过 `rb_ensure` 恢复状态，避免 `to_str`/`to_int` 回调替换仍被使用的 C++ 对象。

### 指针使用示例

分配得到的数值指针请使用 `L` 或 `I` 返回类型。对 Ruby 字符串目标缓冲区，CRT 函数返回值应声明为 `P`（返回有界字符串副本）或 `V`，不会泄露该字符串的宿主地址。需要二进制拷贝且不关心返回值时可使用 `RtlMoveMemory` 的 `V` 返回类型：

```ruby
alloc = Win32API.new('msvcrt', 'malloc', 'L', 'L')
free  = Win32API.new('msvcrt', 'free', 'L', 'V')
copy  = Win32API.new('kernel32', 'RtlMoveMemory', 'PPL', 'V')
pointer = alloc.call(16)
raise 'allocation failed' if pointer == 0
begin
  copy.call(pointer, "hello\0", 6)
  buffer = "\0" * 6
  copy.call(buffer, pointer, 6)
ensure
  free.call(pointer)
end
```

### 全局内存

`GlobalAlloc` 的固定内存（`GMEM_FIXED = 0`）返回虚拟地址；可移动内存（`GMEM_MOVEABLE = 2`）返回独立的虚拟 HGLOBAL，必须用 `GlobalLock` 取得地址。可移动句柄自身不能用于内存读写。`GlobalHandle` 只接受块首地址，`GlobalSize` 返回申请的字节数。

```ruby
alloc  = Win32API.new('kernel32.dll', 'GlobalAlloc', 'LL', 'L')
lock   = Win32API.new('kernel32.dll', 'GlobalLock', 'L', 'L')
unlock = Win32API.new('kernel32.dll', 'GlobalUnlock', 'L', 'L')
free   = Win32API.new('kernel32.dll', 'GlobalFree', 'L', 'L')
handle = alloc.call(0x42, 1024) # GMEM_MOVEABLE | GMEM_ZEROINIT
raise 'allocation failed' if handle == 0
begin
  pointer = lock.call(handle)
  raise 'lock failed' if pointer == 0
  begin
    # pointer 可传给 RtlMoveMemory 等经过范围检查的接口。
  ensure
    unlock.call(handle)
  end
ensure
  free.call(handle)
end
```

可移动内存维护锁计数，`GlobalFlags` 的低字节返回该计数。最后一次成功 `GlobalUnlock` 返回 0 并将 `GetLastError` 设为 0；未锁定时解锁返回 0 和 `ERROR_NOT_LOCKED`。`GlobalFree` 成功返回 0，失败返回传入的句柄；允许释放仍锁定的块。CRT 的 `free/realloc` 与全局内存不可混用。

支持零长度可移动对象的 discarded 状态：`GlobalLock` 返回 0，`GlobalFlags` 包含 `GMEM_DISCARDED`，之后可通过 `GlobalReAlloc` 分配实际内容。重分配保留已有内容，失败保留原对象；固定块或已锁定块增长超出保留范围时，需要显式提供 `GMEM_MOVEABLE` 允许重定位。可移动对象重定位后句柄保持不变，旧地址失效；固定对象重定位后须使用新返回值。缩小后保留的容量仍计入总内存限制，避免通过反复缩小绕过上限。

`GMEM_ZEROINIT` 受支持；出于安全，未要求清零时也会清零新内存。文档中已废弃的分配标志被忽略；未知标志及 `GMEM_MODIFY` 属性修改明确失败，不模拟进程间共享、剪贴板所有权或自动内存丢弃。锁计数最多 255。上述句柄、返回值和锁定语义参照 [GlobalAlloc](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalalloc)、[GlobalReAlloc](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalrealloc)、[GlobalUnlock](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalunlock) 和 [GlobalFree](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-globalfree)。

## 窗口操作

| 库 | 接口 |
| --- | --- |
| `user32` | `FindWindow[A]`、`FindWindowEx[A]`、`GetTopWindow`、`GetWindow` |
| `user32` | `GetForegroundWindow`、`GetActiveWindow`、`IsWindow`、`IsWindowVisible` |
| `user32` | `GetWindowThreadProcessId` |
| `user32` | `GetWindowRect`、`GetClientRect`、`ScreenToClient`、`ClientToScreen` |
| `user32` | `GetWindowText[A]`、`GetWindowTextLength[A]`、`SetWindowText[A]` |
| `user32` | `MoveWindow`、`SetWindowPos`、`ShowWindow`、`SetForegroundWindow` |
| `user32` | `GetSystemMetrics` |

只暴露 URGE 自己的窗口，使用稳定的虚拟 HWND。窗口接口要求主线程；不查找或操作其他应用的窗口。`POINT` 为两个、`RECT` 为四个小端 32 位有符号整数，输出可写入 Ruby 字符串或受保护内存；短缓冲区会被拒绝。

`GetClientRect` 返回 `(0, 0, width, height)`，不含标题栏和边框。`GetWindowText[A]` 的容量包含末尾 NUL，返回已写入的字节数（不含 NUL）；`GetWindowTextLength[A]` 返回 UTF-8 字节数。`SetWindowText[A]` 修改当前窗口标题，不持久化到 Game.ini。

### 窗口与进程关联

`GetWindowThreadProcessId(hwnd, pid_buffer)` 的返回值是创建窗口的**线程 ID**，进程 ID 写入可选的 4 字节小端 DWORD 缓冲区；传入 `nil` / 0 可省略 PID 输出。缓冲区也可使用受保护堆地址，继续执行冻结、长度和范围检查。无效 HWND 返回 0，设置错误码 1400，并保持输出内容不变。接口要求主线程。

反向查询采用遍历窗口、比较 PID 的方式。兼容层只暴露一个顶层游戏窗口，不提供其他应用、子窗口或 message-only 窗口：

- `GetTopWindow(0)` 返回游戏窗口；以游戏窗口为父窗口时返回 0。
- `GetWindow` 的 `GW_HWNDFIRST` / `GW_HWNDLAST` 返回自身；`GW_HWNDNEXT` / `GW_HWNDPREV` 返回 0，使遍历结束。`GW_OWNER` / `GW_CHILD` 返回 0；没有弹出窗口时，`GW_ENABLEDPOPUP` 返回自身。
- `FindWindowEx[A](0, 0, class_name, title)` 可查找当前顶层窗口；以它为 `child_after` 时返回 0，以它为父窗口也没有子窗口。未知句柄明确失败。`FindWindow[A]` 和 `FindWindowEx[A]` 的类名、标题匹配不区分 ASCII 大小写。
- `EnumWindows` / `EnumThreadWindows` 的原生回调尚未实现，会走通用空操作，不执行传入的函数指针。普通 Ruby 脚本可以使用下列遍历方式，不需要宿主指针或额外 DLL。

```ruby
current_pid = Win32API.new('kernel32.dll', 'GetCurrentProcessId', '', 'L')
top_window = Win32API.new('user32.dll', 'GetTopWindow', 'L', 'L')
get_window = Win32API.new('user32.dll', 'GetWindow', 'LL', 'L')
window_pid = Win32API.new('user32.dll', 'GetWindowThreadProcessId', 'LP', 'L')

pid = current_pid.call
hwnd = top_window.call(0)
while hwnd != 0
  output = "\0" * 4
  tid = window_pid.call(hwnd, output)
  break if tid != 0 && output.unpack('L<')[0] == pid
  hwnd = get_window.call(hwnd, 2) # GW_HWNDNEXT
end
# hwnd 为找到的窗口；未找到时为 0。
```

返回值及关系参数参见 [GetWindowThreadProcessId](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowthreadprocessid)、[GetTopWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-gettopwindow) 和 [GetWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindow)。

### macOS

桌面和客户区坐标使用 macOS 逻辑坐标，遵循系统缩放，不混入 Retina backing pixels。`GetSystemMetrics` 支持主显示器宽高、标题栏及窗口边框尺寸（索引 0、1、4、7、8、32、33）。`SetWindowPos` 支持移动、缩放、显示/隐藏、置顶/取消置顶等常见操作；不实现任意窗口间的 Z 序。

### WASM / Web

当前游戏 canvas 同时作为虚拟窗口和虚拟桌面：

- `GetWindowRect` 与 `GetClientRect` 均返回 `(0, 0, width, height)`。
- `ScreenToClient` / `ClientToScreen` 保持坐标不变。
- `GetSystemMetrics(0/1)` 返回 canvas 的逻辑宽高，标题栏和边框指标为 0；不表示浏览器窗口或显示器的物理位置、尺寸。
- `SetWindowText[A]` 更新 SDL 窗口标题和页面 `document.title`；`GetWindowText[A]` 从 SDL 读取 UTF-8 标题，沿用相同的长度、缓冲区和编码检查。
- `MoveWindow`、`SetWindowPos`、`ShowWindow` 和 `SetForegroundWindow` 为空操作，不改变窗口或 canvas 状态。

## 键鼠操作

| 库 | 接口 |
| --- | --- |
| `user32` | `GetCursorPos`、`SetCursorPos`、`ShowCursor` |
| `user32` | `GetKeyState`、`GetAsyncKeyState`、`GetKeyboardState` |

键鼠接口要求主线程，状态基于 SDL。按键查询限于本应用获得焦点时的状态，不提供系统级键盘监听。`GetAsyncKeyState` 的低位表示本兼容层轮询观察到的按下转换。

macOS 鼠标坐标使用系统逻辑坐标，遵循显示缩放。Ruby 构建补齐了 macOS pthread 栈地址/大小接口检测，工作线程可以正常启动，但在调用窗口或输入接口时会得到明确的主线程限制错误。

WASM 的 `GetCursorPos` 返回 SDL 最近记录的 canvas 逻辑坐标，沿用对 CSS 缩放和设备像素比的转换；不采集其他页面或系统桌面的鼠标位置。按键状态限于获得输入焦点的游戏 canvas，`ShowCursor` 操作其 SDL 鼠标可见状态；`SetCursorPos` 为空操作，不移动鼠标。

## 进程与线程

| 库 | 接口 |
| --- | --- |
| `kernel32` | `GetCurrentProcessId`、`GetCurrentThreadId` |
| `kernel32` | `GetCurrentProcess`、`GetCurrentThread`、`GetProcessId`、`GetThreadId` |
| `kernel32` | `SetPriorityClass` |

`GetCurrentProcessId` 返回当前运行时的 PID，与 Ruby `Process.pid` 一致：macOS 使用当前 URGE 进程的系统 PID，WASM 使用 Emscripten 的虚拟 PID，不表示浏览器进程的系统 PID。

`GetCurrentThreadId` 返回当前宿主线程稳定的正数兼容 ID，适配 Win32 的 32 位宽度，不暴露 `pthread_t` 等宿主值。线程退出后不复用其兼容 ID；Ruby VM 重置也不会重置计数。游戏窗口由主线程创建，因此主线程上的 `GetWindowThreadProcessId` 返回值与 `GetCurrentThreadId` 一致。WASM 当前为单线程构建。

`GetCurrentProcess` / `GetCurrentThread` 返回 -1 / -2 伪句柄；`GetProcessId` / `GetThreadId` 仅接受对应的当前进程/线程伪句柄，同时兼容其 32 位无符号表示。其他句柄返回 0 和错误码 6。伪句柄和 ID 应使用数值返回类型，不是可读写的内存地址；本功能不打开或管理其他进程。

macOS 的 `SetPriorityClass` 仅支持当前进程伪句柄，映射到系统 `setpriority`；提升优先级被系统拒绝时如实返回失败，不请求管理员权限。WASM 的 `SetPriorityClass` 为空操作，不改变进程优先级。

## 配置文件

| 库 | 接口 |
| --- | --- |
| `kernel32` | `GetPrivateProfileInt[A]`、`WritePrivateProfileString[A]` |

INI 的段名和键名不区分大小写。当前 Game.ini 的写入通过同一个 Config 解析器保存，避免被退出时的配置保存覆盖，并沿用原编码及转码失败保护，不改变已加载的引擎参数。其他配置文件通过 IOService 读写，遵循游戏的虚拟文件系统与写目录限制。

WASM 沿用相同的配置处理，但写入仅保留在当前页面运行时的内存文件系统中，页面重新载入后重置。本功能没有增加 IndexedDB/IDBFS 持久化，也不会修改 HTTP 服务器上的游戏文件。

## 其他

| 库 | 接口 |
| --- | --- |
| `kernel32` | `GetLastError`、`SetLastError` |

`GetLastError` / `SetLastError` 读取或设置当前线程的错误码；它们也在尚未启用其他函数族的 Linux、Android 框架中注册。

## 模块与平台扩展

本目录集中维护非 Windows 兼容层，通过自己的 `CMakeLists.txt` 加入 `urge-binding`，沿用 `InitWin32APIBinding()` 初始化入口。`bind/CMakeLists.txt` 仅在 `NOT WIN32` 时加入本模块。Windows 的 `bind/binding_win32api.cc`、LoadLibrary/GetProcAddress/libffi 调用链不受影响。

- `binding_win32api_shim.cc`：Ruby 参数封送、返回值和实例生命周期。
- `win32api_shim.{h,cc}`：与 Ruby/宿主 ABI 解耦的参数、结果、函数元数据及不可变注册表。
- `win32api_memory.{h,cc}`：跨平台的受保护虚拟堆及内存函数族。
- `win32api_process.cc`：当前进程/线程 ID 和伪句柄；窗口关联由 SDL 函数族使用。
- `win32api_profile.cc`：基于 URGE 配置与文件系统的 profile API。
- `win32api_sdl.cc`：基于 SDL 的窗口、坐标与输入函数族。
- `win32api_platform.cc`：平台注册入口；`win32api_shim_macos.cc` / `win32api_shim_wasm.cc` 分别选择对应平台支持的共享函数族，平台专有处理保留在各自入口中。

增加平台时，在平台入口选择可支持的共享函数族并补充系统专有处理；增加 API 时向注册表提供名称、参数数目、输出缓冲区位掩码和处理函数；平台专用空操作使用 `AddNoOp` 注册，沿用统一返回值和首次警告规则。处理函数不得在借用字符串指针期间调用 Ruby 或触发 Ruby GC；输出缓冲区必须正确声明为可写。所有数值内存地址均须通过 `win32api_memory` 检查，禁止直接解引用。

WASM 平台通过 `win32api_shim_wasm.cc` 注册上述函数族及空操作，不要求额外浏览器权限。构建仍使用仓库的 `emscripten-wasm32` preset，无需额外 Emscripten 链接参数。浏览器能力依据和限制可参阅 [Emscripten HTML5 API](https://emscripten.org/docs/api_reference/html5.h.html)。

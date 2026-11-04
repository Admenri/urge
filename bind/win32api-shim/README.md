# 非 Windows 平台的 Win32API 兼容层

该兼容层保留 RGSS 脚本的 `Win32API.new(library, function, import, export, calltype)`、`call` 和 `Call` 调用形式，用 URGE/SDL 或目标平台接口实现已注册的功能。它不加载 Windows DLL，也不模拟 Windows 进程、ABI 或原版 RGSS 的对象内存布局。

当前仅 macOS 注册完整的下述兼容接口。Windows 继续使用原有 LoadLibrary/GetProcAddress/libffi 实现；其他平台具备 Ruby 绑定和注册框架，但尚未启用这些平台功能。未注册的函数在创建对象时明确报错，不会统一返回 0 冒充成功。

## 已实现接口

| 库 | 接口 |
| --- | --- |
| `user32` | `FindWindow[A]`、`GetForegroundWindow`、`GetActiveWindow`、`IsWindow`、`IsWindowVisible` |
| `user32` | `GetWindowRect`、`GetClientRect`、`ScreenToClient`、`ClientToScreen`、`MoveWindow`、`SetWindowPos`、`GetSystemMetrics` |
| `user32` | `GetWindowText[A]`、`GetWindowTextLength[A]`、`SetWindowText[A]`、`ShowWindow`、`SetForegroundWindow` |
| `user32` | `GetCursorPos`、`SetCursorPos`、`ShowCursor`、`GetKeyState`、`GetAsyncKeyState`、`GetKeyboardState` |
| `kernel32` | `GetPrivateProfileInt[A]`、`WritePrivateProfileString[A]`、`SetPriorityClass`、`RtlMoveMemory`、`MoveMemory`、`GetLastError`、`SetLastError` |
| `kernel32` | `GlobalAlloc`、`GlobalReAlloc`、`GlobalFree`、`GlobalLock`、`GlobalUnlock`、`GlobalSize`、`GlobalFlags`、`GlobalHandle` |
| `msvcrt` / `ucrtbase` | `malloc`、`calloc`、`realloc`、`free`、`memcpy`、`memmove`、`memset`、`memcmp`、`strlen`、`strcpy` |

`[A]` 表示同时注册无后缀和 `A` 后缀名称，未实现 `W` 版本。DLL 名忽略 ASCII 大小写、路径前缀和 `.dll` 后缀；例如 `kernel32`、`kernel32.dll` 和 `KERNEL32.DLL` 都解析到同一组接口。函数名保持大小写敏感，不匹配 `.exe` 等其他扩展名。这是兼容层内的名称匹配，不会查找或加载磁盘上的 DLL。

原版 CRuby 1.8 Win32API 将库名交给 `LoadLibrary`，后者在省略扩展名时默认补 `.dll`；参见 [CRuby 1.8 源码](https://github.com/ruby/ruby/blob/v1_8_7_374/ext/Win32API/Win32API.c) 和 [LoadLibrary 文档](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibrarya)。

非 Windows 绑定还提供 `Win32API.supported?(library, function)`，可用于能力探测。参数签名支持字符串或单字符数组，接受 `N/L/I/P/V` 及其小写形式，并验证参数数量。可选调用约定不参与语义分发。函数对象不支持 `dup`/`clone`。

## 指针与内存安全

数值地址只可能是兼容层自己的句柄或虚拟堆地址，绝不会被直接转换为宿主指针。内存函数只能访问：

- 兼容层分配的、尚未释放的虚拟内存范围，允许经过范围检查的指针偏移。
- 当前调用明确传入的 Ruby 字符串缓冲区，访问长度不得超过其实际字节数。

虚拟堆以 32 位正数表示地址，与宿主 32/64 位 ABI 解耦。单次分配和内存操作上限为 32 MiB，总分配上限为 128 MiB、同时存活的分配最多 65,536 个；分配内存初始清零。分配失败返回 0，`realloc` 失败保留原块。地址在进程内不复用，32 位虚拟地址空间耗尽时也会返回分配失败；释放后访问、重复释放、释放内部偏移地址、越界访问和无终止符的堆字符串读取都会被拒绝。`memcpy` 拒绝重叠范围，重叠复制应使用 `memmove`。

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

读取 `Bitmap.object_id * 2 + offset` 等原版 RGSS 内部地址的脚本会得到可诊断的 Ruby 异常，而不是读取本 Runtime 的 C/C++ 对象。**这不等于支持旧 RGSS 的私有位图结构或截图存档格式。** 不安装进程级信号处理器，也不依赖捕获 segmentation fault 来恢复。

Ruby 绑定在借用缓冲区前完成参数转换和写权限检查，输出缓冲区遵循 Ruby 的冻结与写时复制规则。调用期间禁止同实例重入或重新初始化，并通过 `rb_ensure` 恢复状态，避免 `to_str`/`to_int` 回调替换仍被使用的 C++ 对象。

## 窗口、输入和配置语义

- 目前只暴露 URGE 自己的窗口，使用稳定的虚拟 HWND；不查找或操作其他应用的窗口。
- 桌面、客户区和鼠标坐标使用 macOS 逻辑坐标，遵循系统缩放。`POINT` 为两个、`RECT` 为四个小端 32 位有符号整数；不混入 Retina backing pixels。
- 窗口和输入接口要求主线程；同时补齐 Ruby 构建对 macOS pthread 栈地址/大小接口的检测，使工作线程可以正常启动并得到明确的调用错误。键盘/鼠标状态基于 SDL，按键查询限于本应用获得焦点时的状态，不提供系统级键盘监听。`GetAsyncKeyState` 的低位表示本兼容层轮询观察到的按下转换。
- `GetSystemMetrics` 当前支持主显示器宽高、标题栏及窗口边框尺寸（索引 0、1、4、7、8、32、33）。`SetWindowPos` 支持移动、缩放、显示/隐藏、置顶/取消置顶等常见操作；不实现任意窗口间的 Z 序。
- `GetClientRect` 返回 `(0, 0, width, height)`，不含标题栏和边框。`GetWindowText[A]` 的容量包含末尾 NUL，返回已写入的字节数（不含 NUL）；`GetWindowTextLength[A]` 返回 UTF-8 字节数。输出可写入 Ruby 字符串或受保护内存，短缓冲区会被拒绝；`SetWindowText[A]` 接受 UTF-8 标题，修改的只是当前窗口，不持久化到 Game.ini。
- `A` 接口的字符串在此兼容层按 UTF-8 处理；`W` 接口及完整 Windows ANSI 代码页语义尚未实现。
- INI 的段名和键名不区分大小写。当前 Game.ini 的写入通过同一个 Config 解析器持久化，避免被退出时的配置保存覆盖，并沿用原编码及转码失败保护，不改变已加载的引擎参数。其他配置文件通过 IOService 读写，遵循游戏的虚拟文件系统与写目录限制。
- `SetPriorityClass` 仅支持当前进程伪句柄，映射到 macOS `setpriority`；提升优先级被系统拒绝时如实返回失败，不请求管理员权限。

## 模块与平台扩展

本目录集中维护非 Windows 兼容层，通过自己的 `CMakeLists.txt` 加入 `urge-binding`，沿用 `InitWin32APIBinding()` 初始化入口。`bind/CMakeLists.txt` 仅在 `NOT WIN32` 时加入本模块。Windows 的 `bind/binding_win32api.cc`、LoadLibrary/GetProcAddress/libffi 调用链不受影响。

- `binding_win32api_shim.cc`：Ruby 参数封送、返回值和实例生命周期。
- `win32api_shim.{h,cc}`：与 Ruby/宿主 ABI 解耦的参数、结果、函数元数据及不可变注册表。
- `win32api_memory.{h,cc}`：跨平台的受保护虚拟堆及内存函数族。
- `win32api_profile.cc`：基于 URGE 配置与文件系统的 profile API。
- `win32api_sdl.cc`：基于 SDL 的窗口、坐标与输入函数族。
- `win32api_platform.cc`：平台注册入口；`win32api_shim_macos.cc` 选择共享函数族，并实现 macOS 特有操作。

增加平台时，在平台入口选择可支持的共享函数族并补充系统专有处理；增加 API 时向注册表提供名称、参数数目、输出缓冲区位掩码和处理函数。处理函数不得在借用字符串指针期间调用 Ruby 或触发 Ruby GC；输出缓冲区必须正确声明为可写。所有数值内存地址均须通过 `win32api_memory` 检查，禁止直接解引用。


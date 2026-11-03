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
| `msvcrt` / `ucrtbase` | `malloc`、`calloc`、`realloc`、`free`、`memcpy`、`memmove`、`memset`、`memcmp`、`strlen`、`strcpy` |

`[A]` 表示同时注册无后缀和 `A` 后缀名称，未实现 `W` 版本。DLL 名忽略 ASCII 大小写、路径前缀和 `.dll` 后缀；函数名保持大小写敏感。

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

读取 `Bitmap.object_id * 2 + offset` 等原版 RGSS 内部地址的脚本会得到可诊断的 Ruby 异常，而不是读取本 Runtime 的 C/C++ 对象。**这不等于支持旧 RGSS 的私有位图结构或截图存档格式。** 不安装进程级信号处理器，也不依赖捕获 segmentation fault 来恢复。

Ruby 绑定在借用缓冲区前完成参数转换和写权限检查，输出缓冲区遵循 Ruby 的冻结与写时复制规则。调用期间禁止同实例重入或重新初始化，并通过 `rb_ensure` 恢复状态，避免 `to_str`/`to_int` 回调替换仍被使用的 C++ 对象。

## 窗口、输入和配置语义

- 目前只暴露 URGE 自己的窗口，使用稳定的虚拟 HWND；不查找或操作其他应用的窗口。
- 桌面、客户区和鼠标坐标使用 macOS 逻辑坐标，遵循系统缩放。`POINT` 为两个、`RECT` 为四个小端 32 位有符号整数；不混入 Retina backing pixels。
- 窗口和输入接口要求主线程；同时补齐 Ruby 构建对 macOS pthread 栈地址/大小接口的检测，使工作线程可以正常启动并得到明确的调用错误。键盘/鼠标状态基于 SDL，按键查询限于本应用获得焦点时的状态，不提供系统级键盘监听。`GetAsyncKeyState` 的低位表示本兼容层轮询观察到的按下转换。
- `GetSystemMetrics` 当前支持主显示器宽高、标题栏及窗口边框尺寸（索引 0、1、4、7、8、32、33）。`SetWindowPos` 支持移动、缩放、显示/隐藏、置顶/取消置顶等常见操作；不实现任意窗口间的 Z 序。
- `A` 接口的字符串在此兼容层按 UTF-8 处理；`W` 接口及完整 Windows ANSI 代码页语义尚未实现。
- INI 的段名和键名不区分大小写。当前 Game.ini 的写入通过同一个 Config 解析器持久化，避免被退出时的配置保存覆盖，并沿用原编码及转码失败保护，不改变已加载的引擎参数。其他配置文件通过 IOService 读写，遵循游戏的虚拟文件系统与写目录限制。
- `SetPriorityClass` 仅支持当前进程伪句柄，映射到 macOS `setpriority`；提升优先级被系统拒绝时如实返回失败，不请求管理员权限。

## 平台扩展

- `binding_win32api_shim.cc`：Ruby 参数封送、返回值和实例生命周期。
- `win32api_shim.{h,cc}`：与 Ruby/宿主 ABI 解耦的参数、结果、函数元数据及不可变注册表。
- `win32api_memory.{h,cc}`：跨平台的受保护虚拟堆及内存函数族。
- `win32api_profile.cc`：基于 URGE 配置与文件系统的 profile API。
- `win32api_sdl.cc`：基于 SDL 的窗口、坐标与输入函数族。
- `win32api_platform.cc`：平台注册入口；`win32api_shim_macos.cc` 选择共享函数族，并实现 macOS 特有操作。

增加平台时，在平台入口选择可支持的共享函数族并补充系统专有处理；增加 API 时向注册表提供名称、参数数目、输出缓冲区位掩码和处理函数。处理函数不得在借用字符串指针期间调用 Ruby 或触发 Ruby GC；输出缓冲区必须正确声明为可写。所有数值内存地址均须通过 `win32api_memory` 检查，禁止直接解引用。

## 旧 RGSS 配置类前向声明

此项为独立的 Ruby 脚本兼容功能，仅在特定继承冲突发生后尝试恢复；不再对正常脚本预处理。实现范围与限制见 [配置类兼容说明](LEGACY_CLASSES.md)。

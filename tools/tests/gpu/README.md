# GPU 安全性与 Ruby 导出回归

测试使用实际 `Game.exe`，为 RGSS1/2/3 分别生成独立临时游戏，不依赖外部 Ruby、测试专用引擎入口或公网。

```sh
cmake --build --preset win64-vendored-ruby --target Game
python -I tools/tests/gpu/run.py --engine build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe
```

可用 `--rgss 3` 单独运行，`--backend d3d12` 更换图形后端（默认 `opengl`）。每个进程默认限时 60 秒；日志和逐项结果保存在输出打印的 `build/gpu-integration/run-*` 目录。断言失败、进程退出或未实际触发验证错误回调均返回非零状态。

## 覆盖内容

- uint32 属性的完整范围、负数/溢出拒绝，以及只读属性不导出 setter。
- 顶点槽、属性位置、颜色槽与描述符列表的设备限额；包括 `0xffffffff` 溢出输入。
- 未显式 submit 的队列写入可通过同步读回看到最新数据。
- 二进制读回为 ASCII-8BIT；全范围哨兵转为实际剩余长度。
- 显式映射复用、重复读取、部分映射、取消映射与销毁后的缓存失效。
- 创建时映射不被 Read 再次映射；错误映射模式、usage、范围和对齐可通过 Ruby `RGSSError` 处理。
- 普通 WebGPU 验证错误记录到日志，而不是使用原生后端会 panic 的默认处理器。

## 刻意跳过

按当前后端替换计划，不测试成功的 `GPUBuffer#write`，也不调用 `map_state` getter：当前 wgpu-native 的 `WriteMappedRange` / `GetMapState` 入口尚未实现。回归仅测试 Write 在进入该入口之前的范围/只读映射错误，并检查 `map_state` 只读导出是否正确。这不表示两个入口已经可用。

运行器验证 Windows 原生后端；WebAssembly 可用构建目标 `urge-core` / `urge-binding` 检查编译，但不能代替浏览器实测。

## 当前 API 约定

- `map_async`、`read`、`write` 的越界、错误模式、对齐错误及已销毁对象使用 `RGSSError` 报错。
- 映射偏移按 8 字节对齐，长度按 4 字节对齐。
- `0xffffffffffffffff` 在 MapAsync/Read 中表示从偏移开始的剩余 buffer 长度，必须仍处于合法/可寻址范围。
- `read` 会复用用户显式建立的映射，不替用户取消它；自行建立的临时映射在读取后取消。
- 同步映射提交此前 staged queue writes，避免原生读回旧数据。
- 无符号 ATTR 使用真实 C++ 类型导出；例如未指定 mip 数量的 sentinel 是 `0xffffffff`，不再使用错误的有符号 `-1`。只读属性没有赋值方法。
- 回调可能运行在 GPU 工作线程，不能直接调用 Ruby 的异常 API。普通 uncaptured validation 错误记录日志；映射输入错误和回调失败通过方法边界转换为 Ruby 异常。

# Ruby 网络与常量集成测试

测试使用实际 `Game.exe` 的 CRuby 绑定，在独立临时游戏目录中运行脚本。HTTP / WebSocket 服务只监听 `127.0.0.1` 的随机端口，不依赖公网、外部 Ruby 或第三方 Python 包，不修改已有游戏。

## 运行

在仓库根目录：

```sh
cmake --build --preset win64-vendored-ruby --target Game
python -I tools/tests/test_binding_constants.py
python -I tools/tests/test_network_bindings.py
python -I tools/tests/network/run.py --engine build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe
```

默认逐一运行 RGSS1、RGSS2、RGSS3。可用 `--rgss 3` 单独运行，或用 `--backend d3d12` 更换图形后端（默认 `opengl`）。每个子进程默认限时 60 秒，支持 `--timeout`。运行失败返回非零退出码；日志、Ruby 逐项结果与临时游戏保存在输出打印的 `build/network-integration/run-*` 目录中。

## 全二进制正文 API

这是不保留旧接口的重新设计。正文与消息载荷一律使用 Ruby ASCII-8BIT（`Encoding::BINARY`）字符串，不做字符集推断、解码、替换或转码。

| API | 行为 |
| --- | --- |
| `WebSocket#send(data)` | 接受字符串的原始字节，始终发送二进制 WebSocket 帧；忽略字符串的 Ruby encoding 标签。它覆盖了继承的 `Object#send`，不是反射调用入口。 |
| `NetworkEvent#data` | 返回消息的原始字节；服务器发送文本帧或二进制帧都使用相同接口。非 MESSAGE 事件返回空的 ASCII-8BIT 字符串。 |
| `Fetch#body` | 返回 HTTP 响应的原始正文，包括非 2xx 响应和空正文，不按 Content-Type 自动转码。 |
| `Network.fetch(url, method, body, headers)` / `Fetch.new(...)` | 请求正文也按原始字节发送，不按参数字符串的 encoding 转码。 |

`NetworkEvent` 类型仅有 `OPEN = 0`、`MESSAGE = 1`、`CLOSE = 2`、`ERROR = 3`。文本帧与二进制帧不再暴露不同的消息类型。Web 后端只移除 Emscripten 为文本回调额外添加的结尾 NUL，消息本身的 NUL 和 BOM 仍保留。

**已移除：**`WebSocket#send_text`、`#send_binary`、`NetworkEvent#text`、`Fetch#text`，以及 `NetworkEvent::TEXT` / `BINARY`。没有别名或兼容模式。`data` / `body` 通过头文件的 `Binary: true` 标记生成绑定，冷生成和增量生成使用同一套二进制编码规则。

URL、HTTP 方法、请求/响应头、状态文本、错误消息和关闭原因属于协议元信息，仍为可读文本；关闭原因受 WebSocket 协议的 UTF-8 规则约束，而不是消息正文。

```ruby
fetch = Network.fetch("http://127.0.0.1:8080/data") # Fetch.new(url) 也可用
fetch.start                                     # 只启动一次
until fetch.done?
  Network.update
  Graphics.update
end
bytes = fetch.body                              # ASCII-8BIT 原始字节

socket = Network.websocket("ws://127.0.0.1:8080/echo")
loop do
  Network.update
  while (event = socket.next_event)
    case event.type
    when NetworkEvent::OPEN
      socket.send("hello\0world")                 # 二进制帧，不会截断 NUL
    when NetworkEvent::MESSAGE
      bytes = event.data                         # ASCII-8BIT，无自动解码
      socket.close(1000, "finished")
    when NetworkEvent::ERROR
      warn event.message
    end
  end
  break if socket.ready_state == WebSocket::CLOSED
  Graphics.update
end
```

需要文本时由应用协议决定编码；例如显式验证 UTF-8：

```ruby
text = fetch.body.force_encoding('UTF-8')
raise 'invalid UTF-8' unless text.valid_encoding?
# 转换其他编码时，例如：fetch.body.force_encoding('GB18030').encode('UTF-8')
```

`body` / `data` 每次返回独立的字符串，调用者修改返回值不会改写 Fetch 响应或已有事件。

`Network.timeout` 单位为秒，在对象创建时读取。Fetch 限制整个请求，WebSocket 限制连接握手。`abort` 使尚未完成的 Fetch 进入终态：`done?` 为真、`success?` 为假并提供错误信息；之后的 `start` 不会重启它。收到 HTTP 非 2xx 响应时，`success?` 为假，但保留正文、响应头和状态文本，`error` 仅用于传输错误。

## 覆盖范围

- Fetch：GET / POST / 非 2xx / chunked 响应的任意字节（全部 0–255 值、内嵌 NUL、无效 UTF-8）、空正文、ASCII-8BIT 字节语义、返回值独立性、HEAD、请求/响应头、重定向最终响应头、状态文本、非法协议、进度、重复 start、abort、请求超时和 `Fetch.new`。
- WebSocket：始终以二进制帧发送、不同 Ruby encoding 下的原始字节、空消息、BOM / NUL / UTF-8 字节、文本与二进制入站帧统一为 MESSAGE、125 / 126 / 65535 / 65536 字节边界、大消息、文本/二进制分片及交错 Ping/Pong、关闭码/原因/状态、握手失败及超时、事件队列限额和 `WebSocket.new`。
- 常量：验证所有 API IR 导出常量存在且为整数，并用独立清单/WebGPU C 头定义逐项比对精确值。
- 生成器：验证 binary 标记、旧手写 getter 向生成 getter 的迁移、冷/增量生成、幂等性、常量作用域、命名覆盖、哨兵过滤、重复常量检测及整数位宽。

## 浏览器二进制回归

```sh
python -I tools/tests/network/run_web.py
# 如未自动找到工具：
python -I tools/tests/network/run_web.py --emxx /path/to/em++ --browser /path/to/chromium
```

运行器直接编译实际 `network.cc` / `network_web.cc`，在独立临时 Chrome / Chromium / Edge 无头配置中运行；不需要完整引擎构建或 Python 第三方包。编译器从 PATH 或 `EMSDK` 查找，浏览器支持 PATH 和 Windows 常用安装位置。只访问随机端口的 loopback fixture，不改用户浏览器配置或 API JSON。

结果与浏览器日志保存在 `build/network-web-integration/run-*`，失败或超时返回非零。该探针验证真实 Web 后端和浏览器传输；Ruby 字符串编码由上述原生引擎测试验证，不把探针当作 Web 版完整 Ruby 引擎测试。

原生后端支持 `send_ping` 的二进制控制载荷。浏览器 WebSocket API 不开放 Ping/Pong 控制帧，因此 Web 后端的 `send_ping` 是空操作（浏览器自动响应服务器 Ping）；不要将原生测试结果视为浏览器 Ping 发送支持。测试不覆盖 HTTPS/WSS 证书校验。

兼容性注意：`GPU::TEXTURE_COPY_SRC` / `TEXTURE_COPY_DST` 已修正为 WebGPU 定义的 `1` / `2`（旧错误值为 `32` / `64`）。新增 GPU 常量不表示设备自动支持或启用了对应可选特性。

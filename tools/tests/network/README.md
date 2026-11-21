# Ruby 网络与常量集成测试

测试使用实际 `Game.exe` 的 CRuby 绑定，在独立临时游戏目录中运行脚本。HTTP / WebSocket 服务只监听 `127.0.0.1` 的随机端口，不依赖公网、外部 Ruby 或第三方 Python 包，不修改已有游戏。

## 运行

在仓库根目录：

```sh
cmake --build --preset win64-vendored-ruby --target Game
python -I tools/tests/test_binding_constants.py
python -I tools/tests/network/run.py --engine build/win64-vendored-ruby/app/RelWithDebInfo/Game.exe
```

默认逐一运行 RGSS1、RGSS2、RGSS3。可用 `--rgss 3` 单独运行，或用 `--backend d3d12` 更换图形后端（默认 `opengl`）。每个子进程默认限时 60 秒，支持 `--timeout`。运行失败返回非零退出码；日志、Ruby 逐项结果与临时游戏保存在输出打印的 `build/network-integration/run-*` 目录中。

## 覆盖范围

- Fetch：GET、POST 二进制正文、HEAD、请求/响应头、重定向最终响应头、非 2xx 正文与状态文本、非法协议、进度、重复 start、启动前与传输中 abort、请求超时，以及 `Fetch.new`。
- WebSocket：子协议握手、UTF-8 文本、空文本/二进制消息、大二进制帧、分片与交错 Ping/Pong、关闭码/原因/状态、握手失败及超时、事件队列限额，以及 `WebSocket.new`。
- 常量：验证 API IR 中所有导出常量存在且为整数，并用独立清单/WebGPU C 头定义逐项对比当前 373 个常量的精确值。
- 生成器回归单独验证冷生成、增量生成、作用域、命名覆盖、哨兵过滤、重复常量检测、整数位宽及幂等性。

## API 行为

```ruby
fetch = Network.fetch("http://127.0.0.1:8080/data") # Fetch.new(url) 也可用
fetch.start                                   # 只启动一次
until fetch.done?
  Network.update
  Graphics.update
end
puts fetch.status
puts fetch.text
bytes = fetch.body                            # ASCII-8BIT 字符串

socket = Network.websocket("ws://127.0.0.1:8080/echo")
loop do
  Network.update
  while (event = socket.next_event)
    case event.type
    when NetworkEvent::OPEN
      socket.send_text("hello")
    when NetworkEvent::TEXT
      puts event.text
      socket.close(1000, "finished")
    when NetworkEvent::ERROR
      warn event.message
    end
  end
  break if socket.ready_state == WebSocket::CLOSED
  Graphics.update
end
```

`Network.timeout` 单位为秒，在对象创建时读取，Fetch 限制整个请求，WebSocket 限制连接握手。`abort` 使尚未完成的 Fetch 进入终态，`done?` 为真、`success?` 为假并提供错误信息；之后的 `start` 不会重启它。收到 HTTP 非 2xx 响应时，`success?` 为假，但保留正文、响应头和状态文本，`error` 仅用于传输错误。

原生后端支持 `send_ping`。浏览器 WebSocket API 不开放 Ping/Pong 控制帧，因此 Web 后端的 `send_ping` 仍是空操作；不要将该原生测试结果视为浏览器 Ping 支持。该运行器验证 Windows 原生后端，不替代浏览器实测，也不覆盖 HTTPS/WSS 证书校验。

兼容性注意：`GPU::TEXTURE_COPY_SRC` / `TEXTURE_COPY_DST` 已修正为 WebGPU 定义的 `1` / `2`（旧错误值为 `32` / `64`）。新增 GPU 常量不表示设备自动支持或启用了对应可选特性。

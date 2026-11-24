# Exercise the public Ruby API in the actual engine, not a test-only binding.
BASE = 'http://127.0.0.1:__PORT__'
WS_BASE = 'ws://127.0.0.1:__PORT__'
$results = []
$sockets = []

def assert(value, message)
  raise message unless value
end

def check(name)
  yield
  $results << "PASS #{name}"
rescue StandardError => error
  $results << "FAIL #{name}: #{error.class}: #{error.message}"
ensure
  File.write('network-results.txt', $results.join("\n") + "\n")
end

def wait_until(seconds = 2)
  deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + seconds
  until yield
    raise 'wait timed out' if Process.clock_gettime(Process::CLOCK_MONOTONIC) >= deadline
    Network.update
    Graphics.update
  end
end

def request(path, method = 'GET', body = '', headers = [])
  fetch = Network.fetch(BASE + path, method, body, headers)
  assert(!fetch.done?, 'fetch should be idle before start')
  fetch.start
  wait_until { fetch.done? }
  fetch
end

def socket(path = '/echo', direct = false)
  ws = direct ? WebSocket.new(WS_BASE + path, ['urge-test']) : Network.websocket(WS_BASE + path, ['urge-test'])
  $sockets << ws
  wait_until { ws.ready_state != WebSocket::CONNECTING }
  assert(ws.ready_state == WebSocket::OPEN, "open failed: #{ws.error}")
  event = next_event(ws)
  assert(event.type == NetworkEvent::OPEN, "first event is #{event.type}")
  ws
end

def next_event(ws, seconds = 2)
  event = nil
  wait_until(seconds) { event = ws.next_event }
  event
end

check('all exported Ruby constants') do
  names = File.readlines('constants.txt', chomp: true).reject(&:empty?)
  assert(!names.empty?, 'no constants were extracted from the API reference')
  names.each do |name|
    value = name.split('::').inject(Object) { |owner, part| owner.const_get(part, false) }
    assert(value.is_a?(Integer), "#{name} is not an Integer")
  end
end

check('all Ruby constant values match native definitions') do
__CONSTANT_CHECKS__
end

check('GET body is raw bytes, with headers and idempotent start') do
  fetch = request('/get')
  assert(fetch.success? && fetch.status == 200 && fetch.status_text == 'OK', 'GET status')
  assert(fetch.body == 'fetch 中文'.b && fetch.body.encoding == Encoding::BINARY, 'binary body')
  assert(fetch.header('x-TEST') == 'final' && fetch.header('missing') == '', 'header lookup')
  assert(fetch.progress == 1.0 && fetch.error.empty?, 'completion state')
  fetch.start
  assert(fetch.done? && fetch.body == 'fetch 中文'.b, 'start must be idempotent')
end

check('public payload API is binary-only, with no legacy accessors') do
  assert(!Fetch.instance_methods(false).include?(:text), 'legacy Fetch#text remains')
  assert(!NetworkEvent.instance_methods(false).include?(:text), 'legacy event#text remains')
  assert(!WebSocket.instance_methods(false).include?(:send_text), 'legacy send_text remains')
  assert(!WebSocket.instance_methods(false).include?(:send_binary), 'legacy send_binary remains')
  assert(WebSocket.instance_method(:send).owner == WebSocket, 'send is only inherited Object#send')
  assert(!NetworkEvent.const_defined?(:TEXT, false) && !NetworkEvent.const_defined?(:BINARY, false), 'legacy event constants remain')
  event = NetworkEvent.new
  assert(event.data.empty? && event.data.encoding == Encoding::BINARY, 'non-message data must be empty binary')
  begin
    event.data(1)
    raise 'event.data accepted an argument'
  rescue ArgumentError
  end
  fetch = Fetch.new(BASE + '/get')
  begin
    fetch.body(1)
    raise 'fetch.body accepted an argument'
  rescue ArgumentError
  end
end

check('POST body ignores Ruby encoding and preserves request headers') do
  body = (0..255).to_a.pack('C*') * 1024
  [body, body.dup.force_encoding('UTF-8'), body.dup.force_encoding('UTF-16LE'), ''.b].each do |payload|
    fetch = request('/echo', 'POST', payload, ['X-Request: fixture', 'Content-Type: application/octet-stream'])
    assert(fetch.success? && fetch.status == 201, 'POST status')
    assert(fetch.body == payload.b && fetch.body.encoding == Encoding::BINARY, 'POST binary echo')
    assert(fetch.header('x-request') == 'fixture', 'POST header')
  end
end

check('GET and HTTP error preserve all bytes without text decoding') do
  expected = (0..255).to_a.pack('C*') * 1024
  ['/binary', '/binary-status', '/binary-chunked'].each do |path|
    fetch = request(path)
    status = path == '/binary-status' ? 418 : 200
    assert(fetch.status == status && fetch.success? == (status == 200), "#{path}: status")
    assert(fetch.body == expected && fetch.body.encoding == Encoding::BINARY, "#{path}: binary body")
    assert(fetch.body.length == expected.bytesize && fetch.body.valid_encoding?, "#{path}: byte-string semantics")
    copy = fetch.body
    copy.setbyte(0, 255)
    assert(fetch.body == expected, "#{path}: mutating a returned body changed the response")
  end
end

check('empty Fetch body remains binary') do
  fetch = request('/empty')
  assert(fetch.success? && fetch.body.empty? && fetch.body.encoding == Encoding::BINARY, 'empty binary response')
end

check('redirect exposes final response headers') do
  fetch = request('/redirect')
  assert(fetch.status == 200 && fetch.header('X-Test') == 'final', "stale redirect header: #{fetch.headers.inspect}")
end

check('HEAD completes with empty body') do
  fetch = request('/get', 'HEAD')
  assert(fetch.success? && fetch.body.empty? && fetch.header('X-Test') == 'head', 'HEAD response')
end

check('HTTP failure keeps response body, headers and status text') do
  fetch = request('/status')
  assert(!fetch.success? && fetch.status == 418 && fetch.body == 'teapot'.b, 'HTTP failure state')
  assert(fetch.status_text == 'Fixture Status' && fetch.header('X-Test') == 'error-response', 'HTTP status text/headers')
  assert(fetch.error.empty?, 'HTTP response is not a transport failure')
end

check('Fetch.new creates a functional request') do
  fetch = Fetch.new(BASE + '/get')
  fetch.start
  wait_until { fetch.done? }
  assert(fetch.success?, 'direct fetch failed')
end

check('abort before start is terminal') do
  fetch = Network.fetch(BASE + '/slow')
  fetch.abort
  assert(fetch.done? && !fetch.success? && !fetch.error.empty?, 'abort must finish an idle fetch')
  fetch.start
  assert(fetch.done? && !fetch.success?, 'start must not resurrect an aborted request')
end

check('abort cancels an in-flight request promptly') do
  fetch = Network.fetch(BASE + '/slow')
  fetch.start
  3.times { Graphics.update }
  started = Process.clock_gettime(Process::CLOCK_MONOTONIC)
  fetch.abort
  elapsed = Process.clock_gettime(Process::CLOCK_MONOTONIC) - started
  assert(elapsed < 1.0, "abort blocked for #{elapsed.round(2)} seconds")
  assert(fetch.done? && !fetch.success? && !fetch.error.empty?, 'aborted fetch never completes')
end

check('configured timeout terminates stalled requests') do
  Network.timeout = 1
  fetch = Network.fetch(BASE + '/slow')
  fetch.start
  wait_until(2.5) { fetch.done? }
  assert(!fetch.success? && !fetch.error.empty?, 'timeout must fail')
ensure
  Network.timeout = 30
end

check('transport failures are terminal') do
  fetch = Network.fetch('urge-invalid://127.0.0.1/')
  fetch.start
  wait_until { fetch.done? }
  assert(!fetch.success? && fetch.status == 0 && !fetch.error.empty?, 'transport error state')
end

check('progress and body getters refresh without done polling') do
  fetch = Network.fetch(BASE + '/progress')
  fetch.start
  wait_until { fetch.progress > 0.0 }
  assert(fetch.progress < 1.0, 'progress skipped the in-flight state')
  wait_until { fetch.body == 'halfdone'.b }
  assert(fetch.success?, 'body getter did not publish completion')
end

check('WebSocket handshake observes configured timeout') do
  Network.timeout = 1
  ws = Network.websocket(WS_BASE + '/ws-slow')
  $sockets << ws
  event = next_event(ws, 2.5)
  assert(event.type == NetworkEvent::ERROR && !event.message.empty?, 'handshake timeout message')
  assert(ws.ready_state == WebSocket::CLOSED, 'handshake timeout state')
ensure
  Network.timeout = 30
end

check('WebSocket binary send, ping and close') do
  ws = socket('/binary-only')
  ws.send('echo 中文')
  event = next_event(ws)
  assert(event.type == NetworkEvent::MESSAGE && event.data == 'echo 中文'.b, 'binary echo')
  assert(event.data.encoding == Encoding::BINARY, 'message encoding')
  ws.send_ping("client\x00\xffping".b)
  event = next_event(ws)
  assert(event.type == NetworkEvent::MESSAGE && event.data == 'client-ping-seen'.b, "unexpected ping event: #{event.type}")
  ws.close(1000, 'finished')
  assert(ws.ready_state == WebSocket::CLOSING || ws.ready_state == WebSocket::CLOSED, 'close does not update state')
  event = next_event(ws)
  assert(event.type == NetworkEvent::CLOSE && event.code == 1000 && event.reason == 'finished', 'close event')
  assert(ws.ready_state == WebSocket::CLOSED, 'closed state')
end

check('empty WebSocket messages remain binary for any Ruby encoding') do
  ws = socket('/binary-only')
  ['', ''.b].each do |value|
    ws.send(value)
    event = next_event(ws)
    assert(event.type == NetworkEvent::MESSAGE && event.data.empty?, 'empty message')
    assert(event.data.encoding == Encoding::BINARY, 'empty message encoding')
  end
  ws.close
end

check('WebSocket sends raw bytes without Ruby encoding conversion') do
  ws = socket('/binary-only')
  text = [0xFEFF].pack('U') + "echo\x00中文\x00"
  [text, text.b, text.b.force_encoding('UTF-16LE')].each do |value|
    ws.send(value)
    event = next_event(ws)
    assert(event.type == NetworkEvent::MESSAGE && event.data == text.b, 'NUL/BOM bytes changed')
    assert(event.data.encoding == Encoding::BINARY, 'message encoding')
    copy = event.data
    copy.clear
    assert(event.data == text.b, 'mutating returned data changed the event')
  end
  [125, 126, 65535, 65536].each do |size|
    data = (0..255).to_a.pack('C*') * ((size + 255) / 256)
    data = data.byteslice(0, size)
    ws.send(data)
    event = next_event(ws)
    assert(event.type == NetworkEvent::MESSAGE && event.data == data, "frame length boundary #{size}")
  end
  ws.close
end

check('wire text and binary frames share the same raw message API') do
  ws = socket('/mixed')
  expected = ([0xFEFF].pack('U') + "echo\x00中文\x00").b
  [expected, expected, ''.b, ''.b].each do |bytes|
    event = next_event(ws)
    assert(event.type == NetworkEvent::MESSAGE && event.data == bytes, 'wire type leaked into message API')
    assert(event.data.encoding == Encoding::BINARY, 'wire text was decoded or tagged UTF-8')
  end
  ws.close
end

check('fragmented binary with an interleaved ping') do
  ws = socket('/binary-fragments')
  event = next_event(ws)
  expected = (0..255).to_a.pack('C*') * 1024
  assert(event.type == NetworkEvent::MESSAGE && event.data == expected, 'fragmented binary bytes')
  assert(event.data.encoding == Encoding::BINARY, 'fragmented binary encoding')
  event = next_event(ws)
  assert(event.type == NetworkEvent::MESSAGE && event.data == 'server-pong-seen'.b, 'binary server ping not answered')
  ws.close
end

check('large binary WebSocket frame') do
  ws = socket
  data = ("\x00\xfflarge\x00".b * 25000)
  ws.send(data)
  event = next_event(ws)
  assert(event.type == NetworkEvent::MESSAGE && event.data == data, "binary frame truncated: #{event.data.bytesize}")
  assert(event.data.encoding == Encoding::BINARY, 'binary event encoding')
  ws.close
end

check('fragmented text with an interleaved ping') do
  ws = socket('/fragments')
  event = next_event(ws)
  assert(event.type == NetworkEvent::MESSAGE && event.data == 'fragment-中文'.b, "fragmented message: #{event.data.inspect}")
  assert(event.data.encoding == Encoding::BINARY, 'wire text must be returned as bytes')
  event = next_event(ws)
  assert(event.type == NetworkEvent::MESSAGE && event.data == 'server-pong-seen'.b, 'server ping not answered')
  ws.close
end

check('failed WebSocket handshake reports an error message') do
  ws = Network.websocket(WS_BASE + '/reject')
  $sockets << ws
  event = next_event(ws)
  assert(event.type == NetworkEvent::ERROR && !event.message.empty?, 'missing error event message')
  assert(ws.ready_state == WebSocket::CLOSED && !ws.error.empty?, 'failure state')
end

check('event queue limit retains close') do
  Network.max_events = 2
  ws = Network.websocket(WS_BASE + '/burst')
  $sockets << ws
  wait_until { ws.ready_state == WebSocket::CLOSED }
  Network.update
  assert(ws.pending <= 2 && ws.dropped > 0, "queue overflow: #{ws.pending}/#{ws.dropped}")
  events = []
  events << ws.next_event while ws.pending > 0
  assert(events.any? { |event| event.type == NetworkEvent::CLOSE && event.reason == 'burst-end' }, 'close event dropped')
ensure
  Network.max_events = 1024
end

check('WebSocket.new connects') do
  ws = socket('/echo', true)
  ws.send('direct')
  assert(next_event(ws).data == 'direct'.b, 'direct socket echo')
  ws.close
end

check('network settings clamp invalid values') do
  Network.timeout = 0
  Network.max_events = 0
  assert(Network.timeout == 1 && Network.max_events == 1, 'settings not clamped')
ensure
  Network.timeout = 30
  Network.max_events = 1024
end

$sockets.each { |ws| ws.close if ws.ready_state == WebSocket::OPEN }
$results << 'ALL_OK' unless $results.any? { |line| line.start_with?('FAIL') }
File.write('network-results.txt', $results.join("\n") + "\n")
exit

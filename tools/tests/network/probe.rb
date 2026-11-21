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

check('all 373 Ruby constant values match native definitions') do
__CONSTANT_CHECKS__
end

check('GET text, binary body, headers and idempotent start') do
  fetch = request('/get')
  assert(fetch.success? && fetch.status == 200 && fetch.status_text == 'OK', 'GET status')
  assert(fetch.text == 'fetch 中文', "text mismatch: #{fetch.text.inspect}")
  assert(fetch.body == 'fetch 中文'.b && fetch.body.encoding == Encoding::BINARY, 'binary body')
  assert(fetch.header('x-TEST') == 'final' && fetch.header('missing') == '', 'header lookup')
  assert(fetch.progress == 1.0 && fetch.error.empty?, 'completion state')
  fetch.start
  assert(fetch.done? && fetch.text == 'fetch 中文', 'start must be idempotent')
end

check('POST binary body and request header') do
  body = "\x00\xffpost\x00".b
  fetch = request('/echo', 'POST', body, ['X-Request: fixture'])
  assert(fetch.success? && fetch.status == 201, 'POST status')
  assert(fetch.body == body && fetch.header('x-request') == 'fixture', 'POST echo')
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

check('WebSocket UTF-8 text, ping and close') do
  ws = socket
  ws.send_text('echo 中文')
  event = next_event(ws)
  assert(event.type == NetworkEvent::TEXT && event.text == 'echo 中文', 'text echo')
  assert(event.data == 'echo 中文'.b, 'event binary data')
  ws.send_ping('client-ping')
  event = next_event(ws)
  assert(event.type == NetworkEvent::TEXT && event.text == 'client-ping-seen', "unexpected ping event: #{event.type}")
  ws.close(1000, 'finished')
  assert(ws.ready_state == WebSocket::CLOSING || ws.ready_state == WebSocket::CLOSED, 'close does not update state')
  event = next_event(ws)
  assert(event.type == NetworkEvent::CLOSE && event.code == 1000 && event.reason == 'finished', 'close event')
  assert(ws.ready_state == WebSocket::CLOSED, 'closed state')
end

check('empty text and binary WebSocket messages') do
  ws = socket
  ws.send_text('')
  event = next_event(ws)
  assert(event.type == NetworkEvent::TEXT && event.text.empty?, 'empty text event')
  ws.send_binary(''.b)
  event = next_event(ws)
  assert(event.type == NetworkEvent::BINARY && event.data.empty?, 'empty binary event')
  ws.close
end

check('large binary WebSocket frame') do
  ws = socket
  data = ("\x00\xfflarge\x00".b * 25000)
  ws.send_binary(data)
  event = next_event(ws)
  assert(event.type == NetworkEvent::BINARY && event.data == data, "binary frame truncated: #{event.data.bytesize}")
  assert(event.data.encoding == Encoding::BINARY, 'binary event encoding')
  ws.close
end

check('fragmented text with an interleaved ping') do
  ws = socket('/fragments')
  event = next_event(ws)
  assert(event.type == NetworkEvent::TEXT && event.text == 'fragment-中文', "fragmented message: #{event.text.inspect}")
  event = next_event(ws)
  assert(event.type == NetworkEvent::TEXT && event.text == 'server-pong-seen', 'server ping not answered')
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
  ws.send_text('direct')
  assert(next_event(ws).text == 'direct', 'direct socket echo')
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

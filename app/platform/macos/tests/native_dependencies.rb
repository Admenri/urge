$stdout.sync = true
begin

def until_done
  deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + 15
  loop do
    Network.update
    break if yield
    raise 'network timeout' if Process.clock_gettime(Process::CLOCK_MONOTONIC) > deadline
    Graphics.update
    Input.update
  end
end

base = File.read('http_url').strip
expected = File.binread('payload')
# Check Ruby's zlib extension as well as curl's HTTP decompressor. Chunked
# streams and file wrappers exercise APIs that script archive loading misses.
raise 'gzip round trip' unless Zlib.gunzip(Zlib.gzip(expected)).b == expected.b
raise 'deflate round trip' unless Zlib.inflate(Zlib.deflate(expected)).b == expected.b
[-Zlib::MAX_WBITS, Zlib::MAX_WBITS, Zlib::MAX_WBITS + 16].each do |window|
  compressor = Zlib::Deflate.new(Zlib::BEST_COMPRESSION, window)
  compressed = ''.b
  expected.bytes.each_slice(4093) { |chunk| compressed << compressor.deflate(chunk.pack('C*'), Zlib::SYNC_FLUSH) }
  compressed << compressor.finish
  compressor.close
  inflater = Zlib::Inflate.new(window)
  decoded = ''.b
  compressed.bytes.each_slice(113) { |chunk| decoded << inflater.inflate(chunk.pack('C*')) }
  decoded << inflater.finish
  inflater.close
  raise "streaming round trip #{window}" unless decoded == expected.b
end
Zlib::GzipWriter.open('roundtrip.gz') { |file| file.write(expected) }
Zlib::GzipReader.open('roundtrip.gz') { |file| raise 'gzip file round trip' unless file.read.b == expected.b }
bad = Zlib.gzip(expected)
bad.setbyte(bad.bytesize - 8, bad.getbyte(bad.bytesize - 8) ^ 255)
begin
  Zlib.gunzip(bad)
  raise 'gzip CRC corruption accepted'
rescue Zlib::GzipFile::CRCError
end
puts 'PASS Ruby gzip/deflate, raw streams, gzip files and CRC validation'

# UTS #46 maps fullwidth "localhost" to localhost, exercising a real IDN
# request without changing hosts/DNS configuration or using the public network.
fetch = Network.fetch(base.sub('127.0.0.1', 'ｌｏｃａｌｈｏｓｔ') + '/identity')
fetch.start
until_done { fetch.done? }
raise "IDN fetch: #{fetch.error}" unless fetch.success? && fetch.text.b == expected.b
puts 'PASS IDN hostname conversion'

%w[identity gzip deflate raw-deflate br zstd stacked redirect].each do |encoding|
  fetch = Network.fetch("#{base}/#{encoding}")
  fetch.start
  until_done { fetch.done? }
  raise "#{encoding}: #{fetch.error}" unless fetch.success? && fetch.status == 200
  raise "#{encoding}: corrupted body" unless fetch.text.b == expected.b
  puts "PASS fetch #{encoding} bytes=#{fetch.text.bytesize}"
end

%w[bad-gzip bad-deflate bad-crc truncated-gzip].each do |encoding|
  fetch = Network.fetch("#{base}/#{encoding}")
  fetch.start
  until_done { fetch.done? }
  raise "#{encoding}: accepted corrupted response" if fetch.success? || fetch.error.empty?
  puts "PASS rejected #{encoding}"
end

fetch = Network.fetch("#{base}/accept-encoding")
fetch.start
until_done { fetch.done? }
raise fetch.error unless fetch.success?
%w[gzip deflate br zstd].each { |name| raise "not advertised: #{name}" unless fetch.text.include?(name) }
puts "PASS Accept-Encoding=#{fetch.text}"

# An untrusted localhost certificate must also fail through the actual binding.
fetch = Network.fetch(File.read('https_url').strip)
fetch.start
until_done { fetch.done? }
raise 'untrusted TLS certificate accepted' if fetch.success? || fetch.error.empty?
puts 'PASS TLS certificate rejection'

socket = Network.websocket(base.sub('http:', 'ws:') + '/echo')
until_done { socket.ready_state == 1 || socket.ready_state == 3 }
raise socket.error unless socket.ready_state == 1
socket.send_text('原生 WebSocket echo')
message = nil
until_done do
  while (event = socket.next_event)
    message = event.text if event.type == 1
    raise event.message if event.type == 4
  end
  !message.nil?
end
raise "WebSocket echo mismatch: #{message}" unless message == '原生 WebSocket echo'
socket.close
puts 'PASS WebSocket echo'

%w[png jpg tif webp avif].each do |format|
  bitmap = Bitmap.new("Graphics/sample.#{format}")
  raise "#{format}: invalid dimensions" unless bitmap.width > 0 && bitmap.height > 0
  bitmap.dispose
  puts "PASS image #{format}"
end
puts 'NATIVE_DEPENDENCIES_PASS'
exit
rescue StandardError => error
  warn "NATIVE_DEPENDENCIES_FAIL: #{error.full_message}"
  exit 1
end

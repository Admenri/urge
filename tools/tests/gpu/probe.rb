# Public Ruby GPU regressions. Deliberately do not call map_state or perform
# successful buffer.write: those native entry points are deferred by request.
$results = []
$buffers = []
WHOLE_MAP_SIZE = 0xffffffffffffffff
PAYLOAD = "\xff\x00\x80abcde".b * 2

def assert(value, message)
  raise message unless value
end

def check(name)
  yield
  $results << "PASS #{name}"
rescue StandardError => error
  $results << "FAIL #{name}: #{error.class}: #{error.message}"
ensure
  File.write('gpu-results.txt', $results.join("\n") + "\n")
end

def expect_gpu_error
  begin
    yield
  rescue RGSSError
    return
  end
  raise 'expected RGSSError'
end

def expect_range_error
  begin
    yield
  rescue RangeError
    return
  end
  raise 'expected RangeError'
end

def buffer(size = 16, usage = GPU::MAP_READ | GPU::COPY_DST, mapped = false)
  desc = GPUBufferDescriptor.new
  desc.size = size
  desc.usage = usage
  desc.mapped_at_creation = mapped
  result = GPU.create_buffer(desc)
  $buffers << result
  result
end

check('uint32 attributes preserve the full unsigned range') do
  view = GPUTextureViewDescriptor.new
  assert(view.mip_level_count == 0xffffffff, 'mip count was narrowed to int32')
  assert(view.array_layer_count == 0xffffffff, 'array count was narrowed to int32')
  view.mip_level_count = 0xffffffff
  view.array_layer_count = 0x80000000
  assert(view.mip_level_count == 0xffffffff && view.array_layer_count == 0x80000000, 'unsigned attribute roundtrip')
  expect_range_error { view.mip_level_count = -1 }
  expect_range_error { view.mip_level_count = 1 << 32 }
  assert(view.mip_level_count == 0xffffffff, 'rejected assignment changed the attribute')
end

check('GPUBuffer properties are exported read-only') do
  b = buffer
  assert(b.respond_to?(:usage) && b.respond_to?(:map_state), 'missing getters')
  assert(!b.respond_to?(:usage=) && !b.respond_to?(:map_state=), 'read-only property has a writer')
  assert(b.usage == GPU::MAP_READ | GPU::COPY_DST, 'usage getter changed')
end

check('vertex slots reject overflow before vector allocation') do
  d = GPURenderPipelineDescriptor.new
  [0xffffffff, 1 << 30].each do |slot|
    expect_gpu_error { d.set_vertex_buffer(slot, 0) }
    expect_gpu_error { d.add_vertex_attribute(slot, 0, GPU::VERTEX_FORMAT_FLOAT32, 0) }
  end
  expect_gpu_error { d.add_vertex_attribute(0, 0xffffffff, GPU::VERTEX_FORMAT_FLOAT32, 0) }
  d.set_vertex_buffer(0, 4)
  d.add_vertex_attribute(0, 0, GPU::VERTEX_FORMAT_FLOAT32, 0)
end

check('color slots reject overflow before vector allocation') do
  d = GPURenderPipelineDescriptor.new
  [0xffffffff, 1 << 30].each do |index|
    expect_gpu_error { d.set_color_blend(index, 1, 2, 1) }
  end
  d.add_color_target(GPU::TEXTURE_FORMAT_RGBA8_UNORM)
  d.set_color_blend(0, 1, 2, 1)
end

check('descriptor attachment and attribute counts are bounded') do
  d = GPURenderPipelineDescriptor.new
  stopped = false
  64.times do
    begin
      d.add_color_target(GPU::TEXTURE_FORMAT_RGBA8_UNORM)
    rescue RGSSError
      stopped = true
      break
    end
  end
  assert(stopped, 'unbounded color target list')
  d = GPURenderPipelineDescriptor.new
  stopped = false
  64.times do
    begin
      d.add_vertex_attribute(0, 0, GPU::VERTEX_FORMAT_FLOAT32, 0)
    rescue RGSSError
      stopped = true
      break
    end
  end
  assert(stopped, 'unbounded vertex attribute list')
end

check('synchronous read flushes queued writes and returns binary data') do
  b = buffer
  GPU.queue.write_buffer(b, 0, PAYLOAD)
  actual = b.read(0, 16)  # No explicit submit before the read.
  assert(actual == PAYLOAD, "read returned stale bytes: #{actual.bytes.inspect}")
  assert(actual.encoding == Encoding::BINARY && actual.valid_encoding?, 'buffer read is tagged as text')
  assert(b.read(0, 16) == PAYLOAD, 'repeat automatic map/read')
end

check('whole-map sentinel is normalized to the remaining buffer length') do
  b = buffer
  GPU.queue.write_buffer(b, 0, PAYLOAD)
  assert(b.read(0, WHOLE_MAP_SIZE) == PAYLOAD, 'whole-buffer length not normalized')
  assert(b.read(8, WHOLE_MAP_SIZE) == PAYLOAD.byteslice(8, 8), 'remaining-buffer length not normalized')
  assert(b.read(16, WHOLE_MAP_SIZE).empty?, 'empty suffix should not map or allocate')
end

check('explicit mappings are reused without overlapping range acquisition') do
  b = buffer
  GPU.queue.write_buffer(b, 0, PAYLOAD)
  assert(b.map_async(GPU::MAP_READ, 0, WHOLE_MAP_SIZE), 'explicit mapping failed')
  2.times do
    assert(b.read(0, 8) == PAYLOAD.byteslice(0, 8), 'first mapped subrange')
    assert(b.read(8, 8) == PAYLOAD.byteslice(8, 8), 'second mapped subrange')
  end
  expect_gpu_error { b.map_async(GPU::MAP_READ, 0, 16) }
  assert(b.read(0, 16) == PAYLOAD, 'failed remap destroyed the active mapping')
  b.unmap
  assert(b.map_async(GPU::MAP_READ, 8, 8), 'remap after unmap failed')
  expect_gpu_error { b.read(0, 8) }
  assert(b.read(8, WHOLE_MAP_SIZE) == PAYLOAD.byteslice(8, 8), 'partial mapping did not survive rejected read')
  b.unmap
end

check('mapped-at-creation reads need no MAP_READ usage or second mapping') do
  b = buffer(8, GPU::COPY_SRC, true)
  2.times { assert(b.read(0, 8) == "\x00".b * 8, 'initial mapping read') }
  expect_gpu_error { b.map_async(GPU::MAP_READ, 0, 8) }
  b.unmap
end

check('mapping bounds and alignment reject invalid input') do
  b = buffer
  [[1 << 32, 8], [0, (1 << 32) + 8], [WHOLE_MAP_SIZE, 8], [8, 16], [4, 4], [0, 3]].each do |offset, size|
    expect_gpu_error { b.read(offset, size) }
    expect_gpu_error { b.map_async(GPU::MAP_READ, offset, size) }
  end
  assert(b.read(0, 0).empty?, 'empty read')
end

check('invalid map modes and usage fail before entering the C API') do
  b = buffer
  [0, 3, 0xffffffff].each { |mode| expect_gpu_error { b.map_async(mode, 0, 8) } }
  expect_gpu_error { b.map_async(GPU::MAP_WRITE, 0, 8) }
  b = buffer(8, GPU::COPY_SRC)
  expect_gpu_error { b.read(0, 8) }
end

check('writes reject invalid ranges and read-only mappings') do
  b = buffer
  expect_gpu_error { b.write(1 << 32, '12345678'.b) }
  assert(b.map_async(GPU::MAP_READ, 0, 16), 'read-only mapping')
  expect_gpu_error { b.write(0, '12345678'.b) }
  b.unmap
end

check('destroy and unmap invalidate cached mapping data') do
  b = buffer
  assert(b.map_async(GPU::MAP_READ, 0, 16), 'mapping before destroy')
  b.read(0, 8)
  b.destroy
  expect_gpu_error { b.read(0, 8) }
  expect_gpu_error { b.map_async(GPU::MAP_READ, 0, 8) }
  b.unmap
  b.destroy
end

check('uncaptured WebGPU validation errors do not terminate Ruby') do
  desc = GPUBufferDescriptor.new
  desc.size = 8
  desc.usage = 0
  # Empty usage is a synchronous device validation error. Merely recording an
  # invalid copy command would defer validation until encoder finish/submit.
  GPU.create_buffer(desc)
  File.write('validation-recovered.txt', 'Ruby continued after GPU validation')
end

$buffers.each(&:destroy)
$results << 'SKIP successful GPUBuffer#write and map_state: native backend implementation deferred'
$results << 'ALL_OK' unless $results.any? { |line| line.start_with?('FAIL') }
File.write('gpu-results.txt', $results.join("\n") + "\n")
exit

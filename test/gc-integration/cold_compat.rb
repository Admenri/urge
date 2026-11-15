# encoding: UTF-8
# Run with gc-integration/run.py --script cold_compat.rb against the freshly
# linked cold bindings. Uses only public production APIs (no native-GC probes).
$stdout.sync = true
$cold_compat_failures = []

def cold_assert(value, message = 'assertion failed')
  raise message unless value
end

def cold_raises(type)
  begin
    yield
  rescue type
    return
  end
  raise "expected #{type}"
end

def cold_collect
  6.times { GC.start(full_mark: true, immediate_mark: true, immediate_sweep: true) }
end

def cold_check(name)
  yield
  puts "PASS #{name}"
rescue Exception => error
  $cold_compat_failures << "#{name}: #{error.class}: #{error.message}"
  puts "FAIL #{$cold_compat_failures.last}"
end

cold_check('Allocated RGSS disposables and Sprite bitmap retain legacy behavior') do
  [Sprite, Plane, WindowXP, WindowVX, TilemapXP, TilemapVX, Viewport, Bitmap].each do |type|
    object = type.allocate
    cold_assert(object.disposed? == true, "#{type}.allocate is not disposed")
    cold_assert(object.dispose.nil?)
    cold_assert(object.dispose.nil?)
    cold_assert(object.disposed? == true)
    cold_raises(ArgumentError) { object.dispose(1) }
    cold_raises(ArgumentError) { object.disposed?(1) }
  end
  sprite = Sprite.allocate
  cold_assert(sprite.bitmap.nil?, 'Sprite.allocate.bitmap must be nil')
  cold_raises(ArgumentError) { sprite.bitmap(1) }
  cold_raises(RGSSError) { sprite.bitmap = nil }
  cold_assert(TilemapXP.allocate.autotiles.nil?)
  cold_assert(TilemapVX.allocate.bitmaps.nil?)
end

cold_check('Live disposables permit repeated dispose and disposed? reads') do
  [Sprite.new, Bitmap.new(2, 2), Viewport.new, TilemapXP.new, TilemapVX.new].each do |object|
    cold_assert(object.disposed? == false, "#{object.class} starts disposed")
    cold_assert(object.dispose.nil?)
    cold_assert(object.dispose.nil?)
    cold_assert(object.disposed? == true)
  end
end

cold_check('Sprite bitmap stays canonical and readable after disposal') do
  sprite = Sprite.new
  cold_assert(sprite.bitmap.nil?)
  bitmap = Bitmap.new(2, 2)
  bitmap.instance_variable_set(:@cold_state, 'sprite bitmap')
  original_id = bitmap.object_id
  sprite.bitmap = bitmap
  cold_assert(sprite.bitmap.equal?(bitmap))
  bitmap = nil
  sprite.dispose
  sprite.dispose
  cold_collect
  GC.compact if GC.respond_to?(:compact)
  cold_assert(sprite.disposed?)
  cold_assert(sprite.bitmap.object_id == original_id)
  cold_assert(sprite.bitmap.equal?(sprite.bitmap))
  cold_assert(sprite.bitmap.instance_variable_get(:@cold_state) == 'sprite bitmap')
  sprite.bitmap.dispose
  cold_assert(sprite.bitmap.disposed?, 'disposed bitmap must remain readable')
  cold_raises(RGSSError) { sprite.x }
  cold_raises(RGSSError) { sprite.bitmap = nil }
end

[[TilemapXP, :autotiles, :get_autotile, :set_autotile, 'TilemapAutotileArray', 7],
 [TilemapVX, :bitmaps, :get_bitmap, :set_bitmap, 'TilemapBitmapArray', 9]].each do |type, accessor, getter, setter, proxy_name, count|
  cold_check("#{type} array proxy API, parent link, bounds and current native state") do
    tilemap = type.new
    slots = tilemap.public_send(accessor)
    cold_assert(slots.instance_of?(Object.const_get(proxy_name)))
    cold_assert(slots.equal?(tilemap.public_send(accessor)), 'proxy identity changed')
    cold_assert(slots.instance_variable_get(:@parent).equal?(tilemap))
    cold_assert(!slots.instance_variable_defined?(:@values), 'old Bitmap cache returned')
    cold_raises(ArgumentError) { tilemap.public_send(accessor, 1) }
    cold_raises(ArgumentError) { slots.public_send(:[]) }
    cold_raises(ArgumentError) { slots.public_send(:[]=, 0) }
    cold_raises(TypeError) { slots.public_send(:[]=, 0, Object.new) }
    [-1, count].each do |index|
      cold_assert(slots[index].nil?)
      cold_assert(slots.public_send(:[]=, index, Object.new).nil?)
    end
    cold_assert(slots[0].nil?)
    bitmap = Bitmap.new(96, 128)
    replacement = Bitmap.new(96, 128)
    cold_assert(slots.public_send(:[]=, 0, bitmap).equal?(bitmap))
    cold_assert(slots[0].equal?(bitmap))
    cold_assert(tilemap.public_send(getter, 0).equal?(bitmap))
    tilemap.public_send(setter, 0, replacement)
    cold_assert(slots[0].equal?(replacement), 'proxy ignored a native slot replacement')
    cold_assert(slots.public_send(:[]=, count - 1, bitmap).equal?(bitmap))
    cold_assert(slots[count - 1].equal?(bitmap))
    cold_assert(slots.public_send(:[]=, 0, nil).nil?)
    cold_assert(tilemap.public_send(getter, 0).nil?)
    cold_assert(slots[0].nil?)
    tilemap.dispose
    cold_assert(tilemap.public_send(accessor).equal?(slots), 'disposed proxy disappeared')
    tilemap.dispose
    cold_assert(tilemap.disposed?)
    bitmap.dispose
    replacement.dispose
  end

  cold_check("#{type} proxy alone retains parent and original Bitmap Ruby state") do
    slots, parent_id, bitmap_id = Fiber.new do
      tilemap = type.new
      bitmap = Bitmap.new(96, 128)
      bitmap.instance_variable_set(:@cold_state, 'tilemap bitmap')
      proxy = tilemap.public_send(accessor)
      proxy[0] = bitmap
      [proxy, tilemap.object_id, bitmap.object_id]
    end.resume
    cold_collect
    GC.compact if GC.respond_to?(:compact)
    parent = slots.instance_variable_get(:@parent)
    cold_assert(parent.object_id == parent_id, 'proxy lost its Ruby parent')
    cold_assert(slots[0].object_id == bitmap_id, 'native-held Bitmap identity changed')
    cold_assert(slots[0].instance_variable_get(:@cold_state) == 'tilemap bitmap')
    cold_assert(parent.public_send(accessor).equal?(slots))
    slots[0].dispose
    parent.dispose
  end

  cold_check("#{type} replaced Bitmap does not survive in a permanent proxy cache") do
    tilemap = type.new
    slots = tilemap.public_send(accessor)
    weak = Fiber.new do
      map = ObjectSpace::WeakMap.new
      old = Bitmap.new(96, 128)
      map[old] = true
      slots[0] = old
      cold_assert(slots[0].equal?(old))
      slots[0] = Bitmap.new(96, 128)
      map
    end.resume
    cold_collect
    cold_assert(weak.keys.empty?, 'proxy retained a replaced Bitmap')
    slots[0].dispose
    tilemap.dispose
  end
end

cold_check('Input legacy query names accept Symbols, Strings and numeric RGSS ids') do
  [:press?, :trigger?, :repeat?].each do |query|
    cold_assert(Input.respond_to?(query), "Input.#{query} is missing")
    Input.update
    symbol = Input.public_send(query, :C)
    cold_assert(symbol == true || symbol == false)
    cold_assert(Input.public_send(query, 'C') == symbol)
    cold_assert(Input.public_send(query, Input::C) == symbol)
    [nil, Object.new, -999, :__cold_unknown_key].each do |key|
      cold_assert(Input.public_send(query, key) == false)
    end
    cold_raises(ArgumentError) { Input.public_send(query) }
    cold_raises(ArgumentError) { Input.public_send(query, :C, :B) }
  end
  [:pressed, :triggered, :repeated].each do |query|
    cold_assert(Input.respond_to?(query), "existing Input.#{query} disappeared")
  end
  cold_assert(Input::DOWN == 2 && Input::C == 13 && Input::F9 == 29)
end

cold_check('Mouse legacy button constants have their original numeric values') do
  { LEFT: 1, MIDDLE: 2, RIGHT: 3, X1: 4, X2: 5 }.each do |name, value|
    cold_assert(Mouse.const_get(name) == value, "Mouse::#{name} changed")
  end
end

cold_check('Table manual indexing preserves value-last writes and zero bounds reads') do
  table = Table.new(2, 2, 2)
  table[1] = 11
  table[1, 1] = 22
  table[1, 1, 1] = 33
  cold_assert(table[1] == 11)
  cold_assert(table[1, 1] == 22)
  cold_assert(table[1, 1, 1] == 33)
  cold_assert(table[-1] == 0 && table[2] == 0)
  cold_assert(table[0, 2] == 0 && table[0, 0, 2] == 0)
  table[2, 2, 2] = 99
  cold_assert(table[1, 1, 1] == 33)
end

if $cold_compat_failures.empty?
  File.write('gc-results.txt', "ALL_OK\n")
  puts 'ALL_OK'
  exit(0)
else
  File.write('gc-results.txt', $cold_compat_failures.join("\n") + "\n")
  puts "FAILED #{$cold_compat_failures.length}"
  exit(1)
end

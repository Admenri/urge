# encoding: UTF-8
# Runs inside the real engine, not a stand-in for its Ruby heap.
$stdout.sync = true
$gc_failures = []

# Registered first, so this runs after the shutdown checks registered below.
at_exit do
  if $gc_failures.empty?
    File.write('gc-results.txt', "ALL_OK\n")
    puts 'ALL_OK'
  else
    File.write('gc-results.txt', $gc_failures.join("\n") + "\n")
    puts "FAILED #{$gc_failures.length}"
  end
end

def check_gc(name)
  yield
  puts "PASS #{name}"
rescue Exception => error
  $gc_failures << "#{name}: #{error.class}: #{error.message}"
  puts "FAIL #{$gc_failures.last}"
ensure
  GC.stress = false
end

def gc_assert(value, message = 'assertion failed')
  raise message unless value
end

def full_gc
  GC.start(full_mark: true, immediate_mark: true, immediate_sweep: true)
end

check_gc('Visible scene remains renderable throughout collection') do
  $gc_display = Sprite.new
  $gc_display.bitmap = Bitmap.new(320, 240)
  $gc_display.bitmap.fill_rect(0, 0, 320, 240, Color.new(28, 48, 66))
  $gc_display.bitmap.fill_rect(16, 70, 288, 100, Color.new(55, 140, 90))
  $gc_display.bitmap.draw_text(16, 80, 288, 40, 'GC identity regression', 1)
  $gc_display.bitmap.draw_text(16, 120, 288, 32, 'Native + Ruby graph', 1)
end

check_gc('Sprite bitmap canonical identity and instance variables') do
  sprite = Sprite.new
  bitmap = Bitmap.new(4, 4)
  bitmap.instance_variable_set(:@gc_payload, ['kept', Object.new])
  original_id = bitmap.object_id
  sprite.bitmap = bitmap
  bitmap = nil
  3.times { full_gc }
  gc_assert(sprite.bitmap.object_id == original_id)
  gc_assert(sprite.bitmap.equal?(sprite.bitmap))
  gc_assert(sprite.bitmap.instance_variable_get(:@gc_payload).first == 'kept')
  sprite.dispose
end

check_gc('Native-created Rect and Font wrappers retain Ruby state') do
  sprite = Sprite.new
  rect = sprite.src_rect
  rect.instance_variable_set(:@gc_payload, 'rect state')
  rect_id = rect.object_id
  rect = nil
  bitmap = Bitmap.new(2, 2)
  font = bitmap.font
  font.instance_variable_set(:@gc_payload, 'font state')
  font_id = font.object_id
  font = nil
  3.times { full_gc }
  gc_assert(sprite.src_rect.object_id == rect_id)
  gc_assert(sprite.src_rect.instance_variable_get(:@gc_payload) == 'rect state')
  gc_assert(bitmap.font.object_id == font_id)
  gc_assert(bitmap.font.instance_variable_get(:@gc_payload) == 'font state')
  sprite.dispose
  bitmap.dispose
end

class GCTaggedNode < Node3D
  def initialize(tag)
    super()
    @tag = tag
  end
end

check_gc('Native children retain the original Ruby subclass and singleton methods') do
  parent = Node3D.new
  child = GCTaggedNode.new('original child')
  child.define_singleton_method(:gc_marker) { @tag }
  original_id = child.object_id
  parent.add_child(child)
  child = nil
  3.times { full_gc }
  GC.compact if GC.respond_to?(:compact)
  fetched = parent.child_at(0)
  gc_assert(fetched.object_id == original_id)
  gc_assert(fetched.instance_of?(GCTaggedNode))
  gc_assert(fetched.gc_marker == 'original child')
  gc_assert(parent.child_at(0).equal?(fetched))
  gc_assert(fetched.parent.equal?(parent))
  parent.remove_child(fetched)
end

check_gc('children array entries reuse existing Ruby objects') do
  parent = Node3D.new
  child = GCTaggedNode.new('array child')
  original_id = child.object_id
  parent.add_child(child)
  child = nil
  full_gc
  entries = parent.children
  gc_assert(entries.size == 1)
  gc_assert(entries.first.object_id == original_id)
  gc_assert(entries.first.equal?(parent.child_at(0)))
  gc_assert(entries.first.instance_variable_get(:@tag) == 'array child')
end

check_gc('Shared native children expose the current graph without stale caches') do
  first = Node3D.new
  second = Node3D.new
  child = GCTaggedNode.new('reparented')
  original_id = child.object_id
  first.add_child(child)
  second.add_child(child)
  child = nil
  full_gc
  gc_assert(first.child_count == 0)
  gc_assert(second.child_count == 1)
  gc_assert(second.child_at(0).object_id == original_id)
  gc_assert(second.child_at(0).parent.equal?(second))
end

check_gc('GC compaction preserves value-wrapper identity and arbitrary Ruby state') do
  node = Node3D.new
  value = node.position
  value.instance_variable_set(:@gc_payload, { 'token' => Object.new })
  original_id = value.object_id
  value = nil
  3.times do
    full_gc
    GC.compact if GC.respond_to?(:compact)
  end
  gc_assert(node.position.object_id == original_id)
  gc_assert(node.position.instance_variable_get(:@gc_payload).key?('token'))
end

check_gc('Minor and incremental GC preserve native-held wrapper state') do
  parent = Node3D.new
  first = GCTaggedNode.new('first')
  parent.add_child(first)
  first_id = first.object_id
  first = nil
  GC.start(full_mark: true, immediate_mark: false, immediate_sweep: false)
  second = GCTaggedNode.new('late child')
  parent.add_child(second)
  second_id = second.object_id
  second = nil
  3000.times { Object.new }
  3.times { GC.start(full_mark: false, immediate_mark: true, immediate_sweep: true) }
  gc_assert(parent.child_at(0).object_id == first_id)
  gc_assert(parent.child_at(1).object_id == second_id)
  gc_assert(parent.child_at(1).instance_variable_get(:@tag) == 'late child')
  full_gc
end

check_gc('Frozen reference setters reject before replacing native state') do
  sprite = Sprite.new
  bitmap = Bitmap.new(2, 2)
  replacement = Bitmap.new(2, 2)
  sprite.bitmap = bitmap
  sprite.freeze
  rejected = false
  begin
    sprite.bitmap = replacement
  rescue FrozenError
    rejected = true
  end
  gc_assert(rejected, 'frozen bitmap setter was accepted')
  gc_assert(sprite.bitmap.equal?(bitmap), 'frozen setter changed native state')
  sprite.dispose
end

check_gc('Wrong native base initializer cannot create invalid typed payload') do
  sprite = Sprite.allocate
  rejected = false
  begin
    Node.instance_method(:initialize).bind(sprite).call
  rescue RuntimeError, TypeError, RGSSError
    rejected = true
  end
  gc_assert(rejected, 'Node native data was attached to a Sprite datatype')
end

check_gc('Float vector warning hooks may raise and throw without native owning frames') do
  mesh = Mesh3D.new
  original = Warning.method(:warn)
  original_verbose = $VERBOSE
  $VERBOSE = false  # Installing a replacement itself emits method-redefined warnings.
  begin
    Warning.define_singleton_method(:warn) { |_message, **_keywords| raise 'gc warning hook' }
    $VERBOSE = true
    rejected = false
    begin
      mesh.set_vertices([0.0, 0.0, 1 << 20000])
    rescue RuntimeError => error
      rejected = error.message == 'gc warning hook'
    end
    gc_assert(rejected)
    $VERBOSE = false
    Warning.define_singleton_method(:warn) { |_message, **_keywords| throw :gc_warning, :escaped }
    $VERBOSE = true
    escaped = catch(:gc_warning) { mesh.set_vertices([0.0, 0.0, 1 << 20000]); :missed }
    gc_assert(escaped == :escaped)
  ensure
    $VERBOSE = false
    Warning.define_singleton_method(:warn, original)
    $VERBOSE = original_verbose
  end
end

check_gc('Numeric vectors reject NaN before integer conversion') do
  mesh = Mesh3D.new
  rejected = false
  begin
    mesh.set_indices([0, Float::NAN])
  rescue RangeError, FloatDomainError
    rejected = true
  end
  gc_assert(rejected, 'NaN index was accepted')
end

check_gc('Later String coercion cannot invalidate an earlier parameter buffer') do
  original = File.expand_path('Data')
  path = original.dup
  mount = Object.new
  mount.define_singleton_method(:to_s) { path.replace('changed' * 10000); 'gc_string_snapshot' }
  gc_assert(URGE.add_load_path(path, mount, true))
  filename = Dir['Data/Scripts.*'].first.sub('Data/', '')
  gc_assert(load_data('gc_string_snapshot/' + filename).is_a?(Array))
  gc_assert(URGE.remove_load_path(original))
end

check_gc('Marshal and dup produce independent native objects') do
  color = Color.new(12, 34, 56, 78)
  copy = color.dup
  loaded = Marshal.load(Marshal.dump(color))
  gc_assert(!copy.equal?(color))
  gc_assert(!loaded.equal?(color))
  copy.red = 99
  gc_assert(color.red == 12)
  full_gc
  gc_assert(loaded.green == 34)
  frozen_copy = color.freeze.clone
  gc_assert(frozen_copy.frozen?)
  gc_assert(!frozen_copy.equal?(color))
end

check_gc('Construction and native-return wrapping survive GC.stress') do
  GC.stress = true
  12.times do |index|
    parent = Node3D.new
    child = GCTaggedNode.new(index)
    parent.add_child(child)
    gc_assert(parent.child_at(0).equal?(child))
    gc_assert(child.parent.equal?(parent))
    gc_assert(parent.position.equal?(parent.position))
    color = Marshal.load(Marshal.dump(Color.new(index, 2, 3)))
    gc_assert(color.red == index)
  end
end

# Build unreachable cycles in a separate Ruby frame so the test harness itself
# does not retain the objects through locals or a captured block environment.
def make_cross_heap_cycle
  weak = ObjectSpace::WeakMap.new
  parent = Node3D.new
  child = GCTaggedNode.new('cycle')
  parent.add_child(child)
  child.instance_variable_set(:@gc_back_reference, parent)
  weak[parent] = true
  weak[child] = true
  weak
end

check_gc('Unrooted native/Ruby cycles do not become permanent roots') do
  # A returned Ruby method is still inside vm_exec_core's live C frame; Debug
  # builds can conservatively retain its old VALUE temporaries indefinitely.
  # End the constructor's machine-stack frame, not merely its Ruby method.
  weak = Fiber.new { make_cross_heap_cycle }.resume
  6.times { full_gc }
  # WeakMap#size can lag behind cleared weak slots until iteration prunes them.
  gc_assert(weak.keys.empty?, 'cross-heap wrappers remain alive')
end

check_gc('Ordinary Ruby cycle control uses the same isolated stack boundary') do
  weak = Fiber.new do
    map = ObjectSpace::WeakMap.new
    first = Object.new
    second = Object.new
    first.instance_variable_set(:@peer, second)
    second.instance_variable_set(:@peer, first)
    map[first] = true
    map[second] = true
    map
  end.resume
  6.times { full_gc }
  gc_assert(weak.keys.empty?, 'ordinary Ruby cycle remains alive')
end

check_gc('Native objects and retired capsules are physically reclaimed') do
  gc_assert(URGE.respond_to?(:__native_gc_stats), 'GC test probe is unavailable')
  3.times { URGE.__collect_native_gc }
  before = URGE.__native_gc_stats
  Fiber.new { 64.times { Node3D.new }; nil }.resume
  6.times { URGE.__collect_native_gc }
  after = URGE.__native_gc_stats
  gc_assert(after[:native_objects] <= before[:native_objects] + 8,
            "native garbage remained: #{before.inspect} -> #{after.inspect}")
  gc_assert(after[:capsules] <= before[:capsules] + 8,
            "retired capsules remained: #{before.inspect} -> #{after.inspect}")
end

# Advancing enough frames exercises the host-owned collection safe point and
# the separate stack used by URGE_RUNNER_MODEL=fiber.
check_gc('Frame collection preserves an existing native child wrapper') do
  parent = Node3D.new
  child = GCTaggedNode.new('across frames')
  original_id = child.object_id
  parent.add_child(child)
  child = nil
  65.times { Graphics.update }
  gc_assert(parent.child_at(0).object_id == original_id)
  gc_assert(parent.child_at(0).instance_variable_get(:@tag) == 'across frames')
  snapshot = Graphics.snap_to_bitmap
  gc_assert(snapshot.get_pixel(5, 5).blue > 20, 'rendered scene is blank')
  snapshot.to_image.save_file('gc-frame.png')
  snapshot.dispose
end

$gc_shutdown_parent = Node3D.new
shutdown_child = GCTaggedNode.new('at_exit child')
$gc_shutdown_child_id = shutdown_child.object_id
$gc_shutdown_parent.add_child(shutdown_child)
shutdown_child = nil
at_exit do
  check_gc('at_exit retains identity while GC and Graphics remain available') do
    full_gc
    gc_assert($gc_shutdown_parent.child_at(0).object_id == $gc_shutdown_child_id)
    gc_assert($gc_shutdown_parent.child_at(0).instance_variable_get(:@tag) == 'at_exit child')
    Graphics.update
  end
end

exit($gc_failures.empty? ? 0 : 1)

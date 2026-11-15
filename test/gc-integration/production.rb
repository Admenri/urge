# Minimal smoke for a production build, without GC-only testing exports.
$stdout.sync = true
begin
  raise 'test-only GC API leaked into production' if URGE.respond_to?(:__native_gc_stats)
  parent = Node3D.new
  child = Node3D.new
  child.instance_variable_set(:@payload, 'production state')
  parent.add_child(child)
  child_id = child.object_id
  child = nil
  3.times { GC.start }
  raise 'identity lost' unless parent.children.first.object_id == child_id
  raise 'state lost' unless parent.child_at(0).instance_variable_get(:@payload) == 'production state'
  sprite = Sprite.new
  sprite.bitmap = Bitmap.new(32, 32)
  sprite.bitmap.fill_rect(0, 0, 32, 32, Color.new(30, 150, 80))
  id = sprite.bitmap.object_id
  3.times { GC.start }
  raise 'bitmap identity lost' unless sprite.bitmap.object_id == id
  Graphics.update
  sprite.dispose
  File.write('gc-results.txt', "ALL_OK\n")
  puts 'ALL_OK'
  exit(0)
rescue StandardError => error
  File.write('gc-results.txt', "#{error.class}: #{error.message}\n")
  puts "FAIL #{error.class}: #{error.message}"
  exit(1)
end

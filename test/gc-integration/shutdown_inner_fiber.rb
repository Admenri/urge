# Shutdown must cross a Ruby-created Fiber without a rescuable UncaughtThrowError.
$stdout.sync = true
forwarded = Fiber.new(blocking: true, storage: {gc_fixture: 9}) do |*args, **keywords, &block|
  [args, keywords, block.nil?, Fiber[:gc_fixture]]
end.resume(1, {positional: 2}, keyword: 3) { :ignored_by_ruby_resume }
raise 'Fiber argument/storage forwarding changed' unless
  forwarded == [[1, {positional: 2}], {keyword: 3}, true, 9]
normal_error = begin
  Fiber.new { raise ArgumentError, 'ordinary fiber error' }.resume
rescue Exception => error
  [error.class, error.message]
end
raise 'ordinary Fiber exception changed' unless normal_error == [ArgumentError, 'ordinary fiber error']
steps = []
rescued = []
at_exit do
  full = steps == [:fiber_dispose, :fiber_save, :outer_dispose, :outer_save] && rescued.empty?
  File.write('gc-results.txt', full ? "ALL_OK\n" : "FAIL #{steps.inspect} #{rescued.inspect}\n")
  puts(full ? 'ALL_OK' : 'ENSURE_INCOMPLETE')
end
outer = Sprite.new
inner = Sprite.new
inner.bitmap = Bitmap.new(16, 16)
begin
  Fiber.new(blocking: true, storage: nil) do
    begin
      loop do
        begin
          Graphics.update
        rescue Exception => error
          rescued << error.class.name
          File.write('shutdown-rescued.txt', rescued.join("\n"))
        end
      end
    ensure
      inner.dispose
      steps << :fiber_dispose
      save_data(steps, 'fiber-save.rxdata')
      raise 'fiber save interrupted' unless load_data('fiber-save.rxdata') == steps
      steps << :fiber_save
    end
  end.resume
  raise 'execution continued after stopped Fiber'
ensure
  outer.dispose
  steps << :outer_dispose
  save_data(steps, 'outer-save.rxdata')
  raise 'outer save interrupted' unless load_data('outer-save.rxdata') == steps
  steps << :outer_save
  File.write('shutdown-result.txt', 'ENSURE_COMPLETE')
end

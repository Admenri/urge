# CRuby protects ObjectSpace finalizers against all nonlocal jumps, not just raise.
$stdout.sync = true
# Exercise swallowing even when finalizer diagnostics are disabled; gc.c must
# not try to format a private non-exception throw as an ordinary exception.
$VERBOSE = nil
$shutdown_steps = []
$shutdown_rescued = []
at_exit do
  full = $shutdown_steps == [:finalizer_dispose, :finalizer_save, :outer_dispose, :outer_save] &&
         $shutdown_rescued.empty?
  File.write('gc-results.txt', full ? "ALL_OK\n" : "FAIL #{$shutdown_steps.inspect} #{$shutdown_rescued.inspect}\n")
  puts(full ? 'ALL_OK' : 'ENSURE_INCOMPLETE')
end

def shutdown_finalizer
  proc do |_id|
    sprite = Sprite.new
    begin
      loop do
        begin
          Graphics.update
        rescue Exception => error
          $shutdown_rescued << error.class.name
        end
      end
    ensure
      sprite.dispose
      $shutdown_steps << :finalizer_dispose
      save_data($shutdown_steps, 'finalizer-save.rxdata')
      raise 'finalizer save interrupted' unless load_data('finalizer-save.rxdata') == $shutdown_steps
      $shutdown_steps << :finalizer_save
      File.write('shutdown-finalizer-step.txt', 'FINALIZER_ENSURE_COMPLETE')
    end
  end
end

def register_shutdown_finalizer
  ObjectSpace.define_finalizer(Object.new, shutdown_finalizer)
  # The Debug VM conservatively scans C-stack remnants. A second, harmless
  # registration overwrites the first target's temporary registration root.
  ObjectSpace.define_finalizer(Object.new, proc {})
end

outer = Sprite.new
begin
  register_shutdown_finalizer
  # A swallowed stop must not leave an ordinary Ruby-only loop running forever.
  loop { GC.start(full_mark: true, immediate_sweep: true) }
ensure
  outer.dispose
  $shutdown_steps << :outer_dispose
  save_data($shutdown_steps, 'outer-save.rxdata')
  raise 'outer save interrupted' unless load_data('outer-save.rxdata') == $shutdown_steps
  $shutdown_steps << :outer_save
  File.write('shutdown-result.txt', 'ENSURE_COMPLETE')
end

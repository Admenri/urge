# Use the host-driven executable built from suspended_shutdown.cc. It closes
# the host while Graphics.update is suspended INSIDE a GC.start finalizer.
# After that finalizer catches stop, rgss_main has no return safe point: LINE
# must cross its audited POD CFUNC, but must never cross unknown native RAII.
$stdout.sync = true
$VERBOSE = nil
$shutdown_steps = []
$shutdown_rescued = []
$shutdown_finalizer_finished = false
at_exit do
  full = $shutdown_steps == [:finalizer_dispose, :finalizer_save, :outer_dispose, :outer_save] &&
         $shutdown_rescued.empty? && $shutdown_finalizer_finished
  File.write('gc-results.txt', full ? "ALL_OK\n" : "FAIL #{$shutdown_steps.inspect} #{$shutdown_rescued.inspect}\n")
  puts(full ? 'ALL_OK' : 'ENSURE_INCOMPLETE')
end

def rgss_main_shutdown_finalizer
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
      save_data($shutdown_steps, 'rgss-finalizer-save.rxdata')
      raise 'finalizer save interrupted' unless load_data('rgss-finalizer-save.rxdata') == $shutdown_steps
      $shutdown_steps << :finalizer_save
      $shutdown_finalizer_finished = true
      File.write('shutdown-finalizer-step.txt', 'FINALIZER_ENSURE_COMPLETE')
    end
  end
end

def register_rgss_main_shutdown_finalizer
  ObjectSpace.define_finalizer(Object.new, rgss_main_shutdown_finalizer)
  # Overwrite conservative Debug C-stack remnants of the first registration.
  ObjectSpace.define_finalizer(Object.new, proc {})
end

outer = Sprite.new
begin
  register_rgss_main_shutdown_finalizer
  rgss_main do
    loop do
      GC.start(full_mark: true, immediate_sweep: true)
      # With rgss_main missing from the C-frame allowlist, the finalizer-local
      # catch swallows stop and this Ruby-only loop hangs host destruction.
      if $shutdown_finalizer_finished
        spins = 0
        while true
          spins += 1
        end
      end
    end
  end
  raise 'rgss_main returned after shutdown instead of relaying stop'
ensure
  outer.dispose
  $shutdown_steps << :outer_dispose
  save_data($shutdown_steps, 'rgss-outer-save.rxdata')
  raise 'outer save interrupted' unless load_data('rgss-outer-save.rxdata') == $shutdown_steps
  $shutdown_steps << :outer_save
  File.write('shutdown-result.txt', 'ENSURE_COMPLETE')
end

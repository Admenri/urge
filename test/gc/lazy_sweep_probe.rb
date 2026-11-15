# encoding: UTF-8
# Run inside an URGE_BUILD_GC_TESTS engine (RGSS1/2/3). No WeakMap-only claims:
# assert the Ruby completion boundary and native/capsule physical reclamation.
module NativeGCLazySweepRegression
  def self.assert(value, message)
    raise message unless value
  end

  def self.make_wrappers
    # Terminate the constructing machine stack, not just a Ruby method frame.
    Fiber.new do
      wrappers = Array.new(512) { |i| Color.new(i % 256, 2, 3) }
      assert(wrappers.length == 512, 'wrapper batch was not constructed')
      nil
    end.resume
  end

  def self.collect_with_lazy_stress(disabled)
    disabled ? GC.disable : GC.enable
    GC.stress = 2
    URGE.__collect_native_gc
    raise 'driver changed the user stress value' unless GC.stress == 2
    # This setter does not finish a lazy cycle. Turn stress off before querying
    # so a statistics allocation cannot start a NEW stress cycle and mask ours.
    GC.stress = false
    state = GC.latest_gc_info(:state)
    assert(state == :none, "driver left an unfinished Ruby cycle: #{state}")
    was_disabled = GC.disable
    GC.enable unless disabled
    assert(was_disabled == disabled, 'driver changed the user GC enabled state')
  ensure
    GC.stress = false
  end

  def self.run
    assert(URGE.respond_to?(:__native_gc_stats), 'GC test exports are disabled')
    assert(URGE.respond_to?(:__collect_native_gc), 'GC collection probe is missing')
    original_stress = GC.stress
    originally_disabled = GC.disable
    GC.stress = false
    begin
      # Warm symbol/statistics paths before the stress cycle being measured.
      GC.latest_gc_info(:state)
      3.times { URGE.__collect_native_gc }
      baseline = URGE.__native_gc_stats
      [:native_objects, :capsules, :live_wrappers, :retired_capsules].each do |key|
        assert(baseline[key].is_a?(Integer), "missing integer probe field #{key}")
      end

      # Many partially occupied pages: unlike a tiny/full heap, CRuby's first
      # lazy sweep step can stop with free slots AND many pages still unswept.
      heap = Array.new(160_000) { Object.new }
      heap.each_index { |i| heap[i] = nil if i.odd? }
      [true, false].each do |disabled|
        # Each enabled/disabled mode gets an independent garbage batch: the
        # second mode must prove reclamation, not inspect the first's empty heap.
        make_wrappers
        inflated = URGE.__native_gc_stats
        assert(inflated[:native_objects] >= baseline[:native_objects] + 512,
               'native batch was reclaimed before the measured collection')
        # Completed sweep may enqueue dfree jobs; returning to Ruby lets those
        # retire capsules before the following legal native sweep boundary.
        6.times { collect_with_lazy_stress(disabled) }
        after = URGE.__native_gc_stats
        assert(after[:native_objects] <= baseline[:native_objects] + 4,
               "native objects were not reclaimed: #{baseline.inspect} -> #{after.inspect}")
        assert(after[:capsules] <= baseline[:capsules] + 4,
               "capsules were not physically purged: #{baseline.inspect} -> #{after.inspect}")
        assert(GC.stress == false, 'test stress cleanup failed')
      end
      assert(heap.count { |item| item } == 80_000, 'live heap fixture was lost')
      puts 'PASS lazy-sweep completion, GC setting restoration, native/capsule reclamation'
    ensure
      GC.stress = false
      originally_disabled ? GC.disable : GC.enable
      GC.stress = original_stress
    end
  end
end

begin
  NativeGCLazySweepRegression.run
  File.write('gc-results.txt', "ALL_OK\n")
  puts 'ALL_OK'
  exit(0)
rescue StandardError => error
  File.write('gc-results.txt', "#{error.class}: #{error.message}\n")
  puts "FAIL #{error.class}: #{error.message}"
  exit(1)
end

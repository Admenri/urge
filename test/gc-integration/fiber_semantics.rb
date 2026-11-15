# Run in the real engine, or directly with the pinned CRuby for a baseline.
# Special variables are LEP-local, so read them HERE, not in an assertion helper.
$stdout.sync = true

def fiber_check(condition, message)
  raise message unless condition
end

def exercise_fiber_semantics(switch)
  /creator-(\w+)/ =~ 'creator-original'
  creator_match = $~
  creator_line = $_ = 'creator-line'
  steps = []
  main = Fiber.current
  fiber = Fiber.new(blocking: true, storage: {fixture: 9}) do |*args, **keywords, &block|
    fiber_check([$~, $1, $_] == [nil, nil, nil], "#{switch}: inherited creator special variables")
    fiber_check([args, keywords, block.nil?, Fiber[:fixture], Fiber.current.blocking?] ==
                [[1, {positional: 2}], {keyword: 3}, true, 9, true], 'Fiber forwarding/storage changed')
    /inner-(\w+)/ =~ 'inner-first'
    inner_match = $~
    inner_line = $_ = 'inner-line'
    begin
      value = switch == :resume ? Fiber.yield(:ready) : main.transfer(:ready)
      fiber_check(value == :again, 'Fiber switch return changed')
      fiber_check($~.equal?(inner_match) && $1 == 'first' && $_.equal?(inner_line),
                  "#{switch}: creator overwrote suspended Fiber special variables")
      # A nested Fiber has its OWN root even though its block captures this LEP.
      nested = Fiber.new(storage: nil) do
        fiber_check([$~, $1, $_] == [nil, nil, nil], 'nested Fiber inherited special variables')
        /nested-(\w+)/ =~ 'nested-value'
        $_ = 'nested-line'
        GC.start
        [$1, $_]
      end
      fiber_check(nested.resume == ['value', 'nested-line'], 'nested Fiber body changed')
      fiber_check($~.equal?(inner_match) && $1 == 'first' && $_.equal?(inner_line),
                  'nested Fiber wrote its creator special variables')
      /inner-(\w+)/ =~ 'inner-last'
      $_ = 'inner-final-line'
      :finished
    ensure
      fiber_check([$1, $_] == ['last', 'inner-final-line'], 'Fiber ensure lost root environment')
      steps << :ensure
    end
  end
  # A singleton ancestor already exists when the native resume/transfer bridge
  # executes. Its super must still reach the correct inherited Fiber entry.
  ancestry = Module.new do
    define_method(switch) do |*args, **keywords, &block|
      steps << :singleton
      super(*args, **keywords, &block)
    end
  end
  fiber.singleton_class.include(ancestry)
  fiber_check(fiber.public_send(switch, 1, {positional: 2}, keyword: 3) { :ignored } == :ready,
              'initial Fiber switch changed')
  fiber_check($~.equal?(creator_match) && $1 == 'original' && $_.equal?(creator_line),
              "#{switch}: Fiber wrote creator special variables")
  /creator-(\w+)/ =~ 'creator-changed'
  creator_match = $~
  creator_line = $_ = 'creator-changed-line'
  3.times do
    GC.start(full_mark: true, immediate_sweep: true)
    GC.compact if GC.respond_to?(:compact)
  end
  fiber_check(fiber.public_send(switch, :again) == :finished, 'Fiber normal return changed')
  fiber_check($~.equal?(creator_match) && $1 == 'changed' && $_.equal?(creator_line),
              "#{switch}: terminated Fiber changed creator special variables")
  fiber_check(steps == [:singleton, :singleton, :ensure] && !fiber.alive?,
              'singleton ancestry/ensure/lifecycle changed')
end

def fiber_from_departed_creator
  /departed-(\w+)/ =~ 'departed-creator'
  $_ = 'departed-line'
  Fiber.new do
    fiber_check([$~, $1, $_] == [nil, nil, nil], 'departed creator specials leaked')
    /departed-(\w+)/ =~ 'departed-fiber'
    $_ = 'departed-fiber-line'
    Fiber.yield(:departed_ready)
    GC.start
    GC.compact if GC.respond_to?(:compact)
    [$1, $_]
  end
end

def exercise_fiber_exits
  /outer-(\w+)/ =~ 'outer-value'
  outer_match = $~
  outer_line = $_ = 'outer-line'
  steps = []
  error = begin
    Fiber.new do
      fiber_check([$~, $1, $_] == [nil, nil, nil], 'exception Fiber inherited specials')
      /error-(\w+)/ =~ 'error-value'
      $_ = 'error-line'
      begin
        raise ArgumentError, 'ordinary fiber error'
      ensure
        fiber_check([$1, $_] == ['value', 'error-line'], 'exception ensure lost Fiber specials')
        steps << :error_ensure
      end
    end.resume
  rescue Exception => caught
    [caught.class, caught.message]
  end
  fiber_check(error == [ArgumentError, 'ordinary fiber error'], 'ordinary Fiber exception changed')
  fiber_check(Fiber.new do
    begin
      catch(:user_tag) { throw :user_tag, :thrown }
    ensure
      steps << :throw_ensure
    end
  end.resume == :thrown, 'user catch/throw changed')
  cross_fiber = begin
    catch(:creator_tag) { Fiber.new { throw :creator_tag }.resume }
  rescue Exception => caught
    caught.class
  end
  fiber_check(cross_fiber == UncaughtThrowError, 'user throw crossed Fiber tag chains')
  body = ->(number, keyword:) do
    fiber_check([$~, $1, $_] == [nil, nil, nil], 'lambda Fiber inherited specials')
    return [number, keyword]
  end
  fiber_check(Fiber.new(&body).resume(4, keyword: 5) == [4, 5], 'lambda return/kwargs changed')
  broken = begin
    Fiber.new { break :invalid_break }.resume
  rescue Exception => caught
    caught.class
  end
  fiber_check(broken == LocalJumpError, 'Fiber break semantics changed')
  fiber_check(steps == [:error_ensure, :throw_ensure], 'Fiber nonlocal-exit ensures changed')
  fiber_check($~.equal?(outer_match) && $1 == 'value' && $_.equal?(outer_line),
              'Fiber nonlocal exit changed creator special variables')
  invalid = begin
    Fiber.new
  rescue Exception => caught
    caught.class
  end
  fiber_check(invalid == ArgumentError, 'Fiber.new without a body changed')
end

begin
  exercise_fiber_semantics(:resume)
  exercise_fiber_semantics(:transfer)
  exercise_fiber_exits
  departed = fiber_from_departed_creator
  GC.start
  GC.compact if GC.respond_to?(:compact)
  fiber_check(departed.resume == :departed_ready, 'departed creator Fiber first entry changed')
  GC.start
  GC.compact if GC.respond_to?(:compact)
  fiber_check(departed.resume == ['fiber', 'departed-fiber-line'], 'original root EP lifetime changed')
  # Top-level and two siblings using the SAME original Proc/LEP must still
  # have three independent svar stores, not copied snapshots of one alias.
  /top-(\w+)/ =~ 'top-creator'
  top_match = $~
  top_line = $_ = 'top-line'
  shared_body = proc do |label|
    fiber_check([$~, $1, $_] == [nil, nil, nil], 'sibling Fiber inherited specials')
    /sibling-(\w+)/ =~ "sibling-#{label}"
    own_match = $~
    own_line = $_ = label
    Fiber.yield(label)
    fiber_check($~.equal?(own_match) && $1 == label && $_.equal?(own_line),
                'sibling Fibers shared a svar store')
    label
  end
  first = Fiber.new(&shared_body)
  second = Fiber.new(&shared_body)
  fiber_check(first.resume('first') == 'first' && second.resume('second') == 'second',
              'sibling Fiber first resume changed')
  GC.start
  GC.compact if GC.respond_to?(:compact)
  fiber_check(first.resume == 'first' && second.resume == 'second', 'sibling Fiber completion changed')
  fiber_check($~.equal?(top_match) && $1 == 'creator' && $_.equal?(top_line),
              'sibling Fibers changed top-level creator specials')
  File.write('gc-results.txt', "ALL_OK\n")
  puts 'ALL_OK'
rescue Exception => error
  File.write('gc-results.txt', "FAIL #{error.class}: #{error.message}\n#{error.backtrace.join("\n")}\n")
  raise
end

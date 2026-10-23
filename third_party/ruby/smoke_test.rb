# frozen_string_literal: true
#
# smoke_test.rb -- the only test kept in this tree.
#
# It exercises the parts of the interpreter that a core-only (no C extension)
# build must get right: the parser, the VM, bignums, strings and encodings,
# regular expressions, fibers, exceptions, marshal, IO and the GC.
#
# Run it with:   ./ruby -I lib smoke_test.rb      (or: ctest)

$failures = 0
$checks = 0

def check(name)
  $checks += 1
  ok = yield
  if ok
    print "."
  else
    $failures += 1
    puts "\nFAIL: #{name}"
  end
rescue Exception => e
  $failures += 1
  puts "\nERROR: #{name}: #{e.class}: #{e.message}"
end

def eq(name, expected, actual)
  check(name) { expected == actual }
end

puts "Ruby #{RUBY_VERSION} (#{RUBY_PLATFORM})"

# --- integers --------------------------------------------------------------
eq("small int", 4, 2 + 2)
eq("bignum", 12_345_678_901_234_567_890 * 2, 24_691_357_802_469_135_780)
eq("bignum digits", 20, (2**64).to_s.size)
eq("integer division", -4, -7 / 2)   # Ruby floors towards -infinity
eq("modulo", 1, -7 % 2)
eq("pow", 1024, 2**10)
eq("bit ops", 8, 0b1100 & 0b1011)
eq("gcd", 6, 54.gcd(24))
eq("float", 3.0, 1.5 + 1.5)

# --- strings and encodings -------------------------------------------------
s = "héllo, 世界"
eq("utf-8 length", 9, s.length)
eq("utf-8 bytes", 14, s.bytesize)
eq("encoding", "UTF-8", s.encoding.name)
eq("upcase", "HÉLLO, 世界", s.upcase)
eq("slice", "世", s[-2])
eq("split", %w[a b c], "a,b,c".split(","))
eq("format", "007", format("%03d", 7))
eq("byteslice", "h", "hello".byteslice(0))

# --- arrays / hashes -------------------------------------------------------
a = [3, 1, 2]
eq("sort", [1, 2, 3], a.sort)
eq("sum", 6, a.sum)
eq("map", [6, 2, 4], a.map { |x| x * 2 })
eq("each_slice", [[3, 1], [2]], a.each_slice(2).to_a)
h = { a: 1, b: 2 }
eq("hash", 2, h[:b])
eq("hash to_a", [[:a, 1], [:b, 2]], h.to_a)
eq("group_by", { 1 => [3, 1], 0 => [2] }, [3, 1, 2].group_by { |x| x % 2 })
eq("reduce", 6, a.reduce(:+))

# --- regexp ----------------------------------------------------------------
eq("regexp", "123", "abc123def"[/\d+/])
eq("regexp unicode", "世界", s[/\p{Han}+/])
eq("named capture", "42", /(?<n>\d+)/.match("v42")[:n])
eq("gsub", "a-b-c", "a b c".gsub(" ", "-"))
eq("casecmp", true, "ABC".casecmp?("abc"))

# --- ranges / enumerators --------------------------------------------------
eq("range", [1, 2, 3], (1..3).to_a)
eq("step", [0, 2, 4], (0..4).step(2).to_a)
eq("lazy", [4, 16], (1..Float::INFINITY).lazy.map { |x| x * x }.select(&:even?).first(2))
enum = [1, 2].each
eq("enumerator", 1, enum.next)
eq("inject", 3, [1, 2].inject(:+))

# --- objects ---------------------------------------------------------------
Point = Struct.new(:x, :y) do
  def norm2 = x * x + y * y
end
eq("struct", 25, Point.new(3, 4).norm2)
eq("symbol", :sym, "sym".to_sym)
eq("freeze", true, "x".freeze.frozen?)
eq("singleton", 7, Object.new.tap { |o| def o.seven = 7 }.seven)
eq("comparable", true, 3.between?(1, 5))

class Base
  def greet = "base"
end
class Derived < Base
  def greet = "derived:" + super
end
eq("inheritance", "derived:base", Derived.new.greet)

module M
  def self.hello = "hi"
end
eq("module", "hi", M.hello)

# --- exceptions ------------------------------------------------------------
check("raise/rescue") do
  begin
    raise ArgumentError, "boom"
  rescue ArgumentError => e
    e.message == "boom"
  end
end
check("ensure") do
  ran = false
  begin
    raise "x"
  rescue
    nil
  ensure
    ran = true
  end
  ran
end
eq("retry-free rescue else", :ok, (begin; 1; rescue; :bad; else; :ok; end))

# --- procs and blocks ------------------------------------------------------
add = ->(x, y) { x + y }
eq("lambda", 5, add.call(2, 3))
def kw(a:, b: 2) = a + b
eq("keywords", 3, kw(a: 1))
def block_given_test = block_given?
eq("block_given?", true, block_given_test { })
eq("proc curry", 6, ->(a, b) { a * b }.curry[2][3])

# --- fibers ----------------------------------------------------------------
f = Fiber.new do |x|
  y = Fiber.yield(x + 1)
  y * 2
end
eq("fiber resume", 2, f.resume(1))
eq("fiber finish", 10, f.resume(5))

# --- numeric tower ---------------------------------------------------------
eq("rational", Rational(1, 2), 1r / 2)
eq("complex", Complex(0, 1), Complex(0, 1))
eq("to_s rational", "1/3", (1r / 3).to_s)

# --- time ------------------------------------------------------------------
t = Time.at(0).utc
eq("time", 1970, t.year)
eq("strftime", "1970-01-01", t.strftime("%Y-%m-%d"))

# --- marshal ---------------------------------------------------------------
data = Marshal.dump([1, "two", :three, { four: 4.0 }])
eq("marshal roundtrip", [1, "two", :three, { four: 4.0 }], Marshal.load(data))

# --- io --------------------------------------------------------------------
require "tmpdir"
Dir.mktmpdir do |dir|
  path = File.join(dir, "sample.txt")
  File.write(path, "line1\nline2\n")
  eq("io read", "line1\nline2\n", File.read(path))
  eq("io lines", 2, File.readlines(path).size)
  eq("file exist", true, File.exist?(path))
end

# --- gc --------------------------------------------------------------------
check("gc") do
  GC.start
  GC.stat[:heap_live_slots].is_a?(Integer)
end

# --- warnings / syntax -----------------------------------------------------
check("eval") { eval("1 + 1") == 2 }
check("__method__") { (def whoami = __method__; whoami == :whoami) }

puts
if $failures.zero?
  puts "OK: #{$checks} checks passed"
  exit 0
else
  puts "FAILED: #{$failures} of #{$checks} checks"
  exit 1
end

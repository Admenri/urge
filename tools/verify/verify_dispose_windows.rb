# Repro for the two-node windows, the shape which reaches a sibling node.
#
# WindowXP is built from two nodes: the window itself and a WindowXPAbove which
# draws the cursor and the contents. The above node is a sibling of the window
# in the list of their common parent, so it is reached by a frame on its own and
# a dispose which misses it leaves a node drawn against released resources.
#
# A dispose of the window is expected to take the above node with it, because
# the window owns the only reference to it. The test disposes the window and
# runs frames, which is where a leaked above node would surface.

def check(label, expected, actual)
  ok = expected == actual
  STDOUT.puts("#{ok ? 'ok  ' : 'FAIL'}  #{label}: expected=#{expected.inspect} actual=#{actual.inspect}")
  STDOUT.flush
  ok
end

results = []

begin
  window = Window.new(40, 40, 200, 140)
  window.contents = Bitmap.new(64, 64)
  window.contents.fill_rect(0, 0, 64, 64, Color.new(0, 255, 0, 255))
  window.openness = 255
  Graphics.update

  window.dispose
  results << check("window disposed", true, window.disposed?)

  # The frames which used to reach the above node, whose window had already
  # released the buffers it writes during its prepare stage.
  12.times { Graphics.update }
  results << check("frames after window dispose survived", true, true)

  # A window which is never disposed still draws, the control of the test.
  live = Window.new(10, 10, 120, 90)
  live.openness = 255
  Graphics.update
  results << check("live window still draws", true, true)

  live.dispose
  6.times { Graphics.update }
  results << check("frames after second window dispose survived", true, true)

  # A tilemap takes the same two-node shape in its VX form.
  tilemap = Tilemap.new
  tilemap.dispose
  6.times { Graphics.update }
  results << check("frames after tilemap dispose survived", true, true)
rescue => error
  STDOUT.puts("FAIL  raised #{error.class}: #{error.message}")
  STDOUT.puts(error.backtrace.first(8).join("\n"))
  STDOUT.flush
  results << false
end

failed = results.count(false)
STDOUT.puts("=== #{results.count(true)} passed, #{failed} failed ===")
STDOUT.flush

# Repro for "a disposed Viewport keeps taking part in Graphics.update".
#
# The node tree of a frame is a doubly linked list per parent held by
# DrawableSet, and Dispose() of a Node is supposed to unlink it. The bug was
# that every Node subclass overrides DisposeObject() to release its own
# resources without chaining to Node::DisposeObject(), which is where the
# unlink lived, so a disposed node stayed in the list of its parent and the
# next frame dispatched its prepare and its draw against released resources.
#
# This script disposes a Viewport with a Sprite inside it, runs frames, and
# prints whether the scene is still reachable and whether the frame survives.

def check(label, expected, actual)
  ok = expected == actual
  STDOUT.puts("#{ok ? 'ok  ' : 'FAIL'}  #{label}: expected=#{expected.inspect} actual=#{actual.inspect}")
  STDOUT.flush
  ok
end

results = []

begin
  # A viewport with a sprite inside it, drawn by a frame before the dispose so
  # the scene is reachable and the list of the root holds it.
  viewport = Viewport.new(20, 20, 160, 120)
  sprite = Sprite.new(viewport)
  bitmap = Bitmap.new(32, 32)
  bitmap.fill_rect(0, 0, 32, 32, Color.new(255, 0, 0, 255))
  sprite.bitmap = bitmap
  sprite.x = 16
  sprite.y = 16

  Graphics.update

  # The dispose under test: the viewport and the sprite inside it.
  viewport.dispose
  sprite.dispose

  results << check("viewport disposed?", true, viewport.disposed?)
  results << check("sprite disposed?", true, sprite.disposed?)

  # The frames after the dispose are the ones which used to crash: the render
  # walked the list of the root, reached the disposed viewport and wrote into
  # the buffers its dispose had released.
  10.times { Graphics.update }

  results << check("frames after dispose survived", true, true)

  # The scene has to be gone from the tree: a snapshot of the screen must not
  # contain the red quad the sprite drew before the dispose.
  snapshot = Graphics.snap_to_bitmap
  results << check("snapshot is not the disposed content", true,
                   snapshot.get_pixel(24, 24).red == 0)

  # A viewport which is never disposed still draws, which is the control of
  # the test: the fix must not have disabled the list altogether.
  live = Viewport.new(0, 0, 64, 64)
  live_sprite = Sprite.new(live)
  live_bitmap = Bitmap.new(32, 32)
  live_bitmap.fill_rect(0, 0, 32, 32, Color.new(0, 255, 0, 255))
  live_sprite.bitmap = live_bitmap
  Graphics.update

  snapshot = Graphics.snap_to_bitmap
  results << check("live sprite still draws", 255,
                   snapshot.get_pixel(8, 8).green)

  live.dispose
  live_sprite.dispose
  5.times { Graphics.update }

  results << check("frames after second dispose survived", true, true)
rescue => error
  STDOUT.puts("FAIL  raised #{error.class}: #{error.message}")
  STDOUT.puts(error.backtrace.first(8).join("\n"))
  STDOUT.flush
  results << false
end

failed = results.count(false)
STDOUT.puts("=== #{results.count(true)} passed, #{failed} failed ===")
STDOUT.flush
exit(failed == 0 ? 0 : 1)

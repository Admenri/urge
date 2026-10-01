# Focused repro for the two shapes a dispose can take in a node tree.
#
# The bug this verifies is that a disposed Node stayed in the render list of
# its parent, so the next frame dispatched its prepare and its draw against the
# resources its dispose had released, which the backend reports as "invalid
# buffer".
#
# The two shapes below are the ones a caller writes most often:
#   A. the parent is disposed and the child is not, the way a viewport is
#      dropped while a sprite inside it is kept.
#   B. the child is disposed and the parent is not.
# Both have to leave the scene reachable by no frame.

def check(label, expected, actual)
  ok = expected == actual
  STDOUT.puts("#{ok ? 'ok  ' : 'FAIL'}  #{label}: expected=#{expected.inspect} actual=#{actual.inspect}")
  STDOUT.flush
  ok
end

def make_scene(parent)
  bitmap = Bitmap.new(32, 32)
  bitmap.fill_rect(0, 0, 32, 32, Color.new(255, 0, 0, 255))
  sprite = Sprite.new(parent)
  sprite.bitmap = bitmap
  sprite.x = 16
  sprite.y = 16
  [sprite, bitmap]
end

results = []

begin
  # ---- A: the parent is disposed, the child is kept ----------------------
  viewport = Viewport.new(20, 20, 160, 120)
  sprite_a, bitmap_a = make_scene(viewport)
  Graphics.update

  viewport.dispose
  results << check("A parent disposed", true, viewport.disposed?)
  results << check("A child still owned", false, sprite_a.disposed?)

  # The frames which used to reach the disposed viewport through the list of
  # the root and write into its released uniform buffers.
  10.times { Graphics.update }

  results << check("A frames survived", true, true)
  results << check("A child left the screen", 0,
                   Graphics.snap_to_bitmap.get_pixel(24, 24).red)

  # The child is disposed after the parent, which has to be safe on its own.
  sprite_a.dispose
  5.times { Graphics.update }
  results << check("A child disposed after parent survived", true, true)

  # ---- B: the child is disposed, the parent is kept ----------------------
  viewport_b = Viewport.new(20, 20, 160, 120)
  sprite_b, bitmap_b = make_scene(viewport_b)
  Graphics.update

  sprite_b.dispose
  results << check("B child disposed", true, sprite_b.disposed?)

  # A frame which reaches the kept viewport still has to skip the disposed
  # child instead of dispatching into the buffers it released.
  10.times { Graphics.update }

  results << check("B frames survived", true, true)
  results << check("B child left the screen", 0,
                   Graphics.snap_to_bitmap.get_pixel(24, 24).red)

  viewport_b.dispose
  5.times { Graphics.update }
  results << check("B parent disposed after child survived", true, true)
rescue => error
  STDOUT.puts("FAIL  raised #{error.class}: #{error.message}")
  STDOUT.puts(error.backtrace.first(8).join("\n"))
  STDOUT.flush
  results << false
end

failed = results.count(false)
STDOUT.puts("=== #{results.count(true)} passed, #{failed} failed ===")
STDOUT.flush

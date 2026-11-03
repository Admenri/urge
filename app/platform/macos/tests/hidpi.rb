# Prepended to smoke.rb by --hidpi. Window APIs must stay in macOS points even
# when the Metal drawable has twice as many pixels along each axis.
$stdout.sync = true

def settle_window
  5.times { Graphics.update; Input.update }
end

def check_window(width, height)
  settle_window
  rect = URGE.get_window_rect
  raise "window points #{rect.width}x#{rect.height}, expected #{width}x#{height}" unless
    rect.width == width && rect.height == height
end

original_width, original_height = Graphics.width, Graphics.height
mode = File.read('hidpi_mode').strip
check_window(original_width, original_height)
Graphics.resize_screen(400, 300)
if mode == 'None'
  check_window(400, 300)
elsif mode == 'Ratio'
  settle_window
  rect = URGE.get_window_rect
  raise 'aspect constraint lost after resize_screen' unless (rect.width.to_f / rect.height - 4.0 / 3.0).abs < 0.01
else
  check_window(original_width, original_height)
end
raise 'DPI changed game resolution' unless Graphics.width == 400 && Graphics.height == 300

# move_window uses window coordinates too. Resizing the window must not change
# the game's bitmap size, even when the presentation scales to a Retina buffer.
URGE.move_window(800, 600)
check_window(800, 600)
raise 'window resize changed game resolution' unless Graphics.width == 400 && Graphics.height == 300
snapshot = Graphics.snap_to_bitmap
raise 'snapshot dimensions use backing pixels' unless snapshot.width == 400 && snapshot.height == 300
snapshot.dispose

Graphics.resize_screen(original_width, original_height)
URGE.move_window(original_width, original_height)
check_window(original_width, original_height)
puts "HIDPI_PASS mode=#{mode} logical=#{Graphics.width}x#{Graphics.height}"

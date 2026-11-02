# Runs inside the actual Ruby/Wasm engine; run twice to check persisted saves.
$stdout.sync = true
raise "wrong runtime: #{RUBY_VERSION}" unless RUBY_VERSION.start_with?('4.')

previous = File.exist?('Smoke.rvdata2') ? load_data('Smoke.rvdata2') : 0
bitmap = Bitmap.new(160, 100)
bitmap.fill_rect(0, 0, 160, 100, Color.new(40, 120, 200))
bitmap.draw_text(0, 45, 160, 40, 'macOS WebGPU', 1)
sprite = Sprite.new
sprite.bitmap = bitmap
sprite.x = 20
sprite.y = 20
asset = Bitmap.new('Graphics/check') # Exercises lazy loading, not preloading.
raise 'asset decode failed' unless asset.width == 1 && asset.height == 1

120.times { Graphics.update; Input.update }
snapshot = Graphics.snap_to_bitmap
pixel = snapshot.get_pixel(25, 25)
actual = [pixel.red, pixel.green, pixel.blue]
expected = [40, 120, 200]
raise "render readback mismatch: #{actual}" unless actual.zip(expected).all? { |a, b| (a - b).abs <= 1 }
save_data(previous + 1, 'Smoke.rvdata2')
raise 'save readback failed' unless load_data('Smoke.rvdata2') == previous + 1
puts "SMOKE_PASS ruby=#{RUBY_VERSION} frames=#{Graphics.frame_count} pixel=#{actual} persisted_run=#{previous + 1}"
exit

# Exercise host shutdown while rgss_stop never returns to its RB_FUNC wrapper.
$stdout.sync = true
sprite = Sprite.new
sprite.bitmap = Bitmap.new(32, 32)
sprite.bitmap.fill_rect(0, 0, 32, 32, Color.new(50, 160, 90))
at_exit do
  completed = File.exist?('shutdown-result.txt') &&
              File.read('shutdown-result.txt').include?('ENSURE_COMPLETE')
  File.write('gc-results.txt', completed ? "ALL_OK\n" : "ENSURE_INCOMPLETE\n")
  puts(completed ? 'ALL_OK' : 'ENSURE_INCOMPLETE')
end
begin
  rgss_stop
ensure
  sprite.dispose
  File.write('shutdown-step.txt', 'DISPOSE_COMPLETE')
  full = File.read('shutdown-step.txt') == 'DISPOSE_COMPLETE'
  raise 'ensure was interrupted' unless full
  File.write('shutdown-result.txt', 'ENSURE_COMPLETE')
end

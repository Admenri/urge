def check(condition, message)
  raise message unless condition
end

def wait_until(message, timeout: 2.0)
  deadline = Process.clock_gettime(Process::CLOCK_MONOTONIC) + timeout
  until yield
    check(Process.clock_gettime(Process::CLOCK_MONOTONIC) < deadline, message)
    sleep 0.01
  end
end

def wait_for_wrap(name)
  previous = Audio.public_send("#{name}_pos")
  wait_until("#{name} never looped") do
    current = Audio.public_send("#{name}_pos")
    wrapped = current < previous - 20
    previous = current
    wrapped
  end
end

begin
  Graphics.update
  Audio.bgm_play('bgm-loop.wav')
  Audio.bgs_play('bgs-loop.wav')
  wait_for_wrap(:bgm)
  wait_for_wrap(:bgs)

  Audio.bgm_play('bgm.wav', 100, 100, 400)
  wait_until('BGM did not start at its requested position') { Audio.bgm_pos > 450 }
  bgs = Audio.bgs_pos
  Audio.me_play('me.wav')
  saved = Audio.bgm_pos
  sleep 0.12
  check(Audio.bgm_pos == saved, 'ME did not freeze the BGM cursor')
  check(Audio.bgs_pos != bgs, 'ME also paused BGS')
  wait_until('BGM did not resume from its saved position') { Audio.bgm_pos > saved + 30 }

  Audio.me_play('me.wav')
  Audio.bgm_play('other.wav', 100, 100, 1200)
  check(Audio.bgm_pos == 1200, 'replacement BGM started during ME')
  Audio.bgm_play('bgm.wav', 100, 100, 1600)
  sleep 0.12
  check(Audio.bgm_pos == 1600, 'latest BGM replacement played before the ME ended')
  wait_until('latest replacement did not start after ME') { Audio.bgm_pos > 1650 }

  Audio.bgm_stop
  Audio.me_play('me.wav')
  Audio.bgm_play('other.wav', 100, 100, 900)
  check(Audio.bgm_pos == 900, 'new BGM played during ME with no prior BGM')
  Audio.me_stop
  wait_until('stopping ME did not release BGM') { Audio.bgm_pos > 950 }

  Audio.me_play('me.wav')
  sleep 0.35
  Audio.me_stop
  Audio.me_play('me.wav') # Replay immediately, without a Graphics.update.
  saved = Audio.bgm_pos
  sleep 0.30 # Longer than the remaining time on the stopped 0.55 s clip.
  check(Audio.bgm_pos == saved, 'same ME resumed instead of restarting after me_stop')
  wait_until('BGM did not resume after the restarted ME ended') { Audio.bgm_pos > saved + 30 }

  Audio.me_play('me.wav')
  saved = Audio.bgm_pos
  sleep 0.28
  Audio.me_play('me.wav')
  sleep 0.30
  check(Audio.bgm_pos == saved, 'replaying the same ME did not restart it')
  Audio.me_fade(80)
  wait_until('fading ME did not release BGM') { Audio.bgm_pos > saved + 30 }

  Audio.me_play('me.wav')
  Audio.bgm_stop
  Audio.me_stop
  check(Audio.bgm_pos == 0, 'stopped BGM resumed after ME')
  Audio.bgs_stop
  sleep 0.15

  16.times { |i| Audio.se_play("se#{i}.wav") }
  sleep 0.35 # SE 1 has ended; the oldest, SE 0, is still playing.
  Audio.se_play('se16.wav')
  sleep 0.25 # The new voice fills the free slot without evicting SE 0.
  Audio.se_play('se17.wav')
  sleep 2.4  # SE 17 evicts SE 0; the remaining voices end without looping.

  Audio.se_play('se-burst.wav', 20)
  sleep 0.7
  64.times { Audio.se_play('se-burst.wav', 20) }
  sleep 0.7

  Audio.se_play('se-burst.wav', 20)
  Audio.se_play('se-burst.wav', 20, 150) # Same path, deliberately different pitch.
  sleep 0.7

  # Retrigger across mix iterations as well: these must still play, and their
  # combined peaks must stay in range even when the single burst is coalesced.
  16.times do
    Audio.se_play('se-burst.wav')
    sleep 0.023
  end
  sleep 0.7

  Audio.se_play('se-release.wav')
  15.times { |i| Audio.se_play("se#{i + 2}.wav", 0) }
  sleep 0.20
  Audio.se_play('se17.wav', 0) # Steal a voice during its nonzero plateau.
  sleep 0.5

  File.write('audio-results.txt', "ALL_OK\n")
rescue => e
  File.write('audio-results.txt', "FAIL #{e.full_message}")
end

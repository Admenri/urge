#!/usr/bin/env python3
"""Check RGSS audio transitions and SE mixing on a real engine build."""
from __future__ import annotations

import argparse
import math
import os
from operator import mul
from pathlib import Path
import runpy
import shutil
import struct
import subprocess
import tempfile
import wave


ROOT = Path(__file__).resolve().parents[3]
pack_script = runpy.run_path(str(ROOT / "tools/tests/network/run.py"))["pack_script"]
TONE_RATE = 48000
# SDL's disk driver advertises the default CD-quality rate. The engine opens
# that device as float32, resampling from its internal 48 kHz mix.
CAPTURE_RATE = 44100
SE_COUNT = 18


def se_frequency(index: int) -> int:
    return 400 + 100 * index


def write_pcm(path: Path, frames: bytes) -> None:
    with wave.open(str(path), "wb") as output:
        output.setparams((1, 2, TONE_RATE, 0, "NONE", "not compressed"))
        output.writeframes(frames)


def write_tone(path: Path, frequency: int, seconds: float, amplitude: int = 1000) -> None:
    # All frequencies divide into whole cycles every 10 ms.
    frames = struct.pack("<480h", *(int(amplitude * math.sin(math.tau * frequency * i / TONE_RATE))
                                   for i in range(480)))
    write_pcm(path, frames * round(seconds * 100))


def check_se_bursts(samples: tuple[float, ...], block: int) -> None:
    sections: list[tuple[int, int]] = []
    for start in range(0, len(samples), block):
        end = start + block
        if max(map(abs, samples[start:end])) < 0.015:
            continue
        if sections and start - sections[-1][1] < CAPTURE_RATE // 10:
            sections[-1] = (sections[-1][0], end)
        else:
            sections.append((start, end))
    if len(sections) < 5:
        raise ValueError("missing separately captured SE bursts or eviction")
    reference, burst, pitched, staggered, eviction = (samples[a:b] for a, b in sections[-5:])
    reference_peak = max(map(abs, reference))
    burst_peak = max(map(abs, burst))
    staggered_peak = max(map(abs, staggered))
    jump = max(abs(a - b) for a, b in zip(eviction, eviction[1:]))
    print(f"SE capture: single={reference_peak:.3f}, burst={burst_peak:.3f}, "
          f"staggered={staggered_peak:.3f}, eviction step={jump:.3f}")
    if not 0.9 * reference_peak <= burst_peak <= 1.1 * reference_peak:
        raise ValueError("64 identical triggers should sound like one SE")
    window = pitched[3 * block:11 * block]
    for frequency in (100, 150):
        cosine = sum(sample * math.cos(math.tau * frequency * i / CAPTURE_RATE)
                     for i, sample in enumerate(window))
        sine = sum(sample * math.sin(math.tau * frequency * i / CAPTURE_RATE)
                   for i, sample in enumerate(window))
        if math.hypot(cosine, sine) < block * 0.4:
            raise ValueError(f"missing {frequency} Hz voice when triggering the same SE at two pitches")
    if len(staggered) < len(reference) + CAPTURE_RATE // 5 or not 0.8 < staggered_peak < 1.0:
        raise ValueError("staggered SE retriggers were dropped or clipped")
    if max(eviction) < 0.15 or len(eviction) > CAPTURE_RATE // 2 or jump > 0.03:
        raise ValueError("stealing a playing SE failed to end it smoothly")


def check_mix(path: Path) -> None:
    if not path.exists():
        raise ValueError("SDL's disk audio driver did not capture audio")
    capture = path.read_bytes()
    if len(capture) < CAPTURE_RATE * 8 or len(capture) % 8:
        raise ValueError("invalid 44.1 kHz stereo float32 audio capture")
    samples = struct.unpack(f"={len(capture) // 4}f", capture)[::2]
    block = CAPTURE_RATE // 50  # 20 ms, to distinguish simultaneous tone frequencies.
    bases = [([math.cos(math.tau * frequency * i / CAPTURE_RATE) for i in range(block)],
              [math.sin(math.tau * frequency * i / CAPTURE_RATE) for i in range(block)])
             for frequency in map(se_frequency, range(SE_COUNT))]
    wanted = {frozenset({0, *range(2, 17)}): "reuse of a finished non-head SE slot",
              frozenset(range(2, 18)): "eviction of the oldest when a 17th concurrent SE starts"}
    previous = frozenset()
    consecutive = 0
    for start in range(0, len(samples) - block + 1, block):
        window = samples[start:start + block]
        active = frozenset(i for i, (cosine, sine) in enumerate(bases)
                           if math.hypot(sum(map(mul, window, cosine)),
                                         sum(map(mul, window, sine))) > block * 0.007)
        # Ignore an isolated window that straddles a voice replacement.
        consecutive = consecutive + 1 if active == previous else 1
        previous = active
        if consecutive == 3:
            wanted.pop(active, None)
            if not wanted:
                break
    if wanted:
        raise ValueError("missing expected SE mix: " + ", ".join(wanted.values()))
    if max(map(abs, samples[-CAPTURE_RATE // 5:])) > 0.001:
        raise ValueError("SE did not end after one play")
    check_se_bursts(samples, block)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/audio-integration")
    parser.add_argument("--backend", default="opengl")
    args = parser.parse_args()
    engine = args.engine.resolve(strict=True)
    args.output.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="run-", dir=args.output.resolve()))
    print(f"Artifacts: {directory}", flush=True)
    target = directory / engine.name
    shutil.copy2(engine, target)
    for dll in engine.parent.glob("*.dll"):
        shutil.copy2(dll, directory / dll.name)
    (directory / "Data").mkdir()
    (directory / "Data/Scripts.rvdata2").write_bytes(pack_script(Path(__file__).with_name("probe.rb").read_bytes()))
    (directory / "Game.ini").write_text(
        "[Game]\nTitle=Audio integration\nRGSS=3\nScripts=Data/Scripts.rvdata2\n"
        f"RTP=\nRTP3=\n[GFX]\nBackend={args.backend}\n"
        "[Display]\nWidth=320\nHeight=240\nVSync=false\nBackgroundRunning=true\n",
        encoding="utf-8")
    for filename, frequency, seconds in [("bgm-loop", 100, 0.48), ("bgs-loop", 200, 0.32),
                                         ("bgm", 100, 4), ("other", 100, 4), ("me", 300, 0.55)]:
        write_tone(directory / f"{filename}.wav", frequency, seconds)
    for i in range(SE_COUNT):
        seconds = {0: 2.8, 1: 0.14}.get(i, 1.6)
        write_tone(directory / f"se{i}.wav", se_frequency(i), seconds)

    write_tone(directory / "se-burst.wav", 100, 0.40, amplitude=24000)
    # A held sample with smooth natural edges isolates any jump on eviction.
    release = [int(8192 * min(i, TONE_RATE - 1 - i, 480) / 480)
               for i in range(TONE_RATE)]
    write_pcm(directory / "se-release.wav", struct.pack(f"<{len(release)}h", *release))

    env = {key: value for key, value in os.environ.items()
           if not key.upper().startswith(("RUBY", "URGE_", "SDL_"))}
    env.update(SDL_AUDIO_DRIVER="disk", SDL_AUDIO_DEVICE_SAMPLE_FRAMES="512",
               URGE_CRASH_DIALOG="0", URGE_CRASH_DIR=str(directory))
    log_path = directory / "engine-output.txt"
    try:
        with log_path.open("wb") as log:
            child = subprocess.run([str(target)], cwd=directory, env=env, stdin=subprocess.DEVNULL,
                                   stdout=log, stderr=log, timeout=30)
        result = directory / "audio-results.txt"
        if child.returncode or not result.exists():
            raise ValueError(f"engine exited with code {child.returncode} or did not write a report")
        report = result.read_text(encoding="utf-8")
        if report.strip() != "ALL_OK":
            raise ValueError(report.strip())
        check_mix(directory / "sdlaudio.raw")
    except (ValueError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}")
        return 1
    print("ALL_OK: looping, BGM/ME transitions, SE voice limit and burst mixing")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

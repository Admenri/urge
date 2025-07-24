// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIOSERVICE_AUDIO_STREAM_H_
#define COMPONENTS_AUDIOSERVICE_AUDIO_STREAM_H_

#include "miniaudio.h"

#include "base/memory/allocator.h"

namespace audioservice {

class AudioStream {
 public:
  ~AudioStream();

  AudioStream(const AudioStream&) = delete;
  AudioStream& operator=(const AudioStream&) = delete;

  // Play audio stream from file in looping.
  // If the filename is different from the last one, it will reset the audio.
  ma_result Play(const base::String& filename,
                 int32_t volume,
                 int32_t pitch,
                 uint64_t pos = 0);
  void Stop();
  void Fade(int32_t time);
  uint64_t Pos();

  // Pause and resume audio stream.
  // Using for me watcher.
  bool IsPlaying();
  bool IsPausing();
  void Pause();
  void Resume();

  // Playback controller
  bool IsLooping();
  void SetLooping(bool looping);

 private:
  friend struct base::Allocator;
  AudioStream(ma_engine* engine);

  ma_engine* engine_;
  base::String filename_;
  ma_sound handle_;
  ma_uint64 cursor_;
  ma_bool32 looping_;
};

}  // namespace audioservice

#endif  //! COMPONENTS_AUDIOSERVICE_AUDIO_STREAM_H_

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audioservice/audio_stream.h"

namespace audioservice {

AudioStream::AudioStream(ma_engine* engine)
    : engine_(engine), handle_({}), cursor_(0) {}

AudioStream::~AudioStream() {
  ma_sound_uninit(&handle_);
}

void AudioStream::Play(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       uint64_t pos) {
  if (filename.empty())
    return;

  if (filename_ != filename) {
    // Reset audio stream handle
    ma_sound_uninit(&handle_);

    // Create new handle
    ma_uint32 sound_flags = MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_LOOPING;
    auto result = ma_sound_init_from_file(
        engine_, filename.c_str(), sound_flags, nullptr, nullptr, &handle_);

    if (result != MA_SUCCESS)
      return;
  }

  // Set sound handle attributes
  ma_sound_set_volume(&handle_, volume / 100.0f);
  ma_sound_set_pitch(&handle_, pitch / 100.0f);
  if (pos)
    ma_sound_seek_to_pcm_frame(&handle_, pos);

  // Start if need
  ma_sound_start(&handle_);
}

void AudioStream::Stop() {
  ma_sound_stop(&handle_);
}

void AudioStream::Fade(int32_t time) {
  ma_sound_stop_with_fade_in_milliseconds(&handle_, time);
}

uint64_t AudioStream::Pos() {
  uint64_t cursor;
  ma_sound_get_cursor_in_pcm_frames(&handle_, &cursor);
  return cursor;
}

void AudioStream::Pause() {
  ma_sound_get_cursor_in_pcm_frames(&handle_, &cursor_);
  ma_sound_stop(&handle_);
}

void AudioStream::Resume() {
  ma_sound_seek_to_pcm_frame(&handle_, cursor_);
  ma_sound_start(&handle_);
}

}  // namespace audioservice

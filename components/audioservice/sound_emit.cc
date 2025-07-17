// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audioservice/sound_emit.h"

#include "base/debug/logging.h"

namespace audioservice {

SoundEmit::SoundEmit(ma_engine* engine) : engine_(engine) {}

SoundEmit::~SoundEmit() {
  Stop();
}

void SoundEmit::Play(const base::String& filename,
                     int32_t volume,
                     int32_t pitch) {
  // Clear queued sounds if the queue is too large.
  if (sound_queue_.size() >= 10) {
    auto* invalid_voice = sound_queue_.front();
    sound_queue_.pop();

    ma_sound_uninit(invalid_voice);
    delete invalid_voice;
  }

  // Create sound playing handle.
  ma_sound* sound_handle = new ma_sound;
  if (ma_sound_init_from_file(engine_, filename.c_str(),
                              MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM,
                              nullptr, nullptr, sound_handle) == MA_SUCCESS) {
    // Push in queue
    sound_queue_.push(sound_handle);

    // Setup handle
    ma_sound_set_volume(sound_handle, volume / 100.0f);
    ma_sound_set_pitch(sound_handle, pitch / 100.0f);

    // Start sound playing
    ma_sound_start(sound_handle);
  } else {
    ma_sound_uninit(sound_handle);
    delete sound_handle;
  }
}

void SoundEmit::Stop() {
  while (!sound_queue_.empty()) {
    auto* sound_handle = sound_queue_.front();
    sound_queue_.pop();

    ma_sound_uninit(sound_handle);
    delete sound_handle;
  }
}

}  // namespace audioservice

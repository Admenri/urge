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

ma_result SoundEmit::Play(const base::String& filename,
                          int32_t volume,
                          int32_t pitch) {
  // Clear queued sounds if the queue is too large.
  if (sound_queue_.size() >= 20) {
    auto* invalid_voice = sound_queue_.front();
    sound_queue_.pop();

    if (!ma_sound_is_playing(invalid_voice)) {
      ma_sound_uninit(invalid_voice);
      delete invalid_voice;
    } else {
      // Requeued sound handle
      sound_queue_.push(invalid_voice);
    }
  }

  // Create sound playing handle.
  ma_sound* sound_handle = base::Allocator::New<ma_sound>();
  auto result = ma_sound_init_from_file(
      engine_, filename.c_str(), MA_SOUND_FLAG_ASYNC | MA_SOUND_FLAG_STREAM,
      nullptr, nullptr, sound_handle);
  if (result == MA_SUCCESS) {
    // Push in queue
    sound_queue_.push(sound_handle);

    // Setup handle
    ma_sound_set_volume(sound_handle, volume / 100.0f);
    ma_sound_set_pitch(sound_handle, pitch / 100.0f);

    // Start sound playing
    ma_sound_start(sound_handle);

    return MA_SUCCESS;
  }

  // When loading failed
  ma_sound_uninit(sound_handle);
  base::Allocator::Delete(sound_handle);

  return result;
}

void SoundEmit::Stop() {
  while (!sound_queue_.empty()) {
    auto* sound_handle = sound_queue_.front();
    sound_queue_.pop();

    ma_sound_uninit(sound_handle);
    base::Allocator::Delete(sound_handle);
  }
}

}  // namespace audioservice

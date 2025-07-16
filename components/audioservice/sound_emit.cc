// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audioservice/sound_emit.h"

namespace audioservice {

SoundEmit::SoundEmit(ma_engine* engine) : engine_(engine) {
  ma_sound_group_init(engine_, MA_SOUND_FLAG_STREAM, nullptr, &sound_group_);
}

SoundEmit::~SoundEmit() {
  ma_sound_group_uninit(&sound_group_);
}

void SoundEmit::Play(const base::String& filename,
                     int32_t volume,
                     int32_t pitch) {
  auto* res_manager = ma_engine_get_resource_manager(engine_);
  ma_resource_manager_data_source* target_source = nullptr;

  if (auto it = source_cache_.find(filename); it == source_cache_.end()) {
    // If the source does not exist, create a new one.
    ma_resource_manager_data_source data_source;
    ma_resource_manager_data_source_init(
        res_manager, filename.c_str(),
        MA_RESOURCE_MANAGER_DATA_SOURCE_FLAG_ASYNC, nullptr, &data_source);

    source_cache_.emplace(filename, std::move(data_source));
  } else {
    // If the source already exists, use it.
    target_source = &it->second;
  }

  // Create sound playing handle.
  ma_sound sound_handle;
  ma_sound_init_from_data_source(engine_, target_source, MA_SOUND_FLAG_ASYNC,
                                 &sound_group_, &sound_handle);

  // Clear queued sounds if the queue is too large.
  if (sound_queue_.size() >= 10) {
    auto* invalid_voice = &sound_queue_.front();
    ma_sound_stop(invalid_voice);
    ma_sound_uninit(invalid_voice);
    sound_queue_.pop();
  }

  // Push in queue
  auto& playing_handle = sound_queue_.emplace(std::move(sound_handle));

  // Setup handle
  ma_sound_set_volume(&playing_handle, volume / 100.0f);
  ma_sound_set_pitch(&playing_handle, pitch / 100.0f);

  // Start sound playing
  ma_sound_start(&playing_handle);
}

void SoundEmit::Stop() {
  ma_sound_group_stop(&sound_group_);
  while (!sound_queue_.empty()) {
    auto* sound_handle = &sound_queue_.front();
    ma_sound_stop(sound_handle);
    ma_sound_uninit(sound_handle);
    sound_queue_.pop();
  }
}

}  // namespace audioservice

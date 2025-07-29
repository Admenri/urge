// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIOSERVICE_SOUND_EMIT_H_
#define COMPONENTS_AUDIOSERVICE_SOUND_EMIT_H_

#include <queue>
#include <string>
#include <unordered_map>

#include "miniaudio.h"

namespace audioservice {

class SoundEmit {
 public:
  ~SoundEmit();

  SoundEmit(const SoundEmit&) = delete;
  SoundEmit& operator=(const SoundEmit&) = delete;

  ma_result Play(const std::string& filename, int32_t volume, int32_t pitch);
  void Stop();

 private:
  friend class AudioService;
  SoundEmit(ma_engine* engine);

  ma_engine* engine_;
  std::queue<ma_sound*> sound_queue_;
};

}  // namespace audioservice

#endif  //! COMPONENTS_AUDIOSERVICE_SOUND_EMIT_H_

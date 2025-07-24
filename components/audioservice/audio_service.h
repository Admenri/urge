// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUDIOSERVICE_AUDIO_SERVICE_H_
#define COMPONENTS_AUDIOSERVICE_AUDIO_SERVICE_H_

#include "SDL3/SDL_audio.h"

#include "components/filesystem/io_service.h"

namespace audioservice {

struct ServiceKernelData;
class AudioStream;
class SoundEmit;

class AudioService {
 public:
  ~AudioService();

  AudioService(const AudioService&) = delete;
  AudioService& operator=(const AudioService&) = delete;

  // Create audio service instance.
  // Return the wrapper of miniaudio engine.
  static base::OwnedPtr<AudioService> Create(filesystem::IOService* io_service);

  // Create audio stream instance.
  // Return the wrapper of miniaudio sound.
  base::OwnedPtr<AudioStream> CreateStream();

  // Create audio emitter instance.
  // Return the wrapper of miniaudio sound queue.
  base::OwnedPtr<SoundEmit> CreateEmitter();

  // Global volume control
  float GetVolume();
  void SetVolume(float volume);

  // Device pause control
  void PauseDevice();
  void ResumeDevice();

  // Raw device id
  SDL_AudioDeviceID GetDeviceID() const;

 private:
  friend struct base::Allocator;
  ServiceKernelData* kernel_;

  AudioService(ServiceKernelData* kernel_engine);
};

}  // namespace audioservice

#endif  // !COMPONENTS_AUDIOSERVICE_AUDIO_SERVICE_H_

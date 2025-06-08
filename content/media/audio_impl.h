// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_MEDIA_AUDIO_IMPL_H_
#define CONTENT_MEDIA_AUDIO_IMPL_H_

#include "SDL3/SDL_audio.h"

#include "soloud.h"
#include "soloud_wav.h"

#include <list>
#include <queue>
#include <unordered_map>

#include "base/worker/thread_worker.h"
#include "components/filesystem/io_service.h"
#include "content/context/engine_object.h"
#include "content/profile/content_profile.h"
#include "content/profile/i18n_profile.h"
#include "content/public/engine_audio.h"

namespace content {

class AudioImpl : public Audio, public EngineObject {
 public:
  AudioImpl(ExecutionContext* execution_context);
  ~AudioImpl() override;

  AudioImpl(const AudioImpl&) = delete;
  AudioImpl& operator=(const AudioImpl&) = delete;

  SDL_AudioDeviceID& output_device() { return output_device_; }
  SDL_AudioStream*& soloud_stream() { return soloud_stream_; }
  SDL_AudioSpec& soloud_spec() { return soloud_spec_; }

  void CreateButtonGUISettings();

 public:
  void SetupMIDI(ExceptionState& exception_state) override;

  void BGMPlay(const base::String& filename,
               int32_t volume,
               int32_t pitch,
               int32_t pos,
               ExceptionState& exception_state) override;
  void BGMStop(ExceptionState& exception_state) override;
  void BGMFade(int32_t time, ExceptionState& exception_state) override;
  int32_t BGMPos(ExceptionState& exception_state) override;

  void BGSPlay(const base::String& filename,
               int32_t volume,
               int32_t pitch,
               int32_t pos,
               ExceptionState& exception_state) override;
  void BGSStop(ExceptionState& exception_state) override;
  void BGSFade(int32_t time, ExceptionState& exception_state) override;
  int32_t BGSPos(ExceptionState& exception_state) override;

  void MEPlay(const base::String& filename,
              int32_t volume,
              int32_t pitch,
              ExceptionState& exception_state) override;
  void MEStop(ExceptionState& exception_state) override;
  void MEFade(int32_t time, ExceptionState& exception_state) override;

  void SEPlay(const base::String& filename,
              int32_t volume,
              int32_t pitch,
              ExceptionState& exception_state) override;
  void SEStop(ExceptionState& exception_state) override;

  void Reset(ExceptionState& exception_state) override;

 private:
  struct SlotInfo {
    base::OwnedPtr<SoLoud::Wav> source;
    base::String filename;
    SoLoud::handle play_handle = 0;
  };

  void InitAudioDeviceInternal();
  void DestroyAudioDeviceInternal();
  void MeMonitorInternal();

  void PlaySlotInternal(SlotInfo* slot,
                        const base::String& filename,
                        int32_t volume = 100,
                        int32_t pitch = 100,
                        double pos = 0,
                        bool loop = true);
  void StopSlotInternal(SlotInfo* slot);
  void FadeSlotInternal(SlotInfo* slot, int32_t time);
  void GetSlotPosInternal(SlotInfo* slot, double* out);

  void EmitSoundInternal(const base::String& filename,
                         int32_t volume = 100,
                         int32_t pitch = 100);
  void StopEmitInternal();

  void ResetInternal();

  filesystem::IOService* io_service_;
  I18NProfile* i18n_profile_;

  SoLoud::Soloud core_;
  SDL_AudioDeviceID output_device_;
  SDL_AudioStream* soloud_stream_;
  SDL_AudioSpec soloud_spec_;

  SlotInfo bgm_;
  SlotInfo bgs_;
  SlotInfo me_;
  std::unordered_map<base::String, base::OwnedPtr<SoLoud::Wav>> se_cache_;
  std::queue<SoLoud::handle> se_queue_;

  base::OwnedPtr<std::thread> me_watcher_;
  std::atomic_bool quit_flag_;

  base::OwnedPtr<base::ThreadWorker> audio_runner_;
};

}  // namespace content

#endif  //! CONTENT_MEDIA_AUDIO_IMPL_H_

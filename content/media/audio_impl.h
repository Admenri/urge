// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_MEDIA_AUDIO_IMPL_H_
#define CONTENT_MEDIA_AUDIO_IMPL_H_

#include "components/audioservice/audio_stream.h"
#include "components/audioservice/sound_emit.h"
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

  void CreateButtonGUISettings();

 public:
  void SetupMIDI(ExceptionState& exception_state) override;

  void BGMPlay(const base::String& filename,
               int32_t volume,
               int32_t pitch,
               uint64_t pos,
               ExceptionState& exception_state) override;
  void BGMStop(ExceptionState& exception_state) override;
  void BGMFade(int32_t time, ExceptionState& exception_state) override;
  uint64_t BGMPos(ExceptionState& exception_state) override;

  void BGSPlay(const base::String& filename,
               int32_t volume,
               int32_t pitch,
               uint64_t pos,
               ExceptionState& exception_state) override;
  void BGSStop(ExceptionState& exception_state) override;
  void BGSFade(int32_t time, ExceptionState& exception_state) override;
  uint64_t BGSPos(ExceptionState& exception_state) override;

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
  I18NProfile* i18n_profile_;

  base::OwnedPtr<audioservice::AudioStream> bgm_;
  base::OwnedPtr<audioservice::AudioStream> bgs_;
  base::OwnedPtr<audioservice::AudioStream> me_;
  base::OwnedPtr<audioservice::SoundEmit> se_;
};

}  // namespace content

#endif  //! CONTENT_MEDIA_AUDIO_IMPL_H_

// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_MEDIA_AUDIO_IMPL_H_
#define CONTENT_MEDIA_AUDIO_IMPL_H_

#include "components/filesystem/io_service.h"
#include "content/profile/content_profile.h"
#include "content/profile/i18n_profile.h"
#include "content/public/engine_audio.h"

namespace content {

class AudioImpl : public Audio {
 public:
  AudioImpl(ContentProfile* profile,
            filesystem::IOService* io_service,
            I18NProfile* i18n_profile);
  ~AudioImpl() override;

  AudioImpl(const AudioImpl&) = delete;
  AudioImpl& operator=(const AudioImpl&) = delete;

  void SetupMIDI(ExceptionState& exception_state) override;

  void BGMPlay(const std::string& filename,
               int32_t volume,
               int32_t pitch,
               int32_t pos,
               ExceptionState& exception_state) override;
  void BGMStop(ExceptionState& exception_state) override;
  void BGMFade(int32_t time, ExceptionState& exception_state) override;
  int32_t BGMPos(ExceptionState& exception_state) override;

  void BGSPlay(const std::string& filename,
               int32_t volume,
               int32_t pitch,
               int32_t pos,
               ExceptionState& exception_state) override;
  void BGSStop(ExceptionState& exception_state) override;
  void BGSFade(int32_t time, ExceptionState& exception_state) override;
  int32_t BGSPos(ExceptionState& exception_state) override;

  void MEPlay(const std::string& filename,
              int32_t volume,
              int32_t pitch,
              ExceptionState& exception_state) override;
  void MEStop(ExceptionState& exception_state) override;
  void MEFade(int32_t time, ExceptionState& exception_state) override;

  void SEPlay(const std::string& filename,
              int32_t volume,
              int32_t pitch,
              ExceptionState& exception_state) override;
  void SEStop(ExceptionState& exception_state) override;

  void Reset(ExceptionState& exception_state) override;

 private:
  ContentProfile* profile_;
  filesystem::IOService* io_service_;
  I18NProfile* i18n_profile_;
};

}  // namespace content

#endif  //! CONTENT_MEDIA_AUDIO_IMPL_H_

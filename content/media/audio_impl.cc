// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/media/audio_impl.h"

#include "imgui/imgui.h"

#include "content/context/execution_context.h"
#include "content/profile/command_ids.h"

namespace content {

AudioImpl::AudioImpl(ExecutionContext* execution_context)
    : EngineObject(execution_context),
      io_service_(execution_context->io_service),
      i18n_profile_(execution_context->i18n_profile) {
  if (execution_context->engine_profile->disable_audio)
    return;
}

AudioImpl::~AudioImpl() {}

void AudioImpl::CreateButtonGUISettings() {
  if (ImGui::CollapsingHeader(
          i18n_profile_->GetI18NString(IDS_SETTINGS_AUDIO, "Audio").c_str())) {
    // Set global volume
    float volume = 0.0f;
    ImGui::SliderFloat(
        i18n_profile_->GetI18NString(IDS_AUDIO_VOLUME, "Volume").c_str(),
        &volume, 0, 1);
  }
}

void AudioImpl::SetupMIDI(ExceptionState& exception_state) {
  // TODO: unsupport MIDI
  LOG(WARNING) << "[Content] Unsupport MIDI device setup.";
}

void AudioImpl::BGMPlay(const base::String& filename,
                        int32_t volume,
                        int32_t pitch,
                        int32_t pos,
                        ExceptionState& exception_state) {}

void AudioImpl::BGMStop(ExceptionState& exception_state) {}

void AudioImpl::BGMFade(int32_t time, ExceptionState& exception_state) {}

int32_t AudioImpl::BGMPos(ExceptionState& exception_state) {
  return 0;
}

void AudioImpl::BGSPlay(const base::String& filename,
                        int32_t volume,
                        int32_t pitch,
                        int32_t pos,
                        ExceptionState& exception_state) {}

void AudioImpl::BGSStop(ExceptionState& exception_state) {}

void AudioImpl::BGSFade(int32_t time, ExceptionState& exception_state) {}

int32_t AudioImpl::BGSPos(ExceptionState& exception_state) {
  return 0;
}

void AudioImpl::MEPlay(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       ExceptionState& exception_state) {}

void AudioImpl::MEStop(ExceptionState& exception_state) {}

void AudioImpl::MEFade(int32_t time, ExceptionState& exception_state) {}

void AudioImpl::SEPlay(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       ExceptionState& exception_state) {}

void AudioImpl::SEStop(ExceptionState& exception_state) {}

void AudioImpl::Reset(ExceptionState& exception_state) {}

}  // namespace content

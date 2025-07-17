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
      i18n_profile_(execution_context->i18n_profile) {
  if (!execution_context->audio_server)
    return;

  // Create slots
  bgm_ = execution_context->audio_server->CreateStream();
  bgs_ = execution_context->audio_server->CreateStream();
  me_ = execution_context->audio_server->CreateStream();
  se_ = execution_context->audio_server->CreateEmitter();

  // Set looping
  bgm_->SetLooping(true);
  bgs_->SetLooping(true);
  me_->SetLooping(false);

  // Setup watcher
  me_watcher_ = base::ThreadWorker::Create();
  me_watcher_->PostTask(base::BindOnce(&AudioImpl::MeThreadMonitorInternal,
                                       base::Unretained(this)));
}

AudioImpl::~AudioImpl() {}

void AudioImpl::CreateButtonGUISettings() {
  if (ImGui::CollapsingHeader(
          i18n_profile_->GetI18NString(IDS_SETTINGS_AUDIO, "Audio").c_str())) {
    // Set global volume
    float volume = context()->audio_server->GetVolume();
    ImGui::SliderFloat(
        i18n_profile_->GetI18NString(IDS_AUDIO_VOLUME, "Volume").c_str(),
        &volume, 0, 1);
    context()->audio_server->SetVolume(volume);
  }
}

void AudioImpl::SetupMIDI(ExceptionState& exception_state) {
  // TODO: unsupport MIDI
  LOG(WARNING) << "[Content] Unsupport MIDI device setup.";
}

void AudioImpl::BGMPlay(const base::String& filename,
                        int32_t volume,
                        int32_t pitch,
                        uint64_t pos,
                        ExceptionState& exception_state) {
  if (!bgm_)
    return;

  bgm_->Play(filename, volume, pitch, pos);
}

void AudioImpl::BGMStop(ExceptionState& exception_state) {
  if (!bgm_)
    return;

  bgm_->Stop();
}

void AudioImpl::BGMFade(int32_t time, ExceptionState& exception_state) {
  if (!bgm_)
    return;

  bgm_->Fade(time);
}

uint64_t AudioImpl::BGMPos(ExceptionState& exception_state) {
  if (!bgm_)
    return 0;

  return bgm_->Pos();
}

void AudioImpl::BGSPlay(const base::String& filename,
                        int32_t volume,
                        int32_t pitch,
                        uint64_t pos,
                        ExceptionState& exception_state) {
  if (!bgs_)
    return;

  bgs_->Play(filename, volume, pitch, pos);
}

void AudioImpl::BGSStop(ExceptionState& exception_state) {
  if (!bgs_)
    return;

  bgs_->Stop();
}

void AudioImpl::BGSFade(int32_t time, ExceptionState& exception_state) {
  if (!bgs_)
    return;

  bgs_->Fade(time);
}

uint64_t AudioImpl::BGSPos(ExceptionState& exception_state) {
  if (!bgs_)
    return 0;

  return bgs_->Pos();
}

void AudioImpl::MEPlay(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       ExceptionState& exception_state) {
  if (!me_)
    return;

  me_->Play(filename, volume, pitch, 0);
}

void AudioImpl::MEStop(ExceptionState& exception_state) {
  if (!me_)
    return;

  me_->Stop();
}

void AudioImpl::MEFade(int32_t time, ExceptionState& exception_state) {
  if (!me_)
    return;

  me_->Fade(time);
}

void AudioImpl::SEPlay(const base::String& filename,
                       int32_t volume,
                       int32_t pitch,
                       ExceptionState& exception_state) {
  if (!se_)
    return;

  se_->Play(filename, volume, pitch);
}

void AudioImpl::SEStop(ExceptionState& exception_state) {
  if (!se_)
    return;

  se_->Stop();
}

void AudioImpl::Reset(ExceptionState& exception_state) {
  if (bgm_)
    bgm_->Stop();
  if (bgs_)
    bgs_->Stop();
  if (me_)
    me_->Stop();
  if (se_)
    se_->Stop();
}

void AudioImpl::MeThreadMonitorInternal() {
  if (me_->IsPlaying() && bgm_->IsPlaying())
    bgm_->Pause();

  if (!me_->IsPlaying() && bgm_->IsPausing())
    bgm_->Resume();

  // Looping for watching
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  me_watcher_->PostTask(base::BindOnce(&AudioImpl::MeThreadMonitorInternal,
                                       base::Unretained(this)));
}

}  // namespace content

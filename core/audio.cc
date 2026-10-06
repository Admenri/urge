// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/audio.h"

#include <algorithm>

#include "core/audio_service.h"
#include "core/config.h"
#include "core/logger.h"

namespace urge {

namespace {

int32_t ClampVolume(int32_t volume) {
  return std::clamp(volume, 0, 100);
}

}  // namespace

Audio::Audio()
    : service_(AudioService::Create(Config::Get().audio.output_device)) {
  if (!service_)
    return;

  bgm_.reset(service_->CreateStream(AudioBus::kBGM));
  bgs_.reset(service_->CreateStream(AudioBus::kBGS));
  me_.reset(service_->CreateStream(AudioBus::kME));
  se_.reset(service_->CreateEmitter());

  bgm_->SetLooping(true);
  bgs_->SetLooping(true);
  me_->SetLooping(false);

  output_device_ = service_->output_device();
  ApplyVolumes();
}

Audio::~Audio() = default;

void Audio::SetupMIDI() {
  LOGGER_WARN("MIDI playback is not supported by the mixer; ignored");
}

void Audio::BGMPlay(std::string filename,
                    int32_t volume,
                    int32_t pitch,
                    float pos) {
  if (bgm_)
    bgm_->Play(filename, ClampVolume(volume), pitch, pos);
}

void Audio::BGMStop() {
  if (bgm_)
    bgm_->Stop();
}

void Audio::BGMFade(int32_t time) {
  if (bgm_)
    bgm_->Fade(time);
}

float Audio::BGMPos() {
  return bgm_ ? bgm_->Pos() : 0.0f;
}

void Audio::BGSPlay(std::string filename,
                    int32_t volume,
                    int32_t pitch,
                    float pos) {
  if (bgs_)
    bgs_->Play(filename, ClampVolume(volume), pitch, pos);
}

void Audio::BGSStop() {
  if (bgs_)
    bgs_->Stop();
}

void Audio::BGSFade(int32_t time) {
  if (bgs_)
    bgs_->Fade(time);
}

float Audio::BGSPos() {
  return bgs_ ? bgs_->Pos() : 0.0f;
}

void Audio::MEPlay(std::string filename, int32_t volume, int32_t pitch) {
  if (!me_)
    return;

  if (bgm_ && bgm_->IsPlaying())
    bgm_->Pause();

  me_->Play(filename, ClampVolume(volume), pitch);
}

void Audio::MEStop() {
  if (me_)
    me_->Stop();
}

void Audio::MEFade(int32_t time) {
  if (me_)
    me_->Fade(time);
}

void Audio::SEPlay(std::string filename, int32_t volume, int32_t pitch) {
  if (se_)
    se_->Play(filename, ClampVolume(volume), pitch);
}

void Audio::SEStop() {
  if (se_)
    se_->Stop();
}

ATTR_DEF(Audio, int32_t, MasterVolume) {
  auto& settings = Config::Get().audio;

  if (value.has_value()) {
    settings.master_volume = ClampVolume(*value);
    if (service_)
      service_->SetMasterVolume(settings.master_volume);
    return std::nullopt;
  }

  return settings.master_volume;
}

ATTR_DEF(Audio, int32_t, BGMVolume) {
  auto& settings = Config::Get().audio;

  if (value.has_value()) {
    settings.bgm_volume = ClampVolume(*value);
    if (service_)
      service_->SetBusVolume(AudioBus::kBGM, settings.bgm_volume);
    return std::nullopt;
  }

  return settings.bgm_volume;
}

ATTR_DEF(Audio, int32_t, BGSVolume) {
  auto& settings = Config::Get().audio;

  if (value.has_value()) {
    settings.bgs_volume = ClampVolume(*value);
    if (service_)
      service_->SetBusVolume(AudioBus::kBGS, settings.bgs_volume);
    return std::nullopt;
  }

  return settings.bgs_volume;
}

ATTR_DEF(Audio, int32_t, MEVolume) {
  auto& settings = Config::Get().audio;

  if (value.has_value()) {
    settings.me_volume = ClampVolume(*value);
    if (service_)
      service_->SetBusVolume(AudioBus::kME, settings.me_volume);
    return std::nullopt;
  }

  return settings.me_volume;
}

ATTR_DEF(Audio, int32_t, SEVolume) {
  auto& settings = Config::Get().audio;

  if (value.has_value()) {
    settings.se_volume = ClampVolume(*value);
    if (service_)
      service_->SetBusVolume(AudioBus::kSE, settings.se_volume);
    return std::nullopt;
  }

  return settings.se_volume;
}

std::vector<std::string> Audio::OutputDevices() {
  return AudioService::OutputDevices();
}

bool Audio::SetOutputDevice(std::string name) {
  if (!service_)
    return false;

  if (!service_->SetOutputDevice(name))
    return false;

  output_device_ = service_->output_device();

  Config::Get().audio.output_device = output_device_;
  return true;
}

void Audio::Update() {
  if (!bgm_)
    return;

  if (me_ && me_->IsPlaying()) {
    if (bgm_->IsPlaying())
      bgm_->Pause();
  } else if (bgm_->IsPausing()) {
    bgm_->Resume();
  }
}

void Audio::ApplyVolumes() {
  if (!service_)
    return;

  const auto& settings = Config::Get().audio;
  service_->SetMasterVolume(settings.master_volume);
  service_->SetBusVolume(AudioBus::kBGM, settings.bgm_volume);
  service_->SetBusVolume(AudioBus::kBGS, settings.bgs_volume);
  service_->SetBusVolume(AudioBus::kME, settings.me_volume);
  service_->SetBusVolume(AudioBus::kSE, settings.se_volume);
}

}  // namespace urge

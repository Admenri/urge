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

#include "core/audio/audio_stream.h"

#include <algorithm>

#include "core/audio/audio.h"
#include "core/audio/audio_service.h"
#include "core/common/exception.h"
#include "core/common/logger.h"

namespace urge {

namespace {

constexpr ma_uint64 kLoopPointEnd = ~static_cast<ma_uint64>(0);

ma_uint64 MillisecondsToFrames(float pos) {
  if (pos <= 0.0f)
    return 0;

  return static_cast<ma_uint64>(static_cast<double>(pos) *
                                AudioService::kEngineSampleRate / 1000.0);
}

float FramesToMilliseconds(ma_uint64 frames) {
  return static_cast<float>(static_cast<double>(frames) * 1000.0 /
                            AudioService::kEngineSampleRate);
}

}

AudioStream::AudioStream(URGE_EXCEPT, std::string filename) {
  auto* service = Audio::Get().service();
  if (!service) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "the audio service is not running.");
    return;
  }

  auto clip = std::make_unique<AudioClip>();
  if (!clip->Open(service, AudioBus::kSE, filename)) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "the audio file could not be opened: {}", filename);
    return;
  }
  clip_ = std::move(clip);
  ApplyState();
}

AudioStream::AudioStream(URGE_EXCEPT, std::vector<uint8_t> data, float length) {
  auto* service = Audio::Get().service();
  if (!service) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "the audio service is not running.");
    return;
  }

  auto clip = std::make_unique<AudioClip>();
  if (!clip->OpenMemory(service, AudioBus::kSE, std::move(data))) {
    exception_state.Throw(ExceptionState::kRGSSError,
                          "the audio track could not be opened.");
    return;
  }
  clip_ = std::move(clip);
  length_ = length > 0.0f ? length : -1.0f;
  ApplyState();
}

AudioStream::~AudioStream() = default;

void AudioStream::ApplyState() {
  if (!clip_)
    return;

  auto* service = Audio::Get().service();
  if (!service)
    return;

  const float volume = volume_;
  const float pan = pan_;
  const float pitch = pitch_;
  const bool loop = loop_;
  service->Post([clip = clip_.get(), volume, pan, pitch, loop] {
    ma_sound* sound = clip->sound();
    ma_sound_set_volume(sound, volume);
    ma_sound_set_pan(sound, pan);
    ma_sound_set_pitch(sound, pitch);
    ma_sound_set_looping(sound, loop ? MA_TRUE : MA_FALSE);
  });
}

void AudioStream::Start() {
  if (!clip_)
    return;

  if (auto* service = Audio::Get().service()) {
    service->Post([clip = clip_.get()] { ma_sound_start(clip->sound()); });
  }
}

void AudioStream::Stop() {
  if (!clip_)
    return;

  if (auto* service = Audio::Get().service()) {
    service->Post([clip = clip_.get()] { ma_sound_stop(clip->sound()); });
  }
}

void AudioStream::Seek(float pos) {
  if (!clip_)
    return;

  if (auto* service = Audio::Get().service()) {
    const ma_uint64 frame = MillisecondsToFrames(pos);
    service->Post([clip = clip_.get(), frame] {
      ma_sound_seek_to_pcm_frame(clip->sound(), frame);
    });
  }
}

float AudioStream::Cursor() {
  if (!clip_)
    return 0.0f;

  return FramesToMilliseconds(clip_->cursor_frames());
}

float AudioStream::Length() {
  if (!clip_)
    return 0.0f;

  if (length_ >= 0.0f)
    return length_;

  return FramesToMilliseconds(clip_->length_frames());
}

bool AudioStream::IsPlaying() {
  return clip_ && clip_->playing();
}

bool AudioStream::IsEnd() {
  return clip_ && clip_->at_end();
}

void AudioStream::SetStartTime(float pos) {
  start_time_ = std::max(pos, 0.0f);
  ApplyLoopPoint();
}

void AudioStream::SetStopTime(float pos) {
  stop_time_ = std::max(pos, 0.0f);
  ApplyLoopPoint();
}

ATTR_DEF(AudioStream, float, Volume) {
  if (value.has_value()) {
    volume_ = std::clamp(*value, 0.0f, 1.0f);
    if (clip_) {
      if (auto* service = Audio::Get().service()) {
        const float volume = volume_;
        service->Post([clip = clip_.get(), volume] {
          ma_sound_set_volume(clip->sound(), volume);
        });
      }
    }
    return std::nullopt;
  }

  return volume_;
}

ATTR_DEF(AudioStream, float, Pan) {
  if (value.has_value()) {
    pan_ = std::clamp(*value, -1.0f, 1.0f);
    if (clip_) {
      if (auto* service = Audio::Get().service()) {
        const float pan = pan_;
        service->Post([clip = clip_.get(), pan] {
          ma_sound_set_pan(clip->sound(), pan);
        });
      }
    }
    return std::nullopt;
  }

  return pan_;
}

ATTR_DEF(AudioStream, float, Pitch) {
  if (value.has_value()) {
    if (*value > 0.0f)
      pitch_ = *value;

    if (clip_) {
      if (auto* service = Audio::Get().service()) {
        const float pitch = pitch_;
        service->Post([clip = clip_.get(), pitch] {
          ma_sound_set_pitch(clip->sound(), pitch);
        });
      }
    }
    return std::nullopt;
  }

  return pitch_;
}

ATTR_DEF(AudioStream, bool, Loop) {
  if (value.has_value()) {
    loop_ = *value;
    if (clip_) {
      if (auto* service = Audio::Get().service()) {
        const bool loop = loop_;
        service->Post([clip = clip_.get(), loop] {
          ma_sound_set_looping(clip->sound(), loop ? MA_TRUE : MA_FALSE);
        });
      }
    }
    return std::nullopt;
  }

  return loop_;
}

void AudioStream::ApplyLoopPoint() {
  if (!clip_)
    return;

  auto* service = Audio::Get().service();
  if (!service)
    return;

  const ma_uint64 begin = MillisecondsToFrames(start_time_);
  const ma_uint64 end = stop_time_ > start_time_
                            ? MillisecondsToFrames(stop_time_)
                            : kLoopPointEnd;
  const float start = start_time_;
  const float stop = stop_time_;

  service->Post([clip = clip_.get(), begin, end, start, stop] {
    auto* source = ma_sound_get_data_source(clip->sound());
    if (!source)
      return;

    if (ma_data_source_set_loop_point_in_pcm_frames(source, begin, end) !=
        MA_SUCCESS) {
      LOGGER_WARN("the loop point [{}, {}] ms was rejected by the audio stream",
                  start, stop);
    }
  });
}

}

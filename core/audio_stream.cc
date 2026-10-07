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

#include "core/audio_stream.h"

#include <algorithm>

#include "core/audio.h"
#include "core/audio_service.h"
#include "core/exception.h"
#include "core/logger.h"

namespace urge {

namespace {

constexpr ma_uint64 kLoopPointEnd = ~static_cast<ma_uint64>(0);

ma_uint32 SoundSampleRate(ma_sound* sound) {
  ma_uint32 rate = 0;
  if (ma_sound_get_data_format(sound, nullptr, nullptr, &rate, nullptr, 0) !=
          MA_SUCCESS ||
      rate == 0)
    return AudioService::kEngineSampleRate;

  return rate;
}

ma_uint64 MillisecondsToFrames(ma_uint32 rate, float pos) {
  if (pos <= 0.0f)
    return 0;

  return static_cast<ma_uint64>(static_cast<double>(pos) * rate / 1000.0);
}

float FramesToMilliseconds(ma_uint32 rate, ma_uint64 frames) {
  if (rate == 0)
    return 0.0f;

  return static_cast<float>(static_cast<double>(frames) * 1000.0 / rate);
}

}  // namespace

AudioStream::AudioStream(std::string filename) {
  auto* service = Audio::Get().service();
  if (!service)
    throw Exception(Exception::kRGSSError, "the audio service is unavailable.");

  auto clip = std::make_unique<AudioClip>();
  if (!clip->Open(service->engine(), service->bus(AudioBus::kSE), filename))
    throw Exception(Exception::kRGSSError,
                    "the audio file could not be opened: {}", filename);

  clip_ = std::move(clip);
  ApplyState();
}

AudioStream::AudioStream(std::vector<uint8_t> data, float length) {
  auto* service = Audio::Get().service();
  if (!service)
    throw Exception(Exception::kRGSSError, "the audio service is unavailable.");

  auto clip = std::make_unique<AudioClip>();
  if (!clip->OpenMemory(service->engine(), service->bus(AudioBus::kSE),
                        std::move(data)))
    throw Exception(Exception::kRGSSError,
                    "the audio track could not be opened.");

  clip_ = std::move(clip);
  length_ = length > 0.0f ? length : -1.0f;
  ApplyState();
}

AudioStream::~AudioStream() = default;

void AudioStream::ApplyState() {
  if (!clip_)
    return;

  ma_sound_set_volume(clip_->sound(), volume_);
  ma_sound_set_pan(clip_->sound(), pan_);
  ma_sound_set_pitch(clip_->sound(), pitch_);
  ma_sound_set_looping(clip_->sound(), loop_ ? MA_TRUE : MA_FALSE);
}

void AudioStream::Start() {
  if (clip_)
    ma_sound_start(clip_->sound());
}

void AudioStream::Stop() {
  if (clip_)
    ma_sound_stop(clip_->sound());
}

void AudioStream::Seek(float pos) {
  if (!clip_)
    return;

  const ma_uint32 rate = SoundSampleRate(clip_->sound());
  ma_sound_seek_to_pcm_frame(clip_->sound(), MillisecondsToFrames(rate, pos));
}

float AudioStream::Cursor() {
  if (!clip_)
    return 0.0f;

  ma_uint64 frames = 0;
  if (ma_sound_get_cursor_in_pcm_frames(clip_->sound(), &frames) != MA_SUCCESS)
    return 0.0f;

  return FramesToMilliseconds(SoundSampleRate(clip_->sound()), frames);
}

float AudioStream::Length() {
  if (!clip_)
    return 0.0f;

  if (length_ >= 0.0f)
    return length_;

  ma_uint64 frames = 0;
  if (ma_sound_get_length_in_pcm_frames(clip_->sound(), &frames) != MA_SUCCESS)
    return 0.0f;

  return FramesToMilliseconds(SoundSampleRate(clip_->sound()), frames);
}

bool AudioStream::IsPlaying() {
  return clip_ && ma_sound_is_playing(clip_->sound()) != MA_FALSE;
}

bool AudioStream::IsEnd() {
  return clip_ && ma_sound_at_end(clip_->sound()) != MA_FALSE;
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
    if (clip_)
      ma_sound_set_volume(clip_->sound(), volume_);
    return std::nullopt;
  }

  return volume_;
}

ATTR_DEF(AudioStream, float, Pan) {
  if (value.has_value()) {
    pan_ = std::clamp(*value, -1.0f, 1.0f);
    if (clip_)
      ma_sound_set_pan(clip_->sound(), pan_);
    return std::nullopt;
  }

  return pan_;
}

ATTR_DEF(AudioStream, float, Pitch) {
  if (value.has_value()) {
    if (*value > 0.0f) {
      pitch_ = *value;
      if (clip_)
        ma_sound_set_pitch(clip_->sound(), pitch_);
    }
    return std::nullopt;
  }

  return pitch_;
}

ATTR_DEF(AudioStream, bool, Loop) {
  if (value.has_value()) {
    loop_ = *value;
    if (clip_)
      ma_sound_set_looping(clip_->sound(), loop_ ? MA_TRUE : MA_FALSE);
    return std::nullopt;
  }

  return loop_;
}

void AudioStream::ApplyLoopPoint() {
  if (!clip_)
    return;

  auto* source = ma_sound_get_data_source(clip_->sound());
  if (!source)
    return;

  const ma_uint32 rate = SoundSampleRate(clip_->sound());
  const ma_uint64 begin = MillisecondsToFrames(rate, start_time_);
  const ma_uint64 end = stop_time_ > start_time_
                            ? MillisecondsToFrames(rate, stop_time_)
                            : kLoopPointEnd;

  if (ma_data_source_set_loop_point_in_pcm_frames(source, begin, end) !=
      MA_SUCCESS)
    LOGGER_WARN("the loop point [{}, {}] ms was rejected by the audio stream",
                start_time_, stop_time_);
}

}  // namespace urge

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

#include "core/video.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

#include "core/audio.h"
#include "core/exception.h"
#include "core/logger.h"

namespace urge {

#if URGE_ENABLE_VIDEO
namespace {

constexpr float kMillisecondsPerSecond = 1000.0f;
constexpr float kAudioSyncTolerance = 250.0f;
constexpr uint32_t kFixedPointOne = 65536;

std::string FormatNumber(double value) {
  char text[32] = {};
  std::snprintf(text, sizeof(text), "%.3f", value);

  std::string result(text);
  while (result.size() > 1 && result.back() == '0')
    result.pop_back();
  if (!result.empty() && result.back() == '.')
    result.pop_back();
  return result;
}

std::string FormatBitrate(const VideoTrackInfo& info) {
  if (info.duration <= 0.0 || info.file_size <= 0)
    return "0";
  return FormatNumber(static_cast<double>(info.file_size) * 8.0 /
                      info.duration);
}

}  // namespace

Video::Video(std::string filename) {
  if (!player_.Open(filename))
    throw Exception(Exception::kRGSSError, "the video could not be opened: {}",
                    filename);

  std::vector<uint8_t> track = player_.TakeAudio();
  if (track.empty())
    return;

  try {
    audio_ = MakeRefCounted<AudioStream>(
        std::move(track),
        static_cast<float>(player_.duration() * kMillisecondsPerSecond));
  } catch (const Exception& error) {
    LOGGER_WARN("the video plays without its audio track: {}", error.message());
    audio_ = nullptr;
  }
}

Video::~Video() = default;

void Video::Play() {
  player_.Play();
  if (!audio_)
    return;

  const float target =
      static_cast<float>(player_.tell() * kMillisecondsPerSecond);
  if (std::fabs(audio_->Cursor() - target) > kAudioSyncTolerance)
    audio_->Seek(target);
  if (!player_.stalled())
    audio_->Start();
}

void Video::Pause() {
  player_.Pause();
  if (audio_)
    audio_->Stop();
}

void Video::Stop() {
  player_.Stop();
  if (!audio_)
    return;

  audio_->Stop();
  audio_->Seek(0.0f);
}

bool Video::End() {
  return player_.ended();
}

bool Video::IsPlaying() {
  return player_.playing() && !player_.ended();
}

void Video::Seek(float pos) {
  const double seconds =
      static_cast<double>(pos > 0.0f ? pos : 0.0f) / kMillisecondsPerSecond;
  player_.Seek(seconds);

  if (!audio_)
    return;

  audio_->Seek(static_cast<float>(seconds * kMillisecondsPerSecond));
  audio_->Stop();
  audio_align_ = true;
}

float Video::Tell() {
  return static_cast<float>(player_.tell() * kMillisecondsPerSecond);
}

float Video::Duration() {
  return static_cast<float>(player_.duration() * kMillisecondsPerSecond);
}

RefPtr<AudioStream> Video::GetAudioStream() {
  return audio_;
}

std::map<std::string, std::string> Video::PlayerInfo() {
  const VideoTrackInfo& info = player_.info();

  std::map<std::string, std::string> result;
  result["width"] = std::to_string(info.width);
  result["height"] = std::to_string(info.height);
  result["duration"] = FormatNumber(info.duration);
  result["frame_rate"] = FormatNumber(info.frame_rate);
  result["bitrate"] = FormatBitrate(info);
  result["file_size"] = std::to_string(info.file_size);
  result["video_codec"] = info.video_codec;
  result["video_codec_name"] = info.video_codec_name;
  result["video_track"] = std::to_string(info.video_track);
  result["audio_codec"] = info.audio_codec;
  result["audio_codec_name"] = info.audio_codec_name;
  result["audio_track"] = std::to_string(info.audio_track);
  result["audio_channels"] = std::to_string(info.audio_channels);
  result["audio_sample_rate"] = std::to_string(info.audio_sample_rate);
  result["has_audio"] = info.has_audio ? "true" : "false";
  return result;
}

void Video::Update() {
  player_.Update();
  SyncAudio();
}

void Video::Render(RefPtr<Bitmap> target) {
  if (!target)
    throw Exception(Exception::kRGSSError, "invalid video render target.");

  const std::vector<uint8_t>* source = player_.rgba();
  if (!source || source->empty())
    return;

  const int32_t source_width = player_.rgba_width();
  const int32_t source_height = player_.rgba_height();
  const glm::ivec2 size = target->size();
  const int32_t target_width = size.x;
  const int32_t target_height = size.y;
  if (source_width <= 0 || source_height <= 0 || target_width <= 0 ||
      target_height <= 0)
    return;

  if (source_width == target_width && source_height == target_height) {
    target->UpdateWithPixels(source->data(),
                             static_cast<uint32_t>(source_width) * 4);
    return;
  }

  scaled_.resize(static_cast<size_t>(target_width) * target_height * 4);
  const uint32_t step_x = static_cast<uint32_t>(
      static_cast<uint64_t>(source_width) * kFixedPointOne / target_width);
  const uint32_t step_y = static_cast<uint32_t>(
      static_cast<uint64_t>(source_height) * kFixedPointOne / target_height);

  uint32_t row = 0;
  for (int32_t y = 0; y < target_height; ++y) {
    const uint8_t* source_row =
        source->data() + static_cast<size_t>(row >> 16) * source_width * 4;
    uint8_t* target_row =
        scaled_.data() + static_cast<size_t>(y) * target_width * 4;

    uint32_t column = 0;
    for (int32_t x = 0; x < target_width; ++x) {
      std::memcpy(target_row + static_cast<size_t>(x) * 4,
                  source_row + static_cast<size_t>(column >> 16) * 4, 4);
      column += step_x;
    }

    row += step_y;
  }

  target->UpdateWithPixels(scaled_.data(),
                           static_cast<uint32_t>(target_width) * 4);
}

void Video::SyncAudio() {
  if (!audio_)
    return;

  if (!player_.playing() || player_.stalled()) {
    if (audio_->IsPlaying())
      audio_->Stop();
    return;
  }

  const float target =
      static_cast<float>(player_.tell() * kMillisecondsPerSecond);
  if (audio_align_ ||
      std::fabs(audio_->Cursor() - target) > kAudioSyncTolerance) {
    audio_->Seek(target);
    audio_->Start();
    audio_align_ = false;
    return;
  }

  if (!audio_->IsPlaying())
    audio_->Start();
}

#else

namespace {

[[noreturn]] void VideoUnavailable() {
  throw Exception(Exception::kRGSSError,
                  "video playback is not available in this build.");
}

}  // namespace

Video::Video(std::string) {
  VideoUnavailable();
}

Video::~Video() = default;

void Video::Play() {
  VideoUnavailable();
}

void Video::Pause() {
  VideoUnavailable();
}

void Video::Stop() {
  VideoUnavailable();
}

bool Video::End() {
  VideoUnavailable();
}

bool Video::IsPlaying() {
  VideoUnavailable();
}

void Video::Seek(float) {
  VideoUnavailable();
}

float Video::Tell() {
  VideoUnavailable();
}

float Video::Duration() {
  VideoUnavailable();
}

RefPtr<AudioStream> Video::GetAudioStream() {
  VideoUnavailable();
}

std::map<std::string, std::string> Video::PlayerInfo() {
  VideoUnavailable();
}

void Video::Update() {
  VideoUnavailable();
}

void Video::Render(RefPtr<Bitmap>) {
  VideoUnavailable();
}

#endif

}  // namespace urge

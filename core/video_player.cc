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

#include "core/video_player.h"

#include <cstddef>
#include <utility>

namespace urge {
namespace {

constexpr size_t kPacketQueueLimit = 48;
constexpr size_t kFrameQueueLimit = 8;
constexpr size_t kIdleFrameLimit = 4;
constexpr int64_t kMicrosecondsPerSecond = 1000000;

struct ColorCoefficients {
  int32_t luma_scale;
  int32_t red_v;
  int32_t green_u;
  int32_t green_v;
  int32_t blue_u;
  int32_t luma_offset;
  int32_t chroma_offset;
};

ColorCoefficients ResolveCoefficients(const Dav1dPicture& picture) {
  const bool full_range = picture.seq_hdr && picture.seq_hdr->color_range != 0;
  const bool bt709 = picture.seq_hdr && picture.seq_hdr->mtrx == DAV1D_MC_BT709;

  if (full_range)
    return bt709 ? ColorCoefficients{256, 403, 48, 120, 475, 0, 128}
                 : ColorCoefficients{256, 359, 88, 183, 454, 0, 128};
  return bt709 ? ColorCoefficients{298, 459, 55, 136, 541, 16, 128}
               : ColorCoefficients{298, 409, 100, 208, 516, 16, 128};
}

uint8_t Clamp8(int32_t value) {
  return static_cast<uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
}

template <typename Sample>
void ConvertPicture(const Dav1dPicture& picture,
                    const ColorCoefficients& coefficients,
                    int32_t shift,
                    std::vector<uint8_t>* out) {
  const int32_t width = picture.p.w;
  const int32_t height = picture.p.h;
  out->resize(static_cast<size_t>(width) * height * 4);

  const auto* luma_plane = static_cast<const Sample*>(picture.data[0]);
  const auto* chroma_u_plane = static_cast<const Sample*>(picture.data[1]);
  const auto* chroma_v_plane = static_cast<const Sample*>(picture.data[2]);
  const ptrdiff_t luma_stride =
      picture.stride[0] / static_cast<ptrdiff_t>(sizeof(Sample));
  const ptrdiff_t chroma_stride =
      picture.stride[1] / static_cast<ptrdiff_t>(sizeof(Sample));

  const bool monochrome = picture.p.layout == DAV1D_PIXEL_LAYOUT_I400;
  const int32_t chroma_row_step =
      picture.p.layout == DAV1D_PIXEL_LAYOUT_I420 ? 2 : 1;
  const int32_t chroma_column_step =
      picture.p.layout == DAV1D_PIXEL_LAYOUT_I444 ? 1 : 2;

  for (int32_t row = 0; row < height; ++row) {
    const Sample* luma_row =
        luma_plane + static_cast<ptrdiff_t>(row) * luma_stride;
    const Sample* chroma_u_row =
        monochrome ? nullptr
                   : chroma_u_plane +
                         static_cast<ptrdiff_t>(row / chroma_row_step) *
                             chroma_stride;
    const Sample* chroma_v_row =
        monochrome ? nullptr
                   : chroma_v_plane +
                         static_cast<ptrdiff_t>(row / chroma_row_step) *
                             chroma_stride;
    uint8_t* target = out->data() + static_cast<size_t>(row) * width * 4;

    for (int32_t column = 0; column < width; ++column, target += 4) {
      int32_t luma =
          (static_cast<int32_t>(luma_row[column]) >> shift) -
          coefficients.luma_offset;
      if (luma < 0)
        luma = 0;

      int32_t chroma_u = 0;
      int32_t chroma_v = 0;
      if (!monochrome) {
        const int32_t index = column / chroma_column_step;
        chroma_u = (static_cast<int32_t>(chroma_u_row[index]) >> shift) -
                   coefficients.chroma_offset;
        chroma_v = (static_cast<int32_t>(chroma_v_row[index]) >> shift) -
                   coefficients.chroma_offset;
      }

      luma *= coefficients.luma_scale;
      target[0] = Clamp8(
          (luma + coefficients.red_v * chroma_v + 128) >> 8);
      target[1] = Clamp8((luma - coefficients.green_u * chroma_u -
                          coefficients.green_v * chroma_v + 128) >>
                         8);
      target[2] = Clamp8(
          (luma + coefficients.blue_u * chroma_u + 128) >> 8);
      target[3] = 255;
    }
  }
}

}  // namespace

VideoPlayer::VideoPlayer() = default;

VideoPlayer::~VideoPlayer() { Close(); }

bool VideoPlayer::Open(const std::string& filename) {
  Close();

  if (!demuxer_.Open(filename))
    return false;

  const uint32_t threads = std::thread::hardware_concurrency();
  if (!decoder_.Open(static_cast<int32_t>(threads), 1)) {
    demuxer_.Close();
    return false;
  }

  info_ = demuxer_.info();

  audio_ = demuxer_.ExtractAudio();

  {
    std::lock_guard<std::mutex> lock(mutex_);
    packets_.clear();
    frames_.clear();
    current_ = VideoFrame();
    converted_.clear();
    converted_width_ = 0;
    converted_height_ = 0;
    converted_valid_ = false;
    playing_ = false;
    waiting_ = false;
    demux_done_ = false;
    seek_serial_ = 0;
    demux_serial_ = 0;
    seek_target_ = 0.0;
    paused_at_ = 0.0;
    resumed_at_ = std::chrono::steady_clock::now();
    running_ = true;
  }

  demux_thread_ = std::thread(&VideoPlayer::DemuxLoop, this);
  decode_thread_ = std::thread(&VideoPlayer::DecodeLoop, this);
  return true;
}

void VideoPlayer::Close() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    wake_.notify_all();
  }

  if (demux_thread_.joinable())
    demux_thread_.join();
  if (decode_thread_.joinable())
    decode_thread_.join();

  decoder_.Close();
  demuxer_.Close();

  std::lock_guard<std::mutex> lock(mutex_);
  ReleaseQueues();
  audio_.clear();
  info_ = VideoTrackInfo();
}

void VideoPlayer::Play() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (playing_ && running_)
    return;

  if (info_.duration > 0.0 && paused_at_ >= info_.duration) {
    paused_at_ = 0.0;
    seek_target_ = 0.0;
    ResetPosition();
    ClearPicture();
    waiting_ = true;
  }

  resumed_at_ = std::chrono::steady_clock::now();
  playing_ = true;
  if (!current_.valid)
    waiting_ = true;
  wake_.notify_all();
}

void VideoPlayer::Pause() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!playing_)
    return;

  paused_at_ = TellLocked();
  playing_ = false;
  wake_.notify_all();
}

void VideoPlayer::Stop() {
  std::lock_guard<std::mutex> lock(mutex_);
  playing_ = false;
  waiting_ = false;
  paused_at_ = 0.0;
  seek_target_ = 0.0;
  ResetPosition();
  ClearPicture();
  wake_.notify_all();
}

void VideoPlayer::Seek(double seconds) {
  std::lock_guard<std::mutex> lock(mutex_);
  const double target = seconds > 0.0 ? seconds : 0.0;

  seek_target_ = target;
  paused_at_ = target;
  resumed_at_ = std::chrono::steady_clock::now();
  waiting_ = true;
  ResetPosition();
  wake_.notify_all();
}

void VideoPlayer::Update() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!running_)
    return;

  Advance(std::chrono::steady_clock::now());
  if (!converted_valid_)
    ConvertCurrent();
}
bool VideoPlayer::ended() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (info_.duration > 0.0 && TellLocked() >= info_.duration)
    return true;
  return demux_done_ && packets_.empty() && frames_.empty();
}

bool VideoPlayer::playing() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return playing_;
}

bool VideoPlayer::stalled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return waiting_;
}

double VideoPlayer::tell() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return TellLocked();
}

double VideoPlayer::duration() const { return info_.duration; }

std::vector<uint8_t> VideoPlayer::TakeAudio() {
  return std::move(audio_);
}

const std::vector<uint8_t>* VideoPlayer::rgba() const {
  return converted_valid_ ? &converted_ : nullptr;
}

VideoPlayer::Step VideoPlayer::NextDemuxStep(double* target) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!running_)
    return Step::kExit;

  if (seek_serial_ != demux_serial_) {
    demux_serial_ = seek_serial_;
    *target = seek_target_;
    demux_done_ = false;
    return Step::kSeek;
  }

  if (demux_done_)
    return Step::kIdle;

  const bool wanted = playing_ || frames_.size() < kIdleFrameLimit;
  if (!wanted || packets_.size() >= kPacketQueueLimit)
    return Step::kIdle;
  return Step::kRead;
}

void VideoPlayer::DemuxLoop() {
  for (;;) {
    double target = 0.0;
    switch (NextDemuxStep(&target)) {
      case Step::kExit:
        return;

      case Step::kSeek: {
        demuxer_.Seek(target);

        std::lock_guard<std::mutex> lock(mutex_);
        if (seek_serial_ != demux_serial_)
          continue;

        QueueEntry command;
        command.seek = true;
        command.position = target;
        packets_.push_back(std::move(command));
        wake_.notify_all();
        continue;
      }

      case Step::kIdle: {
        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock, std::chrono::milliseconds(10), [this] {
          if (!running_ || seek_serial_ != demux_serial_)
            return true;
          if (demux_done_)
            return false;
          return (playing_ || frames_.size() < kIdleFrameLimit) &&
                 packets_.size() < kPacketQueueLimit;
        });
        continue;
      }

      case Step::kRead:
        break;
    }

    VideoPacket packet;
    if (!demuxer_.NextVideoPacket(&packet)) {
      std::lock_guard<std::mutex> lock(mutex_);
      demux_done_ = true;
      wake_.notify_all();
      continue;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (seek_serial_ != demux_serial_)
      continue;

    QueueEntry queued;
    queued.packet = std::move(packet);
    packets_.push_back(std::move(queued));
    wake_.notify_all();
  }
}

bool VideoPlayer::DecodeReady() const {
  if (packets_.empty())
    return false;
  if (packets_.front().seek)
    return true;
  return frames_.size() < kFrameQueueLimit;
}

void VideoPlayer::DecodeLoop() {
  std::vector<VideoFrame> decoded;
  int64_t discard_before = INT64_MIN;

  for (;;) {
    QueueEntry entry;

    {
      std::unique_lock<std::mutex> lock(mutex_);
      while (running_ && !DecodeReady())
        wake_.wait_for(lock, std::chrono::milliseconds(10));
      if (!running_)
        return;

      entry = std::move(packets_.front());
      packets_.pop_front();
    }

    if (entry.seek) {
      decoder_.Flush();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        DropFrames();
      }
      discard_before = entry.position != 0.0
                           ? static_cast<int64_t>(entry.position *
                                                  kMicrosecondsPerSecond)
                           : INT64_MIN;
      wake_.notify_all();
      continue;
    }

    const int64_t timestamp =
        static_cast<int64_t>(entry.packet.pts * kMicrosecondsPerSecond);
    decoded.clear();
    if (!decoder_.Decode(entry.packet.data.data(), entry.packet.data.size(),
                         timestamp, &decoded))
      continue;

    if (decoded.empty())
      continue;

    std::lock_guard<std::mutex> lock(mutex_);
    for (VideoFrame& frame : decoded) {
      if (discard_before != INT64_MIN && frame.timestamp != INT64_MIN &&
          frame.timestamp < discard_before) {
        ReleaseFrame(&frame);
        continue;
      }
      discard_before = INT64_MIN;
      frames_.push_back(std::move(frame));
    }
    wake_.notify_all();
  }
}

double VideoPlayer::TellLocked() const {
  if (!playing_ || waiting_)
    return paused_at_;
  return paused_at_ + std::chrono::duration<double>(
                          std::chrono::steady_clock::now() - resumed_at_)
                          .count();
}

void VideoPlayer::Advance(std::chrono::steady_clock::time_point now) {
  if (waiting_) {
    if (frames_.empty())
      return;

    ReleaseFrame(&current_);
    current_ = std::move(frames_.front());
    frames_.pop_front();
    paused_at_ = current_.valid ? current_.pts() : paused_at_;
    resumed_at_ = now;
    waiting_ = false;
    converted_valid_ = false;
    wake_.notify_all();
    return;
  }

  const double position = TellLocked();
  if (frames_.empty() || frames_.front().pts() > position)
    return;

  ReleaseFrame(&current_);
  do {
    current_ = std::move(frames_.front());
    frames_.pop_front();
  } while (!frames_.empty() && frames_.front().pts() <= position);

  converted_valid_ = false;
  wake_.notify_all();
}

void VideoPlayer::ConvertCurrent() {
  converted_valid_ = false;
  if (!current_.valid) {
    converted_width_ = 0;
    converted_height_ = 0;
    return;
  }

  const Dav1dPicture& picture = current_.picture;
  const ColorCoefficients coefficients = ResolveCoefficients(picture);
  const int32_t shift = picture.p.bpc > 8 ? picture.p.bpc - 8 : 0;
  if (picture.p.bpc > 8)
    ConvertPicture<uint16_t>(picture, coefficients, shift, &converted_);
  else
    ConvertPicture<uint8_t>(picture, coefficients, shift, &converted_);

  converted_width_ = picture.p.w;
  converted_height_ = picture.p.h;
  converted_valid_ = true;
}

void VideoPlayer::ReleaseFrame(VideoFrame* frame) {
  if (frame->valid)
    dav1d_picture_unref(&frame->picture);
  *frame = VideoFrame();
}

void VideoPlayer::DropFrames() {
  for (VideoFrame& frame : frames_)
    ReleaseFrame(&frame);
  frames_.clear();
}

void VideoPlayer::ClearPicture() {
  ReleaseFrame(&current_);

  converted_.clear();
  converted_width_ = 0;
  converted_height_ = 0;
  converted_valid_ = false;
}

void VideoPlayer::ReleaseQueues() {
  DropFrames();
  ClearPicture();
}

void VideoPlayer::ResetPosition() {
  ++seek_serial_;
  demux_done_ = false;
  packets_.clear();
  DropFrames();
}

}  // namespace urge

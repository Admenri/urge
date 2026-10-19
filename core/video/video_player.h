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

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "core/video_decoder.h"
#include "core/video_demuxer.h"

namespace urge {

class VideoPlayer {
 public:
  VideoPlayer();
  ~VideoPlayer();

  VideoPlayer(const VideoPlayer&) = delete;
  VideoPlayer& operator=(const VideoPlayer&) = delete;

  bool Open(const std::string& filename);
  void Close();

  void Play();
  void Pause();
  void Stop();
  void Seek(double seconds);
  void Update();

  bool ended();
  bool playing() const;
  bool stalled() const;
  double tell() const;
  double duration() const;

  int32_t width() const { return info_.width; }
  int32_t height() const { return info_.height; }
  const VideoTrackInfo& info() const { return info_; }

  std::vector<uint8_t> TakeAudio();

  const std::vector<uint8_t>* rgba() const;
  int32_t rgba_width() const { return converted_width_; }
  int32_t rgba_height() const { return converted_height_; }

 private:
  enum class Step {
    kExit,
    kSeek,
    kRead,
    kIdle,
  };

  struct QueueEntry {
    VideoPacket packet;
    bool seek = false;
    double position = 0.0;
  };

  Step NextDemuxStep(double* target);
  void DemuxLoop();
  bool DecodeReady() const;
  void DecodeLoop();

  double TellLocked() const;
  void Advance(std::chrono::steady_clock::time_point now);
  void ConvertCurrent();
  void ReleaseFrame(VideoFrame* frame);
  void DropFrames();
  void ClearPicture();
  void ReleaseQueues();
  void ResetPosition();

  VideoDemuxer demuxer_;
  VideoDecoder decoder_;
  VideoTrackInfo info_;

  std::thread demux_thread_;
  std::thread decode_thread_;

  mutable std::mutex mutex_;
  std::condition_variable wake_;

  std::deque<QueueEntry> packets_;
  std::deque<VideoFrame> frames_;
  VideoFrame current_;
  std::vector<uint8_t> audio_;

  std::vector<uint8_t> converted_;
  int32_t converted_width_ = 0;
  int32_t converted_height_ = 0;
  bool converted_valid_ = false;

  bool running_ = false;
  bool playing_ = false;
  bool waiting_ = false;
  bool demux_done_ = false;
  uint32_t seek_serial_ = 0;
  uint32_t demux_serial_ = 0;
  double seek_target_ = 0.0;
  double paused_at_ = 0.0;
  std::chrono::steady_clock::time_point resumed_at_;
};

}  // namespace urge

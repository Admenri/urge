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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace urge {

struct VideoPacket {
  std::vector<uint8_t> data;
  double pts = 0.0;
  bool key = false;
};

struct VideoTrackInfo {
  double duration = 0.0;
  double frame_rate = 0.0;
  int64_t file_size = 0;
  int32_t width = 0;
  int32_t height = 0;
  int32_t video_track = 0;
  int32_t audio_track = 0;
  int32_t audio_channels = 0;
  int32_t audio_sample_rate = 0;
  std::string video_codec;
  std::string video_codec_name;
  std::string audio_codec;
  std::string audio_codec_name;
  bool has_video = false;
  bool has_audio = false;
};

class VideoDemuxer {
 public:
  VideoDemuxer();
  ~VideoDemuxer();

  VideoDemuxer(const VideoDemuxer&) = delete;
  VideoDemuxer& operator=(const VideoDemuxer&) = delete;

  bool Open(const std::string& filename);
  void Close();

  const VideoTrackInfo& info() const { return info_; }

  std::vector<uint8_t> ExtractAudio();
  bool NextVideoPacket(VideoPacket* packet);
  bool Seek(double seconds);

 private:
  struct Kernel;

  void ResetVideoCursor();

  std::unique_ptr<Kernel> kernel_;
  VideoTrackInfo info_;
};

}  // namespace urge

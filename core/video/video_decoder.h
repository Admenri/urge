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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "dav1d/dav1d.h"

namespace urge {

struct VideoFrame {
  Dav1dPicture picture = {};
  int64_t timestamp = INT64_MIN;
  bool valid = false;

  double pts() const { return static_cast<double>(timestamp) / 1000000.0; }
};

class VideoDecoder {
 public:
  VideoDecoder() = default;
  ~VideoDecoder();

  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;

  bool Open(int32_t threads, int32_t max_frame_delay);
  void Close();
  void Flush();

  bool Decode(const uint8_t* data,
              size_t size,
              int64_t timestamp,
              std::vector<VideoFrame>* frames);

  bool is_open() const { return context_ != nullptr; }

 private:
  void Drain(std::vector<VideoFrame>* frames);

  Dav1dContext* context_ = nullptr;
};

}  // namespace urge

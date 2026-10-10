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

#include "core/video_decoder.h"

#include <cstring>

#include "core/logger.h"

namespace urge {

VideoDecoder::~VideoDecoder() {
  Close();
}

bool VideoDecoder::Open(int32_t threads, int32_t max_frame_delay) {
  Close();

  Dav1dSettings settings = {};
  dav1d_default_settings(&settings);
  settings.n_threads = threads > 0 ? threads : 0;
  settings.max_frame_delay = max_frame_delay > 0 ? max_frame_delay : 0;

  if (dav1d_open(&context_, &settings) != 0 || !context_) {
    LOGGER_ERROR("dav1d could not be initialized");
    context_ = nullptr;
    return false;
  }
  return true;
}

void VideoDecoder::Close() {
  if (context_) {
    dav1d_close(&context_);
    context_ = nullptr;
  }
}

void VideoDecoder::Flush() {
  if (context_)
    dav1d_flush(context_);
}

void VideoDecoder::Drain(std::vector<VideoFrame>* frames) {
  for (;;) {
    Dav1dPicture picture = {};
    const int32_t result = dav1d_get_picture(context_, &picture);
    if (result == DAV1D_ERR(EAGAIN))
      return;

    if (result < 0) {
      LOGGER_WARN("dav1d skipped a frame it could not decode ({})", result);
      return;
    }

    VideoFrame frame;
    frame.picture = picture;
    frame.timestamp = picture.m.timestamp;
    frame.valid = true;
    frames->push_back(frame);
  }
}

bool VideoDecoder::Decode(const uint8_t* data,
                          size_t size,
                          int64_t timestamp,
                          std::vector<VideoFrame>* frames) {
  if (!context_ || !data || size == 0)
    return false;

  Dav1dData input = {};
  uint8_t* buffer = dav1d_data_create(&input, size);
  if (!buffer) {
    LOGGER_ERROR("dav1d ran out of memory for a {} byte packet", size);
    return false;
  }
  std::memcpy(buffer, data, size);
  input.m.timestamp = timestamp;

  for (int32_t attempt = 0; input.sz > 0 && attempt < 16; ++attempt) {
    const int32_t result = dav1d_send_data(context_, &input);
    if (result < 0 && result != DAV1D_ERR(EAGAIN)) {
      LOGGER_WARN("dav1d rejected a packet ({})", result);
      break;
    }
    Drain(frames);
  }

  dav1d_data_unref(&input);
  Drain(frames);
  return true;
}

}  // namespace urge

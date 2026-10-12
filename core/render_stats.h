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

#include "SDL3/SDL_timer.h"

namespace urge {

inline double TicksToMilliseconds(std::uint64_t ticks) {
  static const double frequency =
      static_cast<double>(SDL_GetPerformanceFrequency());
  return static_cast<double>(ticks) * 1000.0 / frequency;
}

// The counters of one frame, split by the subsystem that produced them.  A
// `Bitmap` drawing operation is not a draw call but a whole submission of its
// own, so the interesting number is how many of them a frame pays for and what
// each stage of one costs.
struct RenderFrameCounters {
  std::uint32_t bitmap_ops = 0;
  std::uint32_t bitmap_submits = 0;
  std::uint32_t frame_submits = 0;

  double bitmap_upload_ms = 0.0;
  double bitmap_record_ms = 0.0;
  double bitmap_finish_ms = 0.0;
  double bitmap_submit_ms = 0.0;

  double bitmap_total_ms() const {
    return bitmap_upload_ms + bitmap_record_ms + bitmap_finish_ms +
           bitmap_submit_ms;
  }
};

// `current` collects the frame being built, `last` is the snapshot the debug
// overlay shows -- the counters have to survive the reset that ends a frame.
struct RenderStats {
  RenderFrameCounters current;
  RenderFrameCounters last;

  void EndFrame() {
    last = current;
    current = RenderFrameCounters();
  }
};

inline RenderStats& GetRenderStats() {
  static RenderStats stats;
  return stats;
}

// Every `queue.Submit` is reported here, so a frame can be counted from one
// place.  `bitmap` marks the submissions a `Bitmap` operation made on its own,
// which is exactly the count the batching has to bring down to one.
inline void CountSubmit(bool bitmap) {
  RenderStats& stats = GetRenderStats();
  ++stats.current.frame_submits;
  if (bitmap)
    ++stats.current.bitmap_submits;
}

}  // namespace urge

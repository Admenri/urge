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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "SDL3/SDL_timer.h"

namespace urge {

inline double TicksToMilliseconds(std::uint64_t ticks) {
  static const double frequency =
      static_cast<double>(SDL_GetPerformanceFrequency());
  return static_cast<double>(ticks) * 1000.0 / frequency;
}

// The frame cost split the engine already knows how to measure, kept in one
// place so the debug overlay can show it without reaching into Graphics.
// `update` covers one `Graphics.update` pass of interpreted script (its logic
// and its layout walk), `cpu` is every millisecond the frame spent on the CPU
// (script included) and `present` is the waiting the swapchain made us pay.
struct FrameTimings {
  double cpu_ms = 0.0;
  double update_ms = 0.0;
  double script_ms = 0.0;
  double layout_ms = 0.0;
  double render_ms = 0.0;
  double present_ms = 0.0;
  double other_ms = 0.0;
};

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

// The frame time history the overlay turns into the usual rate numbers.  The
// window is a fixed number of frames, so the percentiles below describe "how
// bad did the last few seconds get", which is what a 1% low is for.
class FrameTimeHistory {
 public:
  static constexpr size_t kCapacity = 240;

  void Push(double milliseconds) {
    if (samples_.size() < kCapacity) {
      samples_.push_back(milliseconds);
    } else {
      samples_[next_] = milliseconds;
      next_ = (next_ + 1) % kCapacity;
    }
    sorted_stale_ = true;
  }

  void Clear() {
    samples_.clear();
    next_ = 0;
    sorted_stale_ = true;
  }

  bool empty() const { return samples_.empty(); }
  size_t size() const { return samples_.size(); }

  // Iteration is oldest-first regardless of where the ring buffer wrapped, so
  // a caller can copy the window straight into a plot.  `PlotLines` needs a
  // contiguous array, hence the ordering promise.
  //
  // `next_` is a cursor *within* the buffer (kept wrapped by Push), so once
  // the buffer is full it points at the oldest sample.  It must not be used as
  // a push counter: an offset that runs past the end of a 240-element vector
  // makes the insert ask for an allocation the size of the distance, which is
  // what surfaced as `vector too long`.
  std::vector<double> Ordered() const {
    if (samples_.size() < kCapacity)
      return samples_;

    std::vector<double> ordered;
    ordered.reserve(kCapacity);
    ordered.insert(ordered.end(), samples_.begin() + next_, samples_.end());
    ordered.insert(ordered.end(), samples_.begin(), samples_.begin() + next_);
    return ordered;
  }

  // The newest sample, i.e. the frame that was just presented.  After the
  // buffer wraps the newest write is the one just before the cursor, not the
  // element at the back of the underlying vector.
  double last() const {
    if (samples_.empty())
      return 0.0;
    if (samples_.size() < kCapacity)
      return samples_.back();
    return samples_[(next_ + kCapacity - 1) % kCapacity];
  }

  double Average() const {
    if (samples_.empty())
      return 0.0;
    double total = 0.0;
    for (double value : samples_)
      total += value;
    return total / static_cast<double>(samples_.size());
  }

  // `ratio` is the slow tail to look at: 0.01 for the 1% low, 0.001 for the
  // 0.1% low.  The result is the average of that tail, sorted worst first.
  double SlowAverage(double ratio) const {
    const std::vector<double>& sorted = Sorted();
    if (sorted.empty())
      return 0.0;

    size_t count = static_cast<size_t>(sorted.size() * ratio);
    if (count == 0)
      count = 1;
    if (count > sorted.size())
      count = sorted.size();

    double total = 0.0;
    for (size_t i = 0; i < count; ++i)
      total += sorted[i];
    return total / static_cast<double>(count);
  }

  double Fastest() const {
    const std::vector<double>& sorted = Sorted();
    return sorted.empty() ? 0.0 : sorted.back();
  }

  double Slowest() const {
    const std::vector<double>& sorted = Sorted();
    return sorted.empty() ? 0.0 : sorted.front();
  }

 private:
  const std::vector<double>& Sorted() const {
    if (sorted_stale_) {
      sorted_ = samples_;
      // Worst (largest frame time) first: index 0 is the slowest frame.
      std::sort(sorted_.begin(), sorted_.end(), std::greater<double>());
      sorted_stale_ = false;
    }
    return sorted_;
  }

  std::vector<double> samples_;
  size_t next_ = 0;

  mutable std::vector<double> sorted_;
  mutable bool sorted_stale_ = true;
};

// `current` collects the frame being built, `last` is the snapshot the debug
// overlay shows -- the counters have to survive the reset that ends a frame.
struct RenderStats {
  RenderFrameCounters current;
  RenderFrameCounters last;

  // Wall clock cost of the frame that just ended, and the history the rate
  // numbers are derived from.  The history is advanced by the engine once per
  // presented frame, so both overlays read the very same numbers.
  FrameTimings timings;
  FrameTimeHistory frame_history;

  void EndFrame() {
    last = current;
    current = RenderFrameCounters();
  }

  // How much of the frame the interpreter spent before handing control back.
  // The script loop calls `Graphics.update` once per frame, so the gap between
  // two consecutive entries into it is the script's own working time -- the
  // engine measures that gap as `logic` already (see Graphics::Update), and
  // this is the same number under the name the profiler shows.
  //
  // There is deliberately no "is a loop running" gate: this engine's
  // `rgss_main` calls its block once rather than looping it, so a game writes
  // its own loop around `Graphics.update`, and the gap between two calls is
  // the right measure whenever updates repeat -- which is the only case the
  // number is shown in.
  void ReportScriptTime(double milliseconds) { script_cost_ms_ = milliseconds; }

  double script_cost_ms() const { return script_cost_ms_; }

 private:
  double script_cost_ms_ = 0.0;
};

inline RenderStats& GetRenderStats() {
  static RenderStats stats;
  return stats;
}

// The overlay plots the same window the rate numbers come from, so the plot
// and the 1% low always describe the same frames.
inline const FrameTimeHistory& GetFrameHistory() {
  return GetRenderStats().frame_history;
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

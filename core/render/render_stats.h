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
#include <array>
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

enum class FramePhase : std::uint8_t {
  kScript = 0,
  kUpdate,
  kBitmap,
  kPresent,
  kOther,
};

inline constexpr std::size_t kFramePhaseCount = 5;

struct FrameTimings {
  double total_ms = 0.0;

  std::array<double, kFramePhaseCount> phase_ms{};

  double phase(FramePhase phase) const {
    return phase_ms[static_cast<std::size_t>(phase)];
  }

  void set_phase(FramePhase phase, double milliseconds) {
    phase_ms[static_cast<std::size_t>(phase)] = std::max(0.0, milliseconds);
  }

  double Sum() const {
    double total = 0.0;
    for (double value : phase_ms)
      total += value;
    return total;
  }
};

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

  std::vector<double> Ordered() const {
    if (samples_.size() < kCapacity)
      return samples_;

    std::vector<double> ordered;
    ordered.reserve(kCapacity);
    ordered.insert(ordered.end(), samples_.begin() + next_, samples_.end());
    ordered.insert(ordered.end(), samples_.begin(), samples_.begin() + next_);
    return ordered;
  }

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

  double RecentAverage(size_t count) const {
    if (samples_.empty() || count == 0)
      return 0.0;

    const std::vector<double> ordered = Ordered();
    count = std::min(count, ordered.size());

    double total = 0.0;
    for (size_t i = ordered.size() - count; i < ordered.size(); ++i)
      total += ordered[i];
    return total / static_cast<double>(count);
  }

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

class FrameTimingRing {
 public:
  static constexpr std::size_t kCapacity = 180;

  void Push(const FrameTimings& timings) {
    samples_[next_] = timings;
    next_ = (next_ + 1) % kCapacity;
    if (count_ < kCapacity)
      ++count_;
  }

  void Clear() {
    next_ = 0;
    count_ = 0;
  }

  std::size_t size() const { return count_; }

  const FrameTimings& At(std::size_t index) const {
    const std::size_t oldest = (next_ + kCapacity - count_) % kCapacity;
    return samples_[(oldest + index) % kCapacity];
  }

 private:
  std::array<FrameTimings, kCapacity> samples_{};
  std::size_t next_ = 0;
  std::size_t count_ = 0;
};

struct RenderStats {
  RenderFrameCounters current;
  RenderFrameCounters last;

  FrameTimings timings;
  FrameTimeHistory frame_history;
  FrameTimingRing timing_history;

  void EndFrame() {
    last = current;
    current = RenderFrameCounters();
  }
};

inline RenderStats& GetRenderStats() {
  static RenderStats stats;
  return stats;
}

inline void CountSubmit(bool bitmap) {
  RenderStats& stats = GetRenderStats();
  ++stats.current.frame_submits;
  if (bitmap)
    ++stats.current.bitmap_submits;
}

}

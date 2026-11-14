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

namespace urge {

// Per-frame cost measurement, kept out of Graphics so the singleton only
// orchestrates the frame instead of carrying the timing bookkeeping.
class Profiler {
 public:
  enum Kind : std::uint8_t { kRender, kLogic, kOther, kPresent };

  // Time a span of the given kind; EndRecord() stores the elapsed milliseconds.
  void BeginRecord(Kind kind);
  void EndRecord();

  // The script span runs from the end of the previous frame to the start of
  // this one, so it is anchored to a stored tick rather than a BeginRecord()
  // call.  BeginFrame() returns that span; EndFrame() returns the whole update
  // span and re-anchors the tick for the next frame.
  double BeginFrame(std::uint64_t frame_begin);
  double EndFrame(std::uint64_t frame_begin);
  void ResumeFromBackground();

  float render_cost_ms() const { return static_cast<float>(cost_ms_[kRender]); }
  float logic_cost_ms() const { return static_cast<float>(cost_ms_[kLogic]); }
  float other_cost_ms() const { return static_cast<float>(cost_ms_[kOther]); }
  double present_cost_ms() const { return cost_ms_[kPresent]; }

 private:
  static constexpr std::size_t kKindCount = 4;

  Kind kind_ = kRender;
  std::uint64_t begin_tick_ = 0;
  std::uint64_t last_update_end_tick_ = 0;
  double cost_ms_[kKindCount] = {};
};

}  // namespace urge
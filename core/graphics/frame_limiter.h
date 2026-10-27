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

class FrameLimiter {
 public:
  void SetRate(int32_t rate) {
    const uint64_t frequency = SDL_GetPerformanceFrequency();
    frequency_ = frequency;

    if (rate <= 0 || frequency == 0) {
      period_ = 0;
      next_ = 0;
      return;
    }

    period_ = frequency / static_cast<uint64_t>(rate);
    next_ = 0;
  }

  int32_t rate() const {
    if (period_ == 0 || frequency_ == 0)
      return 0;
    return static_cast<int32_t>(frequency_ / period_);
  }

  double Wait(uint64_t frame_begin) {
    if (period_ == 0 || frequency_ == 0)
      return 0.0;

    if (next_ == 0)
      next_ = frame_begin + period_;

    if (frame_begin > next_ + period_)
      next_ = frame_begin + period_;

    const uint64_t now = SDL_GetPerformanceCounter();
    if (next_ <= now) {
      next_ += period_;
      return 0.0;
    }

    const uint64_t remaining = next_ - now;
    const uint64_t nanoseconds = remaining * 1'000'000'000ull / frequency_;
    SDL_DelayPrecise(nanoseconds);

    next_ += period_;
    return static_cast<double>(remaining) * 1000.0 /
           static_cast<double>(frequency_);
  }

  void Reset() { next_ = 0; }

 private:
  uint64_t frequency_ = 0;
  uint64_t period_ = 0;
  uint64_t next_ = 0;
};

}

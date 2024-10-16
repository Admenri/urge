// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "components/fpslimiter/fpslimiter.h"

#include "SDL3/SDL_timer.h"

#include <stdint.h>
#include <algorithm>
#include <cmath>

#define NS_PER_S 1e9

namespace fpslimiter {

FPSLimiter::FPSLimiter(int frame_rate)
    : last_tick_count_(SDL_GetPerformanceCounter()),
      tick_freq_(SDL_GetPerformanceFrequency()),
      tick_freq_ns_((double)tick_freq_ / NS_PER_S) {
  SetFrameRate(frame_rate);
}

void FPSLimiter::SetFrameRate(int frame_rate) {
  ticks_per_frame_ = tick_freq_ / frame_rate;
}

void FPSLimiter::Delay() {
  int64_t frame_delta = SDL_GetPerformanceCounter() - last_tick_count_;
  int64_t delay_tick = ticks_per_frame_ - frame_delta;
  delay_tick = std::max<int64_t>(0, delay_tick);

  SDL_DelayNS(delay_tick / tick_freq_ns_);

  last_tick_count_ = SDL_GetPerformanceCounter();
}

}  // namespace fpslimiter

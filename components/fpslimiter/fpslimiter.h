// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FPSLIMITER_FPSLIMITER_H_
#define COMPONENTS_FPSLIMITER_FPSLIMITER_H_

#include <stdint.h>

namespace fpslimiter {

class FPSLimiter {
 public:
  FPSLimiter(int frame_rate);

  FPSLimiter(const FPSLimiter&) = delete;
  FPSLimiter& operator=(const FPSLimiter&) = delete;

  void SetDisabled(bool disable);
  bool IsDisabled() const { return disabled_; }

  void SetFrameRate(int frame_rate);
  void Delay();
  bool RequireFrameSkip();
  void Reset();

 private:
  bool disabled_;
  uint64_t last_tick_count_;
  int64_t ticks_per_frame_;
  const uint64_t tick_freq_;
  const double tick_freq_ns_;
  uint64_t skip_last_;
  int64_t skip_ideal_diff_;
  bool skip_reset_flag_;
};

}  // namespace fpslimiter

#endif  // !COMPONENTS_FPSLIMITER_FPSLIMITER_H_

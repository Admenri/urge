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

#include <array>
#include "core/audio/decoders/decoder.h"

namespace urge::audioformat {
struct Region {
  uint16_t bank = 0;
  uint8_t program = 0;
  bool drums = false;
  uint8_t key_lo = 0, key_hi = 127, vel_lo = 0, vel_hi = 127;
  uint32_t start = 0, end = 0, loop_start = 0, loop_end = 0;
  uint32_t rate = 22050;
  int root = 60, exclusive = 0;
  float tune = 0, scale = 100, gain = 1, pan = 0;
  float attack = 0.001f, hold = 0, decay = 0.1f;
  float sustain = 1, release = 0.2f;
  // 0 = one shot, 1 = continuous loop, 3 = loop until release.
  uint8_t loop = 0;
};
struct SoundBank {
  std::vector<float> samples;
  std::vector<Region> regions;
};
}  // namespace urge::audioformat

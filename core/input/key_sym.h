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
#include <string>

namespace urge {

/* One entry of the input binding table: a slot name (`A`, `LEFT`, ...) and the
   physical control that drives it.

   `gamepad` selects the numbering space of `code` -- an SDL scancode for the
   keyboard, a `Gamepad` button number otherwise.  Both spaces are small
   integers that overlap, so without the flag the pad's `A` and the keyboard's
   `A` would be the same entry. */
struct KeySym {
  std::string name;
  int32_t code = 0;
  bool gamepad = false;

  bool operator==(const KeySym& other) const {
    return name == other.name && code == other.code &&
           gamepad == other.gamepad;
  }

  bool operator!=(const KeySym& other) const { return !(*this == other); }
};

/* Pressed / triggered / repeated bookkeeping, one per physical control.

   The keyboard and the pad share this machine on purpose: RGSS repeats after a
   short delay and then every few frames, and the two devices must not drift
   apart.  `Update` is fed the polled state once per frame. */
struct KeyState {
  bool pressed = false;
  bool trigger = false;
  bool repeat = false;
  int32_t repeat_count = 0;

  void Update(bool down) {
    trigger = !pressed && down;
    pressed = down;

    repeat = false;
    if (pressed) {
      ++repeat_count;
      repeat = repeat_count == 1 ||
               (repeat_count >= 23 && (repeat_count + 1) % 6 == 0);
    } else {
      repeat_count = 0;
    }
  }

  /* Drop the edges but keep what is held: what a key does when the settings
     overlay takes the input away (its events stop arriving, so it neither
     repeats nor fires again until the overlay closes). */
  void Freeze() {
    trigger = false;
    repeat = false;
  }
};

}  // namespace urge

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
#include <string>
#include <vector>

#include "SDL3/SDL_events.h"

#include "core/object.h"

namespace urge {

inline const struct {
  std::string name;
  int32_t key_id;
} kKeyboardBindings[] = {
    {"DOWN", 2},   {"LEFT", 4},  {"RIGHT", 6}, {"UP", 8},

    {"A", 11},     {"B", 12},    {"C", 13},    {"X", 14},  {"Y", 15},
    {"Z", 16},     {"L", 17},    {"R", 18},

    {"SHIFT", 21}, {"CTRL", 22}, {"ALT", 23},

    {"F5", 25},    {"F6", 26},   {"F7", 27},   {"F8", 28}, {"F9", 29},
};

class Input : public Singleton<Input> {
 public:
  Input(int32_t version);
  ~Input();

  /*-export.begin-*/
  void Update();
  bool Pressed(std::string sym);
  bool Triggered(std::string sym);
  bool Repeated(std::string sym);
  int32_t Dir4();
  int32_t Dir8();

  bool KeyPressed(int32_t keycode);
  bool KeyTriggered(int32_t keycode);
  bool KeyRepeated(int32_t keycode);
  /*-export.end-*/

 public:
  void ProcessEvents(SDL_Event* event);

  // sym -> keycode
  using KeySym = std::pair<std::string, int32_t>;
  void SetKeyBinding(std::vector<KeySym> bindings) { bindings_ = bindings_; }

 private:
  void UpdateDir4();
  void UpdateDir8();

  struct {
    bool pressed = false;
    bool trigger = false;
    bool repeat = false;
    int32_t repeat_count = 0;
  } states_[SDL_SCANCODE_COUNT];

  struct {
    int32_t active = 0;
    int32_t previous = 0;
  } dir4_state_;

  struct {
    int32_t active = 0;
  } dir8_state_;

  std::array<bool, SDL_SCANCODE_COUNT> pressed_;
  std::vector<KeySym> bindings_;
};

}  // namespace urge
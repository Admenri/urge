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

#include "core/input.h"

#include "SDL3/SDL_scancode.h"

namespace urge {

const Input::KeySym kDefaultKeyboardBindings[] = {
    {"DOWN", SDL_SCANCODE_DOWN},    {"LEFT", SDL_SCANCODE_LEFT},
    {"RIGHT", SDL_SCANCODE_RIGHT},  {"UP", SDL_SCANCODE_UP},

    {"F5", SDL_SCANCODE_F5},        {"F6", SDL_SCANCODE_F6},
    {"F7", SDL_SCANCODE_F7},        {"F8", SDL_SCANCODE_F8},
    {"F9", SDL_SCANCODE_F9},

    {"SHIFT", SDL_SCANCODE_LSHIFT}, {"SHIFT", SDL_SCANCODE_RSHIFT},
    {"CTRL", SDL_SCANCODE_LCTRL},   {"CTRL", SDL_SCANCODE_RCTRL},
    {"ALT", SDL_SCANCODE_LALT},     {"ALT", SDL_SCANCODE_RALT},

    {"A", SDL_SCANCODE_LSHIFT},     {"A", SDL_SCANCODE_RSHIFT},
    {"B", SDL_SCANCODE_ESCAPE},     {"B", SDL_SCANCODE_KP_0},
    {"B", SDL_SCANCODE_X},          {"C", SDL_SCANCODE_SPACE},
    {"C", SDL_SCANCODE_RETURN},     {"X", SDL_SCANCODE_A},
    {"Y", SDL_SCANCODE_S},          {"Z", SDL_SCANCODE_D},
    {"L", SDL_SCANCODE_Q},          {"R", SDL_SCANCODE_W},
};

const Input::KeySym kKeyboardBindings1[] = {
    {"A", SDL_SCANCODE_Z},
    {"C", SDL_SCANCODE_C},
};

const Input::KeySym kKeyboardBindings2[] = {
    {"C", SDL_SCANCODE_Z},
};

const std::string kArrowDirsSymbol[] = {
    "DOWN",
    "LEFT",
    "RIGHT",
    "UP",
};

const std::string kButtonItems[] = {
    "A", "B", "C", "X", "Y", "Z", "L", "R", "DOWN", "LEFT", "RIGHT", "UP",
};

Input::Input(int32_t version) {
  SDL_AddEventWatch(
      [](void* userdata, SDL_Event* event) -> bool {
        Input::Get().ProcessEvents(event);
        return true;
      },
      nullptr);

  for (size_t i = 0; i < std::size(kDefaultKeyboardBindings); ++i)
    bindings_.push_back(kDefaultKeyboardBindings[i]);

  // == XP
  if (version == 1)
    for (size_t i = 0; i < std::size(kKeyboardBindings1); ++i)
      bindings_.push_back(kKeyboardBindings1[i]);

  // >= VX
  if (version >= 2)
    for (size_t i = 0; i < std::size(kKeyboardBindings2); ++i)
      bindings_.push_back(kKeyboardBindings2[i]);
}

Input::~Input() = default;

void Input::Update() {
  for (int32_t i = 0; i < std::size(states_); ++i) {
    bool key_pressed = pressed_[i];

    // Update key state with elder state
    states_[i].trigger = !states_[i].pressed && key_pressed;

    // After trigger set, set press state
    states_[i].pressed = key_pressed;

    // Based on press state update the repeat state
    states_[i].repeat = false;
    if (states_[i].pressed) {
      ++states_[i].repeat_count;

      bool repeated = false;
      // TODO: RGSS 1/2/3 specific process
      repeated = states_[i].repeat_count == 1 ||
                 (states_[i].repeat_count >= 23 &&
                  (states_[i].repeat_count + 1) % 6 == 0);

      states_[i].repeat = repeated;
    } else {
      states_[i].repeat_count = 0;
    }
  }

  UpdateDir4();
  UpdateDir8();
}

bool Input::Pressed(std::string sym) {
  if (sym.empty())
    return false;

  for (auto& it : bindings_) {
    if (it.first == sym)
      if (states_[it.second].pressed)
        return true;
  }

  return false;
}

bool Input::Triggered(std::string sym) {
  if (sym.empty())
    return false;

  for (auto& it : bindings_) {
    if (it.first == sym)
      if (states_[it.second].trigger)
        return true;
  }

  return false;
}

bool Input::Repeated(std::string sym) {
  if (sym.empty())
    return false;

  for (auto& it : bindings_) {
    if (it.first == sym)
      if (states_[it.second].repeat)
        return true;
  }

  return false;
}

int32_t Input::Dir4() {
  return dir4_state_.active;
}

int32_t Input::Dir8() {
  return dir8_state_.active;
}

bool Input::KeyPressed(int32_t keycode) {
  if (keycode >= 0 && keycode < std::size(states_))
    return states_[keycode].pressed;
  return false;
}

bool Input::KeyTriggered(int32_t keycode) {
  if (keycode >= 0 && keycode < std::size(states_))
    return states_[keycode].trigger;
  return false;
}

bool Input::KeyRepeated(int32_t keycode) {
  if (keycode >= 0 && keycode < std::size(states_))
    return states_[keycode].repeat;
  return false;
}

std::string Input::GetKeyName(int32_t keycode) {
  return SDL_GetScancodeName(static_cast<SDL_Scancode>(keycode));
}

void Input::ProcessEvents(SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
      pressed_[event->key.scancode] = event->key.down;
      break;
    default:
      break;
  }
}

void Input::UpdateDir4() {
  bool key_states[std::size(kArrowDirsSymbol)] = {0};
  for (auto& it : bindings_)
    for (size_t i = 0; i < std::size(kArrowDirsSymbol); ++i)
      if (it.first == kArrowDirsSymbol[i])
        key_states[i] |= states_[it.second].pressed;

  int32_t dir_flag = 0;
  const int32_t dir_flags_fix[] = {
      1 << 1,
      1 << 2,
      1 << 3,
      1 << 4,
  };

  const int32_t block_dir_flags[] = {dir_flags_fix[0] | dir_flags_fix[3],
                                     dir_flags_fix[1] | dir_flags_fix[2]};

  const int32_t other_dirs[][3] = {
      {1, 2, 3},
      {0, 3, 2},
      {0, 3, 1},
      {1, 2, 0},
  };

  for (size_t i = 0; i < 4; ++i)
    dir_flag |= (key_states[i] ? dir_flags_fix[i] : 0);

  if (dir_flag == block_dir_flags[0] || dir_flag == block_dir_flags[1]) {
    dir4_state_.active = 0;
    return;
  }

  if (dir4_state_.previous) {
    if (key_states[dir4_state_.previous / 2 - 1]) {
      for (size_t i = 0; i < 3; ++i) {
        int32_t other_key = other_dirs[dir4_state_.previous / 2 - 1][i];
        if (!key_states[other_key])
          continue;

        dir4_state_.active = (other_key + 1) * 2;
        return;
      }
    }
  }

  for (int32_t i = 0; i < 4; ++i) {
    if (!key_states[i])
      continue;

    dir4_state_.active = (i + 1) * 2;
    dir4_state_.previous = (i + 1) * 2;
    return;
  }

  dir4_state_.active = 0;
  dir4_state_.previous = 0;
}

void Input::UpdateDir8() {
  bool key_states[std::size(kArrowDirsSymbol)] = {0};
  for (auto& it : bindings_)
    for (size_t i = 0; i < std::size(kArrowDirsSymbol); ++i)
      if (it.first == kArrowDirsSymbol[i])
        key_states[i] |= states_[it.second].pressed;

  static const int32_t combos[4][4] = {
      {2, 1, 3, 0}, {1, 4, 0, 7}, {3, 0, 6, 9}, {0, 7, 9, 8}};

  const int32_t other_dirs[][3] = {
      {1, 2, 3},
      {0, 3, 2},
      {0, 3, 1},
      {1, 2, 0},
  };

  dir8_state_.active = 0;

  for (int32_t i = 0; i < 4; ++i) {
    if (!key_states[i])
      continue;

    for (int32_t j = 0; j < 3; ++j) {
      int32_t other_key = other_dirs[i][j];
      if (!key_states[other_key])
        continue;

      dir8_state_.active = combos[i][other_key];
      return;
    }

    dir8_state_.active = (i + 1) * 2;
    return;
  }
}

}  // namespace urge
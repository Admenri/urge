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

#include "core/input/input.h"

#include <algorithm>

#include "SDL3/SDL_scancode.h"
#include "SDL3/SDL_timer.h"

#include "core/graphics/graphics.h"
#include "core/system/config.h"

namespace urge {

const Input::KeySym kDefaultGamepadBindings[] = {
    {"C", SDL_GAMEPAD_BUTTON_SOUTH, true},
    {"B", SDL_GAMEPAD_BUTTON_EAST, true},
    {"A", SDL_GAMEPAD_BUTTON_WEST, true},
    {"X", SDL_GAMEPAD_BUTTON_NORTH, true},

    {"L", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true},
    {"R", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true},

    {"LEFT", SDL_GAMEPAD_BUTTON_DPAD_LEFT, true},
    {"RIGHT", SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true},
    {"UP", SDL_GAMEPAD_BUTTON_DPAD_UP, true},
    {"DOWN", SDL_GAMEPAD_BUTTON_DPAD_DOWN, true},

    {"LEFT", GamepadDevice::kStickLeft, true},
    {"RIGHT", GamepadDevice::kStickRight, true},
    {"UP", GamepadDevice::kStickUp, true},
    {"DOWN", GamepadDevice::kStickDown, true},
};

namespace {

void AppendGamepadDefaults(std::vector<Input::KeySym>& bindings) {
  for (const auto& binding : kDefaultGamepadBindings)
    bindings.push_back(binding);
}

}

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
    {"C", SDL_SCANCODE_RETURN},     {"C", SDL_SCANCODE_KP_ENTER},
    {"X", SDL_SCANCODE_A},          {"Y", SDL_SCANCODE_S},
    {"Z", SDL_SCANCODE_D},          {"L", SDL_SCANCODE_Q},
    {"R", SDL_SCANCODE_W},

    {"W", SDL_SCANCODE_W},          {"S", SDL_SCANCODE_S},
    {"D", SDL_SCANCODE_D},          {"Q", SDL_SCANCODE_Q},
    {"E", SDL_SCANCODE_E},          {"A", SDL_SCANCODE_A},
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
  std::memset(pressed_, 0, sizeof(pressed_));

  for (size_t i = 0; i < std::size(kDefaultKeyboardBindings); ++i)
    bindings_.push_back(kDefaultKeyboardBindings[i]);

  if (version == 1)
    for (size_t i = 0; i < std::size(kKeyboardBindings1); ++i)
      bindings_.push_back(kKeyboardBindings1[i]);

  if (version >= 2)
    for (size_t i = 0; i < std::size(kKeyboardBindings2); ++i)
      bindings_.push_back(kKeyboardBindings2[i]);

  AppendGamepadDefaults(bindings_);

  const auto& stored = Config::Get().key_bindings;
  if (!stored.empty()) {
    bindings_ = stored;

    const bool has_gamepad =
        std::any_of(bindings_.begin(), bindings_.end(),
                    [](const KeySym& binding) { return binding.gamepad; });
    if (!has_gamepad && !Config::Get().input.pad_bindings)
      AppendGamepadDefaults(bindings_);
  }

  Config::Get().input.pad_bindings = true;
  Config::Get().key_bindings = bindings_;
}

Input::~Input() = default;

void Input::Update() {
  for (int32_t i = 0; i < std::size(states_); ++i)
    states_[i].Update(pressed_[i]);

  gamepads_.Update(!blocked_);

  UpdateDir4();
  UpdateDir8();
}

bool Input::Pressed(std::string sym) {
  if (sym.empty())
    return false;

  for (const auto& it : bindings_)
    if (it.name == sym && BindingPressed(it))
      return true;

  return false;
}

bool Input::Triggered(std::string sym) {
  if (sym.empty())
    return false;

  for (const auto& it : bindings_)
    if (it.name == sym && BindingTriggered(it))
      return true;

  return false;
}

bool Input::Repeated(std::string sym) {
  if (sym.empty())
    return false;

  for (const auto& it : bindings_)
    if (it.name == sym && BindingRepeated(it))
      return true;

  return false;
}

bool Input::BindingPressed(const KeySym& binding) const {
  if (binding.gamepad) {
    const GamepadDevice* pad = gamepads_.primary();
    return pad && pad->Pressed(binding.code);
  }

  return binding.code >= 0 && binding.code < SDL_SCANCODE_COUNT &&
         states_[binding.code].pressed;
}

bool Input::BindingTriggered(const KeySym& binding) const {
  if (binding.gamepad) {
    const GamepadDevice* pad = gamepads_.primary();
    return pad && pad->Triggered(binding.code);
  }

  return binding.code >= 0 && binding.code < SDL_SCANCODE_COUNT &&
         states_[binding.code].trigger;
}

bool Input::BindingRepeated(const KeySym& binding) const {
  if (binding.gamepad) {
    const GamepadDevice* pad = gamepads_.primary();
    return pad && pad->Repeated(binding.code);
  }

  return binding.code >= 0 && binding.code < SDL_SCANCODE_COUNT &&
         states_[binding.code].repeat;
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

bool Input::Emulate(int32_t scancode,
                    bool down,
                    int32_t modifier,
                    bool repeat) {
  SDL_Event event = {};
  event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
  event.key.timestamp = SDL_GetTicksNS();
  event.key.windowID = 0;
  if (auto* graphics = Graphics::TryGet())
    if (SDL_Window* window = graphics->window())
      event.key.windowID = SDL_GetWindowID(window);
  event.key.which = 0;
  event.key.scancode = static_cast<SDL_Scancode>(scancode);
  event.key.mod = static_cast<SDL_Keymod>(modifier);
  event.key.raw = 0;
  event.key.down = down;
  event.key.repeat = repeat;
  event.key.key = SDL_GetKeyFromScancode(
      event.key.scancode, event.key.mod, true);

  return SDL_PushEvent(&event);
}

std::string Input::BindingName(const KeySym& binding) {
  if (binding.gamepad)
    return GamepadDevice::ButtonLabel(binding.code);

  return GetKeyName(binding.code);
}

void Input::ProcessEvents(URGE_EXCEPT, SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
      pressed_[event->key.scancode] = true;
      break;
    case SDL_EVENT_KEY_UP:
      pressed_[event->key.scancode] = false;
      break;
    default:
      break;
  }
}

void Input::ProcessGamepadEvents(SDL_Event* event) {
  gamepads_.ProcessEvent(*event);
}

void Input::UpdateDir4() {
  bool key_states[std::size(kArrowDirsSymbol)] = {0};
  for (const auto& it : bindings_)
    for (size_t i = 0; i < std::size(kArrowDirsSymbol); ++i)
      if (it.name == kArrowDirsSymbol[i])
        key_states[i] |= BindingPressed(it);

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
  for (const auto& it : bindings_)
    for (size_t i = 0; i < std::size(kArrowDirsSymbol); ++i)
      if (it.name == kArrowDirsSymbol[i])
        key_states[i] |= BindingPressed(it);

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

}

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

#include "core/input/gamepad.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_stdinc.h"

#include "core/common/logger.h"
#include "core/input/input.h"

namespace urge {

namespace {

constexpr int16_t kStickDeadzone = 16384;

constexpr const char* kStickTokens[4] = {"lsleft", "lsright", "lsup", "lsdown"};

constexpr const char* kStickLabels[4] = {"Pad L-Stick Left",
                                         "Pad L-Stick Right", "Pad L-Stick Up",
                                         "Pad L-Stick Down"};

float NormalizeAxis(SDL_GamepadAxis axis, Sint16 value) {
  if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ||
      axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
    return std::clamp(value / 32767.0f, 0.0f, 1.0f);

  return std::clamp(value / 32767.0f, -1.0f, 1.0f);
}

Uint16 NormalizeRumble(float value) {
  return static_cast<Uint16>(std::clamp(value, 0.0f, 1.0f) * 65535.0f + 0.5f);
}

}

GamepadDevice::~GamepadDevice() {
  Close();
}

bool GamepadDevice::Open(SDL_JoystickID instance_id) {
  Close();

  SDL_Gamepad* pad = SDL_OpenGamepad(instance_id);
  if (!pad) {
    LOGGER_WARN("[Gamepad] cannot open {}: {}",
                static_cast<int32_t>(instance_id), SDL_GetError());
    return false;
  }

  pad_ = pad;
  instance_id_ = SDL_GetGamepadID(pad);
  ResetState();
  return true;
}

void GamepadDevice::Close() {
  if (pad_)
    SDL_CloseGamepad(pad_);

  pad_ = nullptr;
  instance_id_ = 0;
  ResetState();
}

std::string GamepadDevice::name() const {
  if (!pad_)
    return {};

  const char* name = SDL_GetGamepadName(pad_);
  return (name && *name) ? std::string(name) : std::string();
}

void GamepadDevice::ProcessEvent(const SDL_Event& event) {
  if (event.type != SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
      event.type != SDL_EVENT_GAMEPAD_BUTTON_UP)
    return;

  if (!pad_ || event.gbutton.which != instance_id_)
    return;

  const int32_t button = event.gbutton.button;
  if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT)
    raw_[button] = event.gbutton.down;
}

void GamepadDevice::Update(bool accept_input) {
  if (!accept_input) {
    for (auto& state : states_)
      state.Freeze();
    return;
  }

  SampleAxes();

  for (int32_t i = 0; i < kButtonCount; ++i)
    states_[i].Update(raw_[i]);
}

bool GamepadDevice::Pressed(int32_t button) const {
  if (button < 0 || button >= kButtonCount)
    return false;
  return states_[button].pressed;
}

bool GamepadDevice::Triggered(int32_t button) const {
  if (button < 0 || button >= kButtonCount)
    return false;
  return states_[button].trigger;
}

bool GamepadDevice::Repeated(int32_t button) const {
  if (button < 0 || button >= kButtonCount)
    return false;
  return states_[button].repeat;
}

float GamepadDevice::Axis(int32_t axis) const {
  if (!pad_ || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT)
    return 0.0f;

  const auto sdl_axis = static_cast<SDL_GamepadAxis>(axis);
  return NormalizeAxis(sdl_axis, SDL_GetGamepadAxis(pad_, sdl_axis));
}

bool GamepadDevice::Rumble(float low, float high, int32_t duration_ms) {
  if (!pad_)
    return false;

  return SDL_RumbleGamepad(pad_, NormalizeRumble(low), NormalizeRumble(high),
                           static_cast<Uint32>(std::max(duration_ms, 0)));
}

std::string GamepadDevice::ButtonToken(int32_t button) {
  if (button >= kStickLeft && button < kButtonCount)
    return kStickTokens[button - kStickLeft];

  if (button >= 0 && button < SDL_GAMEPAD_BUTTON_COUNT) {
    const char* token =
        SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(button));
    if (token && *token)
      return token;
  }

  return std::to_string(button);
}

std::string GamepadDevice::ButtonLabel(int32_t button) {
  switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
      return "Pad A";
    case SDL_GAMEPAD_BUTTON_EAST:
      return "Pad B";
    case SDL_GAMEPAD_BUTTON_WEST:
      return "Pad X";
    case SDL_GAMEPAD_BUTTON_NORTH:
      return "Pad Y";
    case SDL_GAMEPAD_BUTTON_BACK:
      return "Pad Back";
    case SDL_GAMEPAD_BUTTON_GUIDE:
      return "Pad Guide";
    case SDL_GAMEPAD_BUTTON_START:
      return "Pad Start";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
      return "Pad L3";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
      return "Pad R3";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
      return "Pad LB";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
      return "Pad RB";
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      return "Pad D-Up";
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      return "Pad D-Down";
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      return "Pad D-Left";
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      return "Pad D-Right";
    case SDL_GAMEPAD_BUTTON_MISC1:
      return "Pad Misc";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
      return "Pad R-Paddle1";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
      return "Pad L-Paddle1";
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
      return "Pad R-Paddle2";
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
      return "Pad L-Paddle2";
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
      return "Pad Touchpad";
    default:
      break;
  }

  if (button >= kStickLeft && button < kButtonCount)
    return kStickLabels[button - kStickLeft];

  std::string token = ButtonToken(button);
  for (char& c : token)
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return "Pad " + token;
}

bool GamepadDevice::ButtonFromToken(const std::string& token, int32_t* button) {
  for (int32_t i = 0; i < 4; ++i) {
    if (token == kStickTokens[i]) {
      if (button)
        *button = kStickLeft + i;
      return true;
    }
  }

  const SDL_GamepadButton found = SDL_GetGamepadButtonFromString(token.c_str());
  if (found == SDL_GAMEPAD_BUTTON_INVALID)
    return false;

  if (button)
    *button = static_cast<int32_t>(found);
  return true;
}

void GamepadDevice::SampleAxes() {
  const bool active = pad_ != nullptr;

  const int16_t x =
      active ? SDL_GetGamepadAxis(pad_, SDL_GAMEPAD_AXIS_LEFTX) : 0;
  const int16_t y =
      active ? SDL_GetGamepadAxis(pad_, SDL_GAMEPAD_AXIS_LEFTY) : 0;

  raw_[kStickLeft] = x < -kStickDeadzone;
  raw_[kStickRight] = x > kStickDeadzone;
  raw_[kStickUp] = y < -kStickDeadzone;
  raw_[kStickDown] = y > kStickDeadzone;
}

void GamepadDevice::ResetState() {
  std::memset(raw_, 0, sizeof(raw_));
  for (auto& state : states_)
    state = KeyState{};
}

GamepadManager::GamepadManager() {
  if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
    LOGGER_WARN("[Gamepad] subsystem unavailable: {}", SDL_GetError());
    return;
  }

  ready_ = true;
  OpenExisting();

  int32_t joystick_count = 0;
  SDL_JoystickID* joysticks = SDL_GetJoysticks(&joystick_count);
  SDL_free(joysticks);

  LOGGER_INFO("[Gamepad] {} joystick(s), {} gamepad(s)", joystick_count,
              count());
}

GamepadManager::~GamepadManager() {
  devices_.clear();
}

void GamepadManager::ProcessEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_GAMEPAD_ADDED:
      Add(event.gdevice.which);
      break;

    case SDL_EVENT_GAMEPAD_REMOVED:
      Remove(event.gdevice.which);
      break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      for (auto& device : devices_)
        device->ProcessEvent(event);
      break;

    default:
      break;
  }
}

void GamepadManager::Update(bool accept_input) {
  for (auto& device : devices_)
    device->Update(accept_input);
}

GamepadDevice* GamepadManager::device(int32_t index) const {
  if (index < 0 || index >= count())
    return nullptr;
  return devices_[index].get();
}

void GamepadManager::Add(SDL_JoystickID instance_id) {
  if (!ready_)
    return;

  for (const auto& device : devices_)
    if (device->instance_id() == instance_id)
      return;

  auto device = std::make_unique<GamepadDevice>();
  if (!device->Open(instance_id))
    return;

  LOGGER_INFO("[Gamepad] {} connected (index {})", device->name(), count());
  devices_.push_back(std::move(device));
}

void GamepadManager::Remove(SDL_JoystickID instance_id) {
  for (auto it = devices_.begin(); it != devices_.end(); ++it) {
    if ((*it)->instance_id() != instance_id)
      continue;

    LOGGER_INFO("[Gamepad] {} disconnected", (*it)->name());
    devices_.erase(it);
    return;
  }
}

void GamepadManager::OpenExisting() {
  int32_t gamepad_count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&gamepad_count);
  if (!ids)
    return;

  for (int32_t i = 0; i < gamepad_count; ++i)
    Add(ids[i]);

  SDL_free(ids);
}

Gamepad::Gamepad(int32_t device_id) : id_(device_id) {
  device_ = SDL_OpenGamepad(device_id);
  if (!device_) {
    throw Exception(Exception::kRGSSError, "failed to open gamepad {}: {}",
                    device_id, SDL_GetError());
  }

  char text[33] = {};
  SDL_GUIDToString(SDL_GetGamepadGUIDForID(id_), text, sizeof(text));
  guid_ = text;
}

Gamepad::~Gamepad() {
  Close();
}

void Gamepad::Close() {
  if (device_)
    SDL_CloseGamepad(device_);

  device_ = nullptr;

  for (auto& state : states_)
    state = KeyState{};
}

std::vector<Gamepad::DeviceInfo> Gamepad::GetAvailableDevices() {
  std::vector<DeviceInfo> result;

  int32_t count = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&count);
  if (!pads)
    return result;

  result.reserve(static_cast<size_t>(count));

  for (int32_t i = 0; i < count; ++i) {
    DeviceInfo info;
    info.id = pads[i];

    if (const char* name = SDL_GetGamepadNameForID(pads[i]))
      info.name = name;
    if (const char* path = SDL_GetGamepadPathForID(pads[i]))
      info.path = path;

    info.player_index = SDL_GetGamepadPlayerIndexForID(pads[i]);

    char text[33] = {};
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(pads[i]), text, sizeof(text));
    info.guid = text;

    result.push_back(std::move(info));
  }

  SDL_free(pads);
  return result;
}

void Gamepad::Update() {
  const bool connected = device_ && SDL_GamepadConnected(device_);

  for (int32_t i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i) {
    const bool down =
        connected &&
        SDL_GetGamepadButton(device_, static_cast<SDL_GamepadButton>(i));
    states_[i].Update(down);
  }
}

bool Gamepad::Connect() {
  if (device_ && SDL_GamepadConnected(device_))
    return true;

  id_ = 0;

  int32_t count = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&count);
  if (pads) {
    for (int32_t i = 0; i < count; ++i) {
      char text[33] = {};
      SDL_GUIDToString(SDL_GetGamepadGUIDForID(pads[i]), text, sizeof(text));
      if (guid_ == text) {
        id_ = pads[i];
        break;
      }
    }
    SDL_free(pads);
  }

  Close();

  if (!id_)
    return false;

  device_ = SDL_OpenGamepad(id_);
  return device_ != nullptr;
}

int32_t Gamepad::GetConnectState() {
  if (!device_ || !SDL_GamepadConnected(device_))
    return SDL_JOYSTICK_CONNECTION_INVALID;

  return static_cast<int32_t>(SDL_GetGamepadConnectionState(device_));
}

int32_t Gamepad::GetAxisValue(int32_t axis) {
  if (!device_ || axis < 0 || axis >= SDL_GAMEPAD_AXIS_COUNT)
    return 0;

  return SDL_GetGamepadAxis(device_, static_cast<SDL_GamepadAxis>(axis));
}

bool Gamepad::IsButtonPressed(int32_t button) {
  if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
    return false;
  return states_[button].pressed;
}

bool Gamepad::IsButtonTriggered(int32_t button) {
  if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
    return false;
  return states_[button].trigger;
}

bool Gamepad::IsButtonRepeated(int32_t button) {
  if (button < 0 || button >= SDL_GAMEPAD_BUTTON_COUNT)
    return false;
  return states_[button].repeat;
}

std::vector<int32_t> Gamepad::GetPressedButtons() {
  std::vector<int32_t> result;

  for (int32_t i = 0; i < SDL_GAMEPAD_BUTTON_COUNT; ++i)
    if (states_[i].pressed)
      result.push_back(i);

  return result;
}

ATTR_DEF(Gamepad, int32_t, PlayerIndex) {
  if (value.has_value()) {
    if (device_)
      SDL_SetGamepadPlayerIndex(device_, *value);
    return std::nullopt;
  }

  return device_ ? SDL_GetGamepadPlayerIndex(device_) : -1;
}

bool Gamepad::Rumble(float low, float high, int32_t duration) {
  if (!device_ || !SDL_GamepadConnected(device_))
    return false;

  return SDL_RumbleGamepad(device_, NormalizeRumble(low), NormalizeRumble(high),
                           static_cast<Uint32>(std::max(duration, 0)));
}

}

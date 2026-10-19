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
#include <memory>
#include <string>
#include <vector>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_gamepad.h"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
#include "core/input/key_sym.h"

namespace urge {

/* One physical controller: the SDL handle, the raw button set, the axes and
   rumble.  Internal -- the script side goes through `Gamepad` below, which is
   a handle onto one of these by index.

   Buttons are numbered with SDL's `SDL_GamepadButton` values, and the left
   stick is folded into the same numbering as four virtual buttons
   (`kStickLeft` ...) so a slot can bind "stick left" exactly like "d-pad
   left".  The right stick is deliberately absent from that set: it is a
   camera control and no RGSS slot asks for one -- `Gamepad#right_stick`
   exposes it as an axis instead. */
class GamepadDevice {
 public:
  enum VirtualButton : int32_t {
    kStickLeft = SDL_GAMEPAD_BUTTON_COUNT,
    kStickRight,
    kStickUp,
    kStickDown,
    kButtonCount,
  };

  GamepadDevice() = default;
  ~GamepadDevice();

  GamepadDevice(const GamepadDevice&) = delete;
  GamepadDevice& operator=(const GamepadDevice&) = delete;

  bool Open(SDL_JoystickID instance_id);
  void Close();

  bool connected() const { return pad_ != nullptr; }
  SDL_JoystickID instance_id() const { return instance_id_; }
  std::string name() const;

  void ProcessEvent(const SDL_Event& event);
  void Update(bool accept_input);

  bool Pressed(int32_t button) const;
  bool Triggered(int32_t button) const;
  bool Repeated(int32_t button) const;

  /* -1..1 for sticks, 0..1 for triggers (SDL's own ranges, normalized). */
  float Axis(int32_t axis) const;

  bool Rumble(float low, float high, int32_t duration_ms);

  /* Stable token for the ini file ("a", "leftshoulder", "lsleft", ...). */
  static std::string ButtonToken(int32_t button);

  /* Human-readable form for the settings UI ("Pad A", "Pad LB", ...). */
  static std::string ButtonLabel(int32_t button);

  /* Inverse of `ButtonToken`. */
  static bool ButtonFromToken(const std::string& token, int32_t* button);

 private:
  void SampleAxes();
  void ResetState();

  SDL_Gamepad* pad_ = nullptr;
  SDL_JoystickID instance_id_ = 0;

  bool raw_[kButtonCount] = {};
  KeyState states_[kButtonCount];
};

/* Every attached controller, in the order they were plugged in.

   Indices are positions in that list, so unplugging a pad shifts the ones
   behind it down -- a handle taken before the change may then refer to a
   different pad.  The list is compact on purpose: `count` and the valid index
   range always agree, which is the least surprising thing for a script. */
class GamepadManager {
 public:
  GamepadManager();
  ~GamepadManager();

  GamepadManager(const GamepadManager&) = delete;
  GamepadManager& operator=(const GamepadManager&) = delete;

  void ProcessEvent(const SDL_Event& event);
  void Update(bool accept_input);

  int32_t count() const { return static_cast<int32_t>(devices_.size()); }
  GamepadDevice* device(int32_t index) const;
  GamepadDevice* primary() const {
    return devices_.empty() ? nullptr : devices_.front().get();
  }

 private:
  void Add(SDL_JoystickID instance_id);
  void Remove(SDL_JoystickID instance_id);
  void OpenExisting();

  std::vector<std::unique_ptr<GamepadDevice>> devices_;
  bool ready_ = false;
};

/* One controller, owned by the script.

     Gamepad.available_devices        # => [{ id:, name:, path:, player:, guid: }, ...]
     pad = Gamepad.new(device_id)     # opens the device
     pad.update                       # poll once per frame
     pad.button_press?(Gamepad::BUTTON_SOUTH)
     pad.button_trigger?(Gamepad::BUTTON_SOUTH)
     pad.button_repeat?(Gamepad::BUTTON_SOUTH)
     pad.get_axis_value(Gamepad::AXIS_LEFTX)      # raw SDL value
     pad.pressed_buttons              # => [button, ...]
     pad.connect_state                # SDL_JoystickConnectionState
     pad.player_index / pad.player_index = 1
     pad.rumble(0.5, 0.5, 200)        # engine extension

   The object owns its own `SDL_Gamepad`, so a script may hold several pads at
   once.  `update` polls the device (no SDL events involved), which is why a
   pad created here works independently of `Input`'s slot bindings -- those
   follow the primary pad through `GamepadManager` instead. */
URGE_BINDING()
class Gamepad : public Object {
 public:
  /* One entry of `Gamepad.available_devices`; the glue turns it into a Hash. */
  struct DeviceInfo {
    int32_t id = 0;
    std::string name;
    std::string path;
    int32_t player_index = -1;
    std::string guid;
  };

  static std::vector<DeviceInfo> GetAvailableDevices();

  /* SDL's axis numbering, under the names the reference API uses. */
  URGE_BINDING()
  enum Axis {
    AXIS_LEFTX = SDL_GAMEPAD_AXIS_LEFTX,
    AXIS_LEFTY = SDL_GAMEPAD_AXIS_LEFTY,
    AXIS_RIGHTX = SDL_GAMEPAD_AXIS_RIGHTX,
    AXIS_RIGHTY = SDL_GAMEPAD_AXIS_RIGHTY,
    AXIS_LEFT_TRIGGER = SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
    AXIS_RIGHT_TRIGGER = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
  };

  /* SDL's button numbering, under the names the reference API uses. */
  URGE_BINDING()
  enum Button {
    BUTTON_SOUTH = SDL_GAMEPAD_BUTTON_SOUTH,
    BUTTON_EAST = SDL_GAMEPAD_BUTTON_EAST,
    BUTTON_WEST = SDL_GAMEPAD_BUTTON_WEST,
    BUTTON_NORTH = SDL_GAMEPAD_BUTTON_NORTH,
    BUTTON_BACK = SDL_GAMEPAD_BUTTON_BACK,
    BUTTON_GUIDE = SDL_GAMEPAD_BUTTON_GUIDE,
    BUTTON_START = SDL_GAMEPAD_BUTTON_START,
    BUTTON_LEFT_STICK = SDL_GAMEPAD_BUTTON_LEFT_STICK,
    BUTTON_RIGHT_STICK = SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    BUTTON_LEFT_SHOULDER = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    BUTTON_RIGHT_SHOULDER = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    BUTTON_DPAD_UP = SDL_GAMEPAD_BUTTON_DPAD_UP,
    BUTTON_DPAD_DOWN = SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    BUTTON_DPAD_LEFT = SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    BUTTON_DPAD_RIGHT = SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    BUTTON_MISC1 = SDL_GAMEPAD_BUTTON_MISC1,
    BUTTON_MISC2 = SDL_GAMEPAD_BUTTON_MISC2,
    BUTTON_MISC3 = SDL_GAMEPAD_BUTTON_MISC3,
    BUTTON_MISC4 = SDL_GAMEPAD_BUTTON_MISC4,
    BUTTON_MISC5 = SDL_GAMEPAD_BUTTON_MISC5,
    BUTTON_MISC6 = SDL_GAMEPAD_BUTTON_MISC6,
    BUTTON_RIGHT_PADDLE1 = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1,
    BUTTON_RIGHT_PADDLE2 = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2,
    BUTTON_LEFT_PADDLE1 = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1,
    BUTTON_LEFT_PADDLE2 = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2,
    BUTTON_TOUCHPAD = SDL_GAMEPAD_BUTTON_TOUCHPAD,
  };

  /* Opens `device_id`; throws RGSSError when the pad is gone. */
  URGE_BINDING()
  Gamepad(int32_t device_id);
  URGE_BINDING()
  ~Gamepad() override;

  /* Poll the buttons.  `button_trigger?` / `button_repeat?` only move here. */
  URGE_BINDING()
  void Update();

  /* Re-open the same physical pad after it was unplugged and plugged back in,
     matched by GUID.  False when it is not present. */
  URGE_BINDING()
  bool Connect();

  URGE_BINDING(Name : "device_id")
  int32_t GetDeviceID() const { return id_; }

  URGE_BINDING(Name : "connect_state")
  int32_t GetConnectState();

  URGE_BINDING()
  int32_t GetAxisValue(int32_t axis);

  URGE_BINDING(Name : "button_press?")
  bool IsButtonPressed(int32_t button);

  URGE_BINDING(Name : "button_trigger?")
  bool IsButtonTriggered(int32_t button);

  URGE_BINDING(Name : "button_repeat?")
  bool IsButtonRepeated(int32_t button);

  URGE_BINDING(Name : "pressed_buttons")
  std::vector<int32_t> GetPressedButtons();

  URGE_BINDING()
  ATTR(int32_t, PlayerIndex);

  /* Not part of the reference API: this engine also drives the motors.  `low`
     and `high` are 0..1, `duration` is milliseconds (0 keeps them running). */
  URGE_BINDING()
  bool Rumble(float low, float high, int32_t duration = 250);

 private:
  void Close();

  SDL_JoystickID id_ = 0;
  SDL_Gamepad* device_ = nullptr;
  std::string guid_;
  KeyState states_[SDL_GAMEPAD_BUTTON_COUNT];
};

}  // namespace urge

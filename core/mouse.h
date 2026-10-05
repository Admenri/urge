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
#include <cstdint>

#include "SDL3/SDL_events.h"
#include "SDL3/SDL_mouse.h"

#include "core/bitmap.h"
#include "core/definition.h"
#include "core/object.h"

namespace urge {

//! The mouse device. Its position is in game-screen coordinates (Graphics
//! size), not window coordinates; its button state is the one the frame of the
//! last Update() closed. A singleton, driven by the engine.
URGE_BINDING()
class Mouse : public Singleton<Mouse> {
 public:
  Mouse();
  ~Mouse();

  //! SDL button values; index 0 is unused because SDL numbers from one.
  enum Button {
    Left = SDL_BUTTON_LEFT,
    Middle = SDL_BUTTON_MIDDLE,
    Right = SDL_BUTTON_RIGHT,
    X1 = SDL_BUTTON_X1,
    X2 = SDL_BUTTON_X2,
  };

  URGE_BINDING()
  void Update();

  URGE_BINDING()
  float X();
  URGE_BINDING()
  float Y();
  URGE_BINDING()
  void SetPosition(float x, float y);

  URGE_BINDING(Name : "down?")
  bool IsDown(int32_t button);
  URGE_BINDING(Name : "up?")
  bool IsUp(int32_t button);
  URGE_BINDING(Name : "double?")
  bool IsDouble(int32_t button);
  URGE_BINDING(Name : "press?")
  bool IsPress(int32_t button);
  URGE_BINDING(Name : "move?")
  bool IsMove(int32_t button);

  URGE_BINDING()
  int32_t ScrollX();
  URGE_BINDING()
  int32_t ScrollY();

  URGE_BINDING()
  void SetCursor(RefPtr<Bitmap> image, int32_t hot_x, int32_t hot_y);

  URGE_BINDING()
  ATTR(bool, Capture);
  URGE_BINDING()
  ATTR(bool, Visible);

  void ProcessEvents(SDL_Event* event);

 private:
  //! A pointer position, in window coordinates (the space SDL reports in).
  struct Point {
    float x = 0.0f;
    float y = 0.0f;
  };

  //! The state of one button during one frame, see Update().
  struct ButtonState {
    //! Held down right now, read by IsPress().
    bool pressed = false;
    //! Pressed during the frame, read by IsDown().
    bool down = false;
    //! Released during the frame, read by IsUp().
    bool up = false;
    //! Click count of the press, 2 on the second one of a double click.
    int32_t clicks = 0;
  };

  //! Button slots; one longer than the buttons because index 0 is unused.
  static constexpr int32_t kButtonCount = SDL_BUTTON_X2 + 1;

  //! Maps a window coordinate onto the game screen.
  static Point WindowToScreenInternal(const Point& position);
  //! Maps a game screen coordinate back into a window coordinate.
  static Point ScreenToWindowInternal(const Point& position);

  //! The state of the buttons as the events of the frame left it.
  std::array<bool, kButtonCount> raw_pressed_ = {};
  //! The click counts as the events of the frame left them.
  std::array<int32_t, kButtonCount> raw_clicks_ = {};

  //! The state the queries read, see Update().
  std::array<ButtonState, kButtonCount> buttons_ = {};

  //! Pointer position in window coordinates, i.e. the raw SDL space.
  Point position_;
  //! Position of the frame before, to tell whether the pointer moved.
  Point last_position_;
  //! Whether the pointer moved during the frame, see Update().
  bool moved_ = false;

  //! Wheel offset the events of the frame accumulated, in notches.
  Point scroll_;
  //! Wheel offset accumulated up to the frame before.
  Point last_scroll_;
  //! Wheel offset of the frame, i.e. scroll_ minus last_scroll_.
  Point scroll_delta_;

  //! The capture state the Capture attribute last asked SDL for.
  bool capture_ = false;
  //! The color cursor of SetCursor(), owned by this class.
  SDL_Cursor* cursor_ = nullptr;
};

}  // namespace urge

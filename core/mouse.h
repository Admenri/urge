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

URGE_BINDING()
class Mouse : public Singleton<Mouse> {
 public:
  Mouse();
  ~Mouse();

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

  struct Point {
    float x = 0.0f;
    float y = 0.0f;
  };

  struct ButtonState {
    bool pressed = false;

    bool down = false;

    bool up = false;

    int32_t clicks = 0;
  };

  static constexpr int32_t kButtonCount = SDL_BUTTON_X2 + 1;

  static Point WindowToScreenInternal(const Point& position);

  static Point ScreenToWindowInternal(const Point& position);

  std::array<bool, kButtonCount> raw_pressed_ = {};

  std::array<int32_t, kButtonCount> raw_clicks_ = {};

  std::array<ButtonState, kButtonCount> buttons_ = {};

  Point position_;

  Point last_position_;

  bool moved_ = false;

  Point scroll_;

  Point last_scroll_;

  Point scroll_delta_;

  bool capture_ = false;

  SDL_Cursor* cursor_ = nullptr;
};

}  // namespace urge

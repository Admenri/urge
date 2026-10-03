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

/**
\brief The mouse device of the engine.

The position the class reports is the one of the pointer inside the game
screen, i.e. the logical size the scene is rendered at -- Graphics::Width() by
Graphics::Height() -- and not the size of the window. A pointer at the centre
of a window whose screen is 544x416 reads (272, 208), whatever the window is
scaled to on the desktop: the window is normally a multiple of the screen by
the density of the display, see Graphics, so SDL reports the pointer in window
coordinates and the class maps them onto the screen. SetPosition() takes the
same screen coordinates and maps them back.

The state of the buttons is the one of the frame the last Update() closed: the
events of a frame are folded into it once, at the end of the event pump of
Graphics::PresentInternal(). Each query then reads:

- IsDown() and IsUp() are the two edges of a button, true during one frame.
- IsPress() is the held state, true for as long as the button is down.
- IsDouble() is true during the frame a press happened which SDL counted as
  the second of a double click.
- IsMove() is a drag: the pointer moved during the frame while the given
  button was held.

ScrollX() and ScrollY() are the wheel offset of the frame in notches, which is
1 or -1 for an ordinary wheel and may be a larger or a fractional value for a
finer one.

A Mouse is a singleton, see Singleton: it owns the one cursor and the one
capture state of the window, so it is reached with Mouse::Get() and driven by
the engine rather than constructed by a script.
*/
class Mouse : public Singleton<Mouse> {
 public:
  Mouse();
  ~Mouse();

  /*! The buttons of SDL, the values the button arguments of the queries take.
      They are not part of the export block -- the binding passes the number of
      a button through -- and index 0 of a button table is unused because SDL
      numbers its buttons from one. */
  enum Button {
    Left = SDL_BUTTON_LEFT,
    Middle = SDL_BUTTON_MIDDLE,
    Right = SDL_BUTTON_RIGHT,
    X1 = SDL_BUTTON_X1,
    X2 = SDL_BUTTON_X2,
  };

  /*-export.begin-*/
  void Update();

  float X();
  float Y();
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

  int32_t ScrollX();
  int32_t ScrollY();

  void SetCursor(RefPtr<Bitmap> image, int32_t hot_x, int32_t hot_y);

  ATTR(bool, Capture);
  ATTR(bool, Visible);
  /*-export.end-*/

  void ProcessEvents(SDL_Event* event);

 private:
  //! A pointer position. The one of this class is in window coordinates, i.e.
  //! the space SDL reports events in; the queries map it onto the screen.
  struct Point {
    float x = 0.0f;
    float y = 0.0f;
  };

  //! The state of one button during one frame, see Update().
  struct ButtonState {
    //! Held down right now, the state IsPress() reads.
    bool pressed = false;
    //! Pressed down during the frame, the state IsDown() reads.
    bool down = false;
    //! Released during the frame, the state IsUp() reads.
    bool up = false;
    //! The click count of the press, 2 on the second one of a double click.
    int32_t clicks = 0;
  };

  /*! The number of button slots. SDL numbers its buttons from one while the
      tables below are indexed by that number, so the tables are one longer
      than the number of buttons and index 0 is unused. */
  static constexpr int32_t kButtonCount = SDL_BUTTON_X2 + 1;

  //! Maps a window coordinate onto the game screen, see the class docs.
  static Point WindowToScreenInternal(const Point& position);
  //! Maps a game screen coordinate back into a window coordinate.
  static Point ScreenToWindowInternal(const Point& position);

  //! The state of the buttons as the events of the frame left it.
  std::array<bool, kButtonCount> raw_pressed_ = {};
  //! The click counts as the events of the frame left them.
  std::array<int32_t, kButtonCount> raw_clicks_ = {};

  //! The state the queries read, see Update().
  std::array<ButtonState, kButtonCount> buttons_ = {};

  //! The pointer position in window coordinates, i.e. the raw SDL space.
  Point position_;
  //! The position of the frame before, to tell whether the pointer moved.
  Point last_position_;
  //! Whether the pointer moved during the frame, see Update().
  bool moved_ = false;

  //! The wheel offset the events of the frame accumulated, in notches.
  Point scroll_;
  //! The wheel offset accumulated up to the frame before.
  Point last_scroll_;
  //! The wheel offset of the frame, i.e. scroll_ minus last_scroll_.
  Point scroll_delta_;

  //! The capture state the Capture attribute last asked SDL for.
  bool capture_ = false;
  //! The color cursor of SetCursor(), owned by this class.
  SDL_Cursor* cursor_ = nullptr;
};

}  // namespace urge

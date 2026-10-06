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

#include "core/mouse.h"

#include <cmath>

#include "core/graphics.h"
#include "core/palette.h"

namespace urge {

namespace {

bool WindowSizeInternal(SDL_Window* window, float* width, float* height) {
  int size_x = 0, size_y = 0;
  if (!window || !SDL_GetWindowSize(window, &size_x, &size_y))
    return false;

  if (size_x <= 0 || size_y <= 0)
    return false;

  *width = static_cast<float>(size_x);
  *height = static_cast<float>(size_y);
  return true;
}

}  // namespace

Mouse::Mouse() = default;

Mouse::~Mouse() {
  if (!cursor_)
    return;

  if (SDL_GetCursor() == cursor_)
    SDL_SetCursor(SDL_GetDefaultCursor());
  SDL_DestroyCursor(cursor_);
}

void Mouse::Update() {
  for (int32_t button = 0; button < kButtonCount; ++button) {
    const bool pressed = raw_pressed_[button];

    buttons_[button].down = !buttons_[button].pressed && pressed;
    buttons_[button].up = buttons_[button].pressed && !pressed;
    buttons_[button].pressed = pressed;

    buttons_[button].clicks = raw_clicks_[button];
  }

  moved_ = position_.x != last_position_.x || position_.y != last_position_.y;
  last_position_ = position_;

  scroll_delta_ = Point{scroll_.x - last_scroll_.x, scroll_.y - last_scroll_.y};
  last_scroll_ = scroll_;
}

float Mouse::X() {
  return WindowToScreenInternal(position_).x;
}

float Mouse::Y() {
  return WindowToScreenInternal(position_).y;
}

void Mouse::SetPosition(float x, float y) {
  SDL_Window* window = Graphics::Get().window();
  if (!window)
    return;

  const Point position = ScreenToWindowInternal(Point{x, y});
  SDL_WarpMouseInWindow(window, position.x, position.y);

  position_ = position;
}

bool Mouse::IsDown(int32_t button) {
  if (button < 0 || button >= kButtonCount)
    return false;
  return buttons_[button].down;
}

bool Mouse::IsUp(int32_t button) {
  if (button < 0 || button >= kButtonCount)
    return false;
  return buttons_[button].up;
}

bool Mouse::IsDouble(int32_t button) {
  if (button < 0 || button >= kButtonCount)
    return false;

  return buttons_[button].down && buttons_[button].clicks == 2;
}

bool Mouse::IsPress(int32_t button) {
  if (button < 0 || button >= kButtonCount)
    return false;
  return buttons_[button].pressed;
}

bool Mouse::IsMove(int32_t button) {
  if (button < 0 || button >= kButtonCount)
    return false;
  return moved_ && buttons_[button].pressed;
}

int32_t Mouse::ScrollX() {
  return static_cast<int32_t>(std::lround(scroll_delta_.x));
}

int32_t Mouse::ScrollY() {
  return static_cast<int32_t>(std::lround(scroll_delta_.y));
}

void Mouse::SetCursor(RefPtr<Bitmap> image, int32_t hot_x, int32_t hot_y) {
  if (!image) {
    SDL_SetCursor(SDL_GetDefaultCursor());
    if (cursor_) {
      SDL_DestroyCursor(cursor_);
      cursor_ = nullptr;
    }
    return;
  }

  RefPtr<Palette> surface = image->ToPalette();
  if (!surface)
    return;

  SDL_Cursor* cursor = SDL_CreateColorCursor(surface->image(), hot_x, hot_y);
  if (!cursor)
    return;

  SDL_Cursor* previous = cursor_;
  cursor_ = cursor;
  SDL_SetCursor(cursor_);
  if (previous)
    SDL_DestroyCursor(previous);
}

ATTR_DEF(Mouse, bool, Capture) {
  if (value.has_value()) {
    capture_ = *value;
    SDL_CaptureMouse(capture_);
    return std::nullopt;
  }

  return capture_;
}

ATTR_DEF(Mouse, bool, Visible) {
  if (value.has_value()) {
    if (*value)
      SDL_ShowCursor();
    else
      SDL_HideCursor();
    return std::nullopt;
  }

  return SDL_CursorVisible();
}

void Mouse::ProcessEvents(SDL_Event* event) {
  switch (event->type) {
    case SDL_EVENT_MOUSE_MOTION: {
      const SDL_MouseMotionEvent& motion = event->motion;
      position_ = Point{motion.x, motion.y};
      break;
    }

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      const SDL_MouseButtonEvent& button = event->button;
      position_ = Point{button.x, button.y};

      if (button.button < static_cast<Uint8>(kButtonCount)) {
        raw_pressed_[button.button] = button.down;
        raw_clicks_[button.button] = button.clicks;
      }
      break;
    }

    case SDL_EVENT_MOUSE_WHEEL: {
      const SDL_MouseWheelEvent& wheel = event->wheel;
      position_ = Point{wheel.mouse_x, wheel.mouse_y};

      const float direction =
          wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
      scroll_.x += wheel.x * direction;
      scroll_.y += wheel.y * direction;
      break;
    }

    default:
      break;
  }
}

Mouse::Point Mouse::WindowToScreenInternal(const Point& position) {
  float window_width = 0.0f, window_height = 0.0f;
  if (!WindowSizeInternal(Graphics::Get().window(), &window_width,
                          &window_height))
    return position;

  const float screen_width = static_cast<float>(Graphics::Get().Width());
  const float screen_height = static_cast<float>(Graphics::Get().Height());
  if (screen_width <= 0.0f || screen_height <= 0.0f)
    return position;

  return Point{position.x * screen_width / window_width,
               position.y * screen_height / window_height};
}

Mouse::Point Mouse::ScreenToWindowInternal(const Point& position) {
  float window_width = 0.0f, window_height = 0.0f;
  if (!WindowSizeInternal(Graphics::Get().window(), &window_width,
                          &window_height))
    return position;

  const float screen_width = static_cast<float>(Graphics::Get().Width());
  const float screen_height = static_cast<float>(Graphics::Get().Height());
  if (screen_width <= 0.0f || screen_height <= 0.0f)
    return position;

  return Point{position.x * window_width / screen_width,
               position.y * window_height / screen_height};
}

}  // namespace urge

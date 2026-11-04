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

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <string>
#include "SDL3/SDL.h"
#include "bind/win32api-shim/win32api_memory.h"
#include "bind/win32api-shim/win32api_shim.h"
#include "core/graphics/graphics.h"
#include "core/system/ini_encoding.h"
namespace binding::win32api {
namespace {
constexpr int64_t kWindow = 1;
Result Fail(uint32_t code) {
  SetLastError(code);
  return {};
}
SDL_Window* Window() {
  auto* graphics = urge::Graphics::TryGet();
  return graphics ? graphics->window() : nullptr;
}
bool MainThread() {
  return SDL_IsMainThread();
}
Result FindWindow(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Window();
  if (!window)
    return {};
  std::string text, error;
  if (!args[0].null()) {
    if (!memory::String(args[0], text, error))
      return Result::Failure(std::move(error));
    if (SDL_strcasecmp(text.c_str(), "RGSS Player") != 0)
      return {};
  }
  if (!args[1].null()) {
    if (!memory::String(args[1], text, error))
      return Result::Failure(std::move(error));
    if (text != SDL_GetWindowTitle(window))
      return {};
  }
  return Result{kWindow};
}
Result Foreground(std::span<const Argument>) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Window();
  return Result{window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)
                    ? kWindow
                    : 0};
}
Result MoveWindow(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Window();
  if (!window || args[0].number != kWindow)
    return Fail(1400);
  int top = 0, left = 0, bottom = 0, right = 0;
  SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
  for (size_t i = 1; i <= 4; ++i)
    if (args[i].number < INT_MIN || args[i].number > INT_MAX)
      return Fail(87);
  if (args[1].number + left > INT_MAX || args[2].number + top > INT_MAX)
    return Fail(87);
  const int64_t width = args[3].number - left - right,
                height = args[4].number - top - bottom;
  if (width <= 0 || height <= 0 || width > INT_MAX || height > INT_MAX ||
      args[1].number < INT_MIN || args[1].number > INT_MAX ||
      args[2].number < INT_MIN || args[2].number > INT_MAX)
    return Fail(87);
  // SDL sizes and desktop coordinates are macOS logical points. Preserve
  // backing pixel density; changing window size never changes the game's
  // logical render resolution.
  const bool moved =
      SDL_SetWindowPosition(window, static_cast<int>(args[1].number + left),
                            static_cast<int>(args[2].number + top));
  const bool resized = SDL_SetWindowSize(window, static_cast<int>(width),
                                         static_cast<int>(height));
  return moved && resized ? Result{1} : Fail(87);
}
Result SystemMetrics(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  SDL_Rect bounds{};
  if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &bounds))
    return Fail(87);
  int top = 0, left = 0, bottom = 0, right = 0;
  if (auto* window = Window())
    SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
  switch (args[0].number) {
    case 0:
      return Result{bounds.w};
    case 1:
      return Result{bounds.h};
    case 4:
      return Result{std::max(0, top - bottom)};
    case 7:
    case 32:
      return Result{(left + right) / 2};
    case 8:
    case 33:
      return Result{bottom};
    default:
      return Fail(87);
  }
}
SDL_Scancode Scancode(int key) {
  if (key >= 0x41 && key <= 0x5A)
    return static_cast<SDL_Scancode>(SDL_SCANCODE_A + key - 0x41);
  if (key >= 0x31 && key <= 0x39)
    return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + key - 0x31);
  if (key >= 0x70 && key <= 0x7B)
    return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + key - 0x70);
  if (key >= 0x7C && key <= 0x87)
    return static_cast<SDL_Scancode>(SDL_SCANCODE_F13 + key - 0x7C);
  if (key >= 0x61 && key <= 0x69)
    return static_cast<SDL_Scancode>(SDL_SCANCODE_KP_1 + key - 0x61);
  switch (key) {
    case 0x08:
      return SDL_SCANCODE_BACKSPACE;
    case 0x09:
      return SDL_SCANCODE_TAB;
    case 0x0D:
      return SDL_SCANCODE_RETURN;
    case 0x13:
      return SDL_SCANCODE_PAUSE;
    case 0x14:
      return SDL_SCANCODE_CAPSLOCK;
    case 0x1B:
      return SDL_SCANCODE_ESCAPE;
    case 0x20:
      return SDL_SCANCODE_SPACE;
    case 0x21:
      return SDL_SCANCODE_PAGEUP;
    case 0x22:
      return SDL_SCANCODE_PAGEDOWN;
    case 0x23:
      return SDL_SCANCODE_END;
    case 0x24:
      return SDL_SCANCODE_HOME;
    case 0x25:
      return SDL_SCANCODE_LEFT;
    case 0x26:
      return SDL_SCANCODE_UP;
    case 0x27:
      return SDL_SCANCODE_RIGHT;
    case 0x28:
      return SDL_SCANCODE_DOWN;
    case 0x2C:
      return SDL_SCANCODE_PRINTSCREEN;
    case 0x2D:
      return SDL_SCANCODE_INSERT;
    case 0x2E:
      return SDL_SCANCODE_DELETE;
    case 0x30:
      return SDL_SCANCODE_0;
    case 0x60:
      return SDL_SCANCODE_KP_0;
    case 0x6A:
      return SDL_SCANCODE_KP_MULTIPLY;
    case 0x6B:
      return SDL_SCANCODE_KP_PLUS;
    case 0x6D:
      return SDL_SCANCODE_KP_MINUS;
    case 0x6E:
      return SDL_SCANCODE_KP_PERIOD;
    case 0x6F:
      return SDL_SCANCODE_KP_DIVIDE;
    case 0x90:
      return SDL_SCANCODE_NUMLOCKCLEAR;
    case 0x91:
      return SDL_SCANCODE_SCROLLLOCK;
    case 0x5B:
      return SDL_SCANCODE_LGUI;
    case 0x5C:
      return SDL_SCANCODE_RGUI;
    case 0x5D:
      return SDL_SCANCODE_APPLICATION;
    case 0xBA:
      return SDL_SCANCODE_SEMICOLON;
    case 0xBB:
      return SDL_SCANCODE_EQUALS;
    case 0xBC:
      return SDL_SCANCODE_COMMA;
    case 0xBD:
      return SDL_SCANCODE_MINUS;
    case 0xBE:
      return SDL_SCANCODE_PERIOD;
    case 0xBF:
      return SDL_SCANCODE_SLASH;
    case 0xC0:
      return SDL_SCANCODE_GRAVE;
    case 0xDB:
      return SDL_SCANCODE_LEFTBRACKET;
    case 0xDC:
      return SDL_SCANCODE_BACKSLASH;
    case 0xDD:
      return SDL_SCANCODE_RIGHTBRACKET;
    case 0xDE:
      return SDL_SCANCODE_APOSTROPHE;
    case 0xE2:
      return SDL_SCANCODE_NONUSBACKSLASH;
    case 0xA0:
      return SDL_SCANCODE_LSHIFT;
    case 0xA1:
      return SDL_SCANCODE_RSHIFT;
    case 0xA2:
      return SDL_SCANCODE_LCTRL;
    case 0xA3:
      return SDL_SCANCODE_RCTRL;
    case 0xA4:
      return SDL_SCANCODE_LALT;
    case 0xA5:
      return SDL_SCANCODE_RALT;
    default:
      return SDL_SCANCODE_UNKNOWN;
  }
}
Result KeyState(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("keyboard APIs must run on the main thread");
  const int64_t key = args[0].number;
  if (key < 0 || key > 255)
    return {};
  auto* window = Window();
  if (!window || !(SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS))
    return {};
  const bool* state = SDL_GetKeyboardState(nullptr);
  bool down = false;
  switch (key) {
    case 0x10:
      down = state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT];
      break;
    case 0x11:
      down = state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL];
      break;
    case 0x12:
      down = state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
      break;
    case 1:
    case 2:
    case 4:
    case 5:
    case 6: {
      const int button = key == 1   ? SDL_BUTTON_LEFT
                         : key == 2 ? SDL_BUTTON_RIGHT
                         : key == 4 ? SDL_BUTTON_MIDDLE
                         : key == 5 ? SDL_BUTTON_X1
                                    : SDL_BUTTON_X2;
      down =
          (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(button)) != 0;
      break;
    }
    default: {
      const auto code = Scancode(static_cast<int>(key));
      down = code != SDL_SCANCODE_UNKNOWN && state[code];
      break;
    }
  }
  const SDL_Keymod mods = SDL_GetModState();
  const bool toggled = (key == 0x14 && (mods & SDL_KMOD_CAPS)) ||
                       (key == 0x90 && (mods & SDL_KMOD_NUM)) ||
                       (key == 0x91 && (mods & SDL_KMOD_SCROLL));
  return Result{static_cast<int16_t>((down ? 0x8000 : 0) | (toggled ? 1 : 0))};
}
template <size_t N>
Result WriteWords(const Argument& output, const std::array<int32_t, N>& words) {
  std::array<unsigned char, N * 4> bytes{};
  for (size_t i = 0; i < N; ++i) {
    const uint32_t value = static_cast<uint32_t>(words[i]);
    for (size_t j = 0; j < 4; ++j)
      bytes[i * 4 + j] = static_cast<unsigned char>(value >> (j * 8));
  }
  std::string error;
  return memory::Write(output, bytes.data(), bytes.size(), error)
             ? Result{1}
             : Result::Failure(std::move(error));
}
bool ReadPoint(const Argument& input,
               std::array<int32_t, 2>& point,
               std::string& error) {
  std::array<unsigned char, 8> bytes{};
  if (!memory::Read(input, bytes.data(), bytes.size(), error))
    return false;
  for (size_t i = 0; i < 2; ++i) {
    uint32_t value = 0;
    for (size_t j = 0; j < 4; ++j)
      value |= static_cast<uint32_t>(bytes[i * 4 + j]) << (j * 8);
    point[i] = std::bit_cast<int32_t>(value);
  }
  return true;
}
SDL_Window* Handle(const Argument& argument) {
  return !argument.buffer && argument.number == kWindow ? Window() : nullptr;
}
Result GetRect(std::span<const Argument> args, bool client) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  int x = 0, y = 0, w = 0, h = 0, top = 0, left = 0, bottom = 0, right = 0;
  SDL_GetWindowSize(window, &w, &h);
  if (!client) {
    SDL_GetWindowPosition(window, &x, &y);
    SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
  }
  const std::array<int64_t, 4> rect{static_cast<int64_t>(x) - left,
                                    static_cast<int64_t>(y) - top,
                                    static_cast<int64_t>(x) + w + right,
                                    static_cast<int64_t>(y) + h + bottom};
  for (auto value : rect)
    if (value < INT_MIN || value > INT_MAX)
      return Fail(87);
  return WriteWords(args[1],
                    std::array<int32_t, 4>{static_cast<int32_t>(rect[0]),
                                           static_cast<int32_t>(rect[1]),
                                           static_cast<int32_t>(rect[2]),
                                           static_cast<int32_t>(rect[3])});
}
Result ConvertPoint(std::span<const Argument> args, bool to_client) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  std::array<int32_t, 2> point;
  std::string error;
  if (!ReadPoint(args[1], point, error))
    return Result::Failure(std::move(error));
  int x = 0, y = 0;
  SDL_GetWindowPosition(window, &x, &y);
  const int sign = to_client ? -1 : 1;
  const int64_t target_x =
      static_cast<int64_t>(point[0]) + sign * static_cast<int64_t>(x);
  const int64_t target_y =
      static_cast<int64_t>(point[1]) + sign * static_cast<int64_t>(y);
  if (target_x < INT_MIN || target_x > INT_MAX || target_y < INT_MIN ||
      target_y > INT_MAX)
    return Fail(87);
  return WriteWords(args[1],
                    std::array<int32_t, 2>{static_cast<int32_t>(target_x),
                                           static_cast<int32_t>(target_y)});
}
Result CursorPosition(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("mouse APIs must run on the main thread");
  float x = 0, y = 0;
  SDL_GetGlobalMouseState(&x, &y);
  return WriteWords(args[0], std::array<int32_t, 2>{static_cast<int32_t>(x),
                                                    static_cast<int32_t>(y)});
}
Result SetCursorPosition(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("mouse APIs must run on the main thread");
  if (args[0].buffer || args[1].buffer || args[0].number < INT_MIN ||
      args[0].number > INT_MAX || args[1].number < INT_MIN ||
      args[1].number > INT_MAX)
    return Fail(87);
  return SDL_WarpMouseGlobal(static_cast<float>(args[0].number),
                             static_cast<float>(args[1].number))
             ? Result{1}
             : Fail(5);
}
Result KeyboardState(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("keyboard APIs must run on the main thread");
  std::array<unsigned char, 256> bytes{};
  for (size_t i = 0; i < bytes.size(); ++i) {
    const Argument key{static_cast<int64_t>(i)};
    const auto state = KeyState(std::span<const Argument>(&key, 1)).number;
    bytes[i] =
        static_cast<unsigned char>((state & 0x8000 ? 0x80 : 0) | (state & 1));
  }
  std::string error;
  return memory::Write(args[0], bytes.data(), bytes.size(), error)
             ? Result{1}
             : Result::Failure(std::move(error));
}
Result AsyncKeyState(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("keyboard APIs must run on the main thread");
  static thread_local std::array<bool, 256> previous{};
  if (args[0].number < 0 || args[0].number > 255)
    return {};
  const auto index = static_cast<size_t>(args[0].number);
  const bool down = (KeyState(args).number & 0x8000) != 0;
  const bool pressed = down && !previous[index];
  previous[index] = down;
  return Result{static_cast<int16_t>((down ? 0x8000 : 0) | (pressed ? 1 : 0))};
}
Result WindowText(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  if (args[2].number <= 0 || args[2].number > INT_MAX)
    return Fail(87);
  const std::string_view title(SDL_GetWindowTitle(window));
  const size_t length =
      std::min(title.size(), static_cast<size_t>(args[2].number - 1));
  std::string text(title.substr(0, length));
  text.push_back('\0');
  std::string error;
  if (!memory::Write(args[1], text.data(), text.size(), error))
    return Result::Failure(std::move(error));
  return Result{static_cast<int64_t>(length)};
}
Result SetWindowText(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  std::string text, error;
  if (!memory::String(args[1], text, error))
    return Result::Failure(std::move(error));
  if (!urge::ini_encoding::IsUTF8(text))
    return Fail(1113);
  return SDL_SetWindowTitle(window, text.c_str()) ? Result{1} : Fail(87);
}
Result WindowTextLength(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  return Result{static_cast<int64_t>(SDL_strlen(SDL_GetWindowTitle(window)))};
}
Result IsWindow(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  return Result{Handle(args[0]) ? 1 : 0};
}
Result Visible(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  return Result{
      window && !(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN) ? 1 : 0};
}
Result RaiseWindow(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  return SDL_RaiseWindow(window) ? Result{1} : Fail(5);
}
Result ShowWindow(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  const bool was_visible = !(SDL_GetWindowFlags(window) & SDL_WINDOW_HIDDEN);
  bool ok = false;
  switch (args[1].number) {
    case 0:
      ok = SDL_HideWindow(window);
      break;
    case 2:
    case 6:
    case 7:
      ok = SDL_MinimizeWindow(window);
      break;
    case 3:
      ok = SDL_MaximizeWindow(window);
      break;
    case 1:
    case 4:
    case 5:
    case 8:
    case 9:
    case 10:
      ok = SDL_RestoreWindow(window) && SDL_ShowWindow(window);
      break;
    default:
      return Fail(87);
  }
  if (!ok)
    SetLastError(5);
  return Result{was_visible ? 1 : 0};
}
Result SetWindowPosition(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("window APIs must run on the main thread");
  auto* window = Handle(args[0]);
  if (!window)
    return Fail(1400);
  const uint32_t flags = static_cast<uint32_t>(args[6].number);
  if (flags & ~uint32_t{0x00FF})
    return Fail(87);
  int x = 0, y = 0, w = 0, h = 0, top = 0, left = 0, bottom = 0, right = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &w, &h);
  SDL_GetWindowBordersSize(window, &top, &left, &bottom, &right);
  std::array<Argument, 6> move_args{
      args[0],
      Argument{static_cast<int64_t>(x) - left},
      Argument{static_cast<int64_t>(y) - top},
      Argument{static_cast<int64_t>(w) + left + right},
      Argument{static_cast<int64_t>(h) + top + bottom},
      Argument{1}};
  if (!(flags & 2)) {
    move_args[1] = args[2];
    move_args[2] = args[3];
  }
  if (!(flags & 1)) {
    move_args[3] = args[4];
    move_args[4] = args[5];
  }
  if (!(flags & 4) && args[1].number != 0 && args[1].number != -1 &&
      args[1].number != -2)
    return Fail(87);
  const auto moved = MoveWindow(move_args);
  if (!moved.error.empty() || !moved.number)
    return moved;
  if (flags & 0x40)
    SDL_ShowWindow(window);
  if (flags & 0x80)
    SDL_HideWindow(window);
  if (!(flags & 4)) {
    if (args[1].number == -1)
      SDL_SetWindowAlwaysOnTop(window, true);
    if (args[1].number == -2)
      SDL_SetWindowAlwaysOnTop(window, false);
  }
  if (!(flags & 0x10))
    SDL_RaiseWindow(window);
  return Result{1};
}
Result ShowCursor(std::span<const Argument> args) {
  if (!MainThread())
    return Result::Failure("mouse APIs must run on the main thread");
  static thread_local int32_t count = 0;
  if (args[0].number) {
    if (count < INT_MAX)
      ++count;
  } else {
    if (count > INT_MIN)
      --count;
  }
  if (count >= 0)
    SDL_ShowCursor();
  else
    SDL_HideCursor();
  return Result{count};
}

}  // namespace
void RegisterSDL(Registry& registry) {
  registry.Add("user32", "FindWindow", 2, 0, FindWindow);
  registry.Add("user32", "FindWindowA", 2, 0, FindWindow);
  registry.Add("user32", "GetForegroundWindow", 0, 0, Foreground);
  registry.Add("user32", "MoveWindow", 6, 0, MoveWindow);
  registry.Add("user32", "GetSystemMetrics", 1, 0, SystemMetrics);
  registry.Add("user32", "GetKeyState", 1, 0, KeyState);
  registry.Add("user32", "GetActiveWindow", 0, 0, Foreground);
  registry.Add("user32", "GetAsyncKeyState", 1, 0, AsyncKeyState);
  registry.Add("user32", "GetKeyboardState", 1, 1, KeyboardState);
  registry.Add("user32", "GetCursorPos", 1, 1, CursorPosition);
  registry.Add("user32", "SetCursorPos", 2, 0, SetCursorPosition);
  registry.Add(
      "user32", "ScreenToClient", 2, 2,
      [](std::span<const Argument> args) { return ConvertPoint(args, true); });
  registry.Add(
      "user32", "ClientToScreen", 2, 2,
      [](std::span<const Argument> args) { return ConvertPoint(args, false); });
  registry.Add(
      "user32", "GetWindowRect", 2, 2,
      [](std::span<const Argument> args) { return GetRect(args, false); });
  registry.Add(
      "user32", "GetClientRect", 2, 2,
      [](std::span<const Argument> args) { return GetRect(args, true); });
  registry.Add("user32", "GetWindowText", 3, 2, WindowText);
  registry.Add("user32", "GetWindowTextA", 3, 2, WindowText);
  registry.Add("user32", "GetWindowTextLength", 1, 0, WindowTextLength);
  registry.Add("user32", "GetWindowTextLengthA", 1, 0, WindowTextLength);
  registry.Add("user32", "SetWindowText", 2, 0, SetWindowText);
  registry.Add("user32", "SetWindowTextA", 2, 0, SetWindowText);
  registry.Add("user32", "IsWindow", 1, 0, IsWindow);
  registry.Add("user32", "IsWindowVisible", 1, 0, Visible);
  registry.Add("user32", "SetForegroundWindow", 1, 0, RaiseWindow);
  registry.Add("user32", "ShowWindow", 2, 0, ShowWindow);
  registry.Add("user32", "SetWindowPos", 7, 0, SetWindowPosition);
  registry.Add("user32", "ShowCursor", 1, 0, ShowCursor);
}
}  // namespace binding::win32api

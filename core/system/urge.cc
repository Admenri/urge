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

#include "core/system/urge.h"

#include <string>
#include <vector>

#include "SDL3/SDL_clipboard.h"
#include "SDL3/SDL_messagebox.h"
#include "SDL3/SDL_misc.h"
#include "SDL3/SDL_platform.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_surface.h"
#include "SDL3/SDL_video.h"

#include "core/canvas/image.h"
#include "core/common/utility.h"
#include "core/graphics/graphics.h"

namespace urge {

namespace {

SDL_Window* CurrentWindow() {
  auto* graphics = Graphics::TryGet();
  return graphics ? graphics->window() : nullptr;
}

}

URGE::URGE() = default;

URGE::~URGE() = default;

std::string URGE::GetPlatform() {
  const char* platform = SDL_GetPlatform();
  return platform ? std::string(platform) : std::string();
}

void URGE::SetWindowTitle(std::string title) {
  SDL_Window* window = CurrentWindow();
  if (!window)
    return;
  SDL_SetWindowTitle(window, title.c_str());
}

void URGE::SetWindowIcon(RefPtr<Image> icon) {
  SDL_Window* window = CurrentWindow();
  if (!window || !icon)
    return;
  SDL_SetWindowIcon(window, icon->image());
}

void URGE::MoveWindow(int32_t width, int32_t height) {
  SDL_Window* window = CurrentWindow();
  if (!window)
    return;
  SDL_SetWindowSize(window, width, height);
}

void URGE::MoveWindow(int32_t x, int32_t y, int32_t width, int32_t height) {
  SDL_Window* window = CurrentWindow();
  if (!window)
    return;
  SDL_SetWindowPosition(window, x, y);
  SDL_SetWindowSize(window, width, height);
}

RefPtr<Rect> URGE::GetWindowRect() {
  SDL_Window* window = CurrentWindow();
  if (!window)
    return MakeRefCounted<Rect>();

  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  SDL_GetWindowPosition(window, &x, &y);
  SDL_GetWindowSize(window, &width, &height);
  return MakeRefCounted<Rect>(x, y, width, height);
}

void URGE::OpenURL(std::string url) {
  SDL_OpenURL(url.c_str());
}

int32_t URGE::ShowMessageBox(std::string message) {
  return ShowMessageBox("", message, {}, 0, 0, MESSAGEBOX_INFORMATION);
}

int32_t URGE::ShowMessageBox(std::string title,
                             std::string message,
                             std::vector<std::string> buttons,
                             int32_t default_button,
                             int32_t escape_button,
                             int32_t flags) {
  SDL_Window* window = CurrentWindow();

  std::string title_text = title;
  if (title_text.empty() && window) {
    const char* window_title = SDL_GetWindowTitle(window);
    if (window_title)
      title_text = window_title;
  }
  if (title_text.empty())
    title_text = "URGE";

  if (buttons.empty())
    buttons.push_back("OK");

  std::vector<SDL_MessageBoxButtonData> button_data(buttons.size());
  for (std::size_t i = 0; i < buttons.size(); ++i) {
    uint32_t button_flags = 0;
    if (static_cast<int32_t>(i) == default_button)
      button_flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    if (static_cast<int32_t>(i) == escape_button)
      button_flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;

    button_data[i].flags =
        static_cast<SDL_MessageBoxButtonFlags>(button_flags);
    button_data[i].buttonID = static_cast<int32_t>(i);
    button_data[i].text = buttons[i].c_str();
  }

  SDL_MessageBoxData data = {};
  data.flags = static_cast<SDL_MessageBoxFlags>(flags);
  data.window = window;
  data.title = title_text.c_str();
  data.message = message.c_str();
  data.numbuttons = static_cast<int>(button_data.size());
  data.buttons = button_data.data();

  int clicked = -1;
  if (!SDL_ShowMessageBox(&data, &clicked))
    return -1;
  return static_cast<int32_t>(clicked);
}

ATTR_DEF(URGE, std::string, Clipboard) {
  if (value) {
    SDL_SetClipboardText(value->c_str());
    return std::nullopt;
  }

  char* text = SDL_GetClipboardText();
  std::string result = text ? std::string(text) : std::string();
  if (text)
    SDL_free(text);
  return result;
}

}

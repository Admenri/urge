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

#include "SDL3/SDL_clipboard.h"
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

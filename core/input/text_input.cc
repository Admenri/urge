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

#include "core/input/text_input.h"

#include "SDL3/SDL_keyboard.h"

#include "core/graphics/graphics.h"

namespace urge {

namespace {

SDL_Window* CurrentWindow() {
  auto* graphics = Graphics::TryGet();
  return graphics ? graphics->window() : nullptr;
}

void ResetComposition(std::string* composition,
                      int32_t* start,
                      int32_t* length) {
  composition->clear();
  *start = 0;
  *length = 0;
}

} // namespace

TextInput::TextInput() = default;

TextInput::~TextInput() = default;

bool TextInput::Enable(URGE_EXCEPT) {
  return Enable(exception_state, RefPtr<Rect>());
}

bool TextInput::Enable(URGE_EXCEPT, RefPtr<Rect> region) {
  SDL_Window* window = CurrentWindow();
  if (!window)
    return false;

  if (region) {
    SDL_Rect area = region->data.ToSDLRect();
    SDL_SetTextInputArea(window, &area, 0);
  } else {
    SDL_SetTextInputArea(window, nullptr, 0);
  }

  ResetComposition(&composition_, &editing_start_, &editing_length_);
  return SDL_StartTextInput(window);
}

bool TextInput::Disable(URGE_EXCEPT) {
  SDL_Window* window = CurrentWindow();
  if (!window)
    return false;

  const bool result = SDL_StopTextInput(window);
  SDL_SetTextInputArea(window, nullptr, 0);
  ResetComposition(&composition_, &editing_start_, &editing_length_);
  return result;
}

bool TextInput::IsActive() {
  SDL_Window* window = CurrentWindow();
  return window && SDL_TextInputActive(window);
}

std::string TextInput::Text() {
  return text_;
}

std::string TextInput::Composition() {
  return composition_;
}

int32_t TextInput::EditingStart() {
  return editing_start_;
}

int32_t TextInput::EditingLength() {
  return editing_length_;
}

void TextInput::Clear() {
  text_.clear();
  ResetComposition(&composition_, &editing_start_, &editing_length_);
}

void TextInput::ProcessEvent(SDL_Event* event) {
  if (!event)
    return;

  switch (event->type) {
    case SDL_EVENT_TEXT_INPUT:
      if (event->text.text)
        text_ += event->text.text;
      ResetComposition(&composition_, &editing_start_, &editing_length_);
      break;
    case SDL_EVENT_TEXT_EDITING:
      composition_ = event->edit.text ? event->edit.text : "";
      editing_start_ = event->edit.start;
      editing_length_ = event->edit.length;
      break;
    case SDL_EVENT_WINDOW_FOCUS_LOST:
      ResetComposition(&composition_, &editing_start_, &editing_length_);
      break;
    default:
      break;
  }
}

}

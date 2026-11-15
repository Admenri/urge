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
#include <string>

#include "SDL3/SDL_events.h"

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/refptr.h"
#include "core/common/utility.h"

namespace urge {

// A stateful bridge to SDL's text-input (IME) support.  `text` accumulates
// committed text until Clear(); `composition` is the current preedit string,
// with the same byte offsets SDL reports.  Enable() turns the platform IME on;
// the window is taken from Graphics at call time, like Mouse and Touch.
URGE_BINDING()
class TextInput : public Singleton<TextInput> {
 public:
  TextInput();
  ~TextInput();

  URGE_BINDING()
  bool Enable(URGE_EXCEPT);
  URGE_BINDING()
  bool Enable(URGE_EXCEPT, RefPtr<Rect> region);
  URGE_BINDING()
  bool Disable(URGE_EXCEPT);
  URGE_BINDING(Name : "active?")
  bool IsActive();

  URGE_BINDING()
  std::string Text();
  URGE_BINDING()
  std::string Composition();
  URGE_BINDING()
  int32_t EditingStart();
  URGE_BINDING()
  int32_t EditingLength();

  URGE_BINDING()
  void Clear();

  void ProcessEvent(SDL_Event* event);

 private:
  std::string text_;
  std::string composition_;
  int32_t editing_start_ = 0;
  int32_t editing_length_ = 0;
};

}

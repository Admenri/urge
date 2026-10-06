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
#include <vector>

#include "SDL3/SDL_surface.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "core/common.h"
#include "core/definition.h"
#include "core/refptr.h"
#include "core/utility.h"

namespace urge {

URGE_BINDING()
class Font : public Object {
 public:
  URGE_BINDING()
  Font(std::vector<std::string> names = {}, int32_t size = 24);
  URGE_BINDING()
  Font(RefPtr<Font> other);
  URGE_BINDING()
  ~Font() override = default;

  URGE_BINDING(Name : "exist?")
  static bool Existed(std::string name);

  URGE_BINDING()
  ATTR(std::vector<std::string>, Name);
  URGE_BINDING()
  ATTR(int32_t, Size);
  URGE_BINDING()
  ATTR(bool, Bold);
  URGE_BINDING()
  ATTR(bool, Italic);
  URGE_BINDING()
  ATTR(bool, Outline);
  URGE_BINDING()
  ATTR(bool, Shadow);
  URGE_BINDING()
  ATTR(bool, Solid);
  URGE_BINDING()
  ATTR(RefPtr<Color>, Color);
  URGE_BINDING()
  ATTR(RefPtr<Color>, OutColor);
  URGE_BINDING()
  ATTR(RefPtr<Color>, GradientColor);

  URGE_BINDING()
  static ATTR(std::vector<std::string>, DefaultName);
  URGE_BINDING()
  static ATTR(int32_t, DefaultSize);
  URGE_BINDING()
  static ATTR(bool, DefaultBold);
  URGE_BINDING()
  static ATTR(bool, DefaultItalic);
  URGE_BINDING()
  static ATTR(bool, DefaultOutline);
  URGE_BINDING()
  static ATTR(bool, DefaultShadow);
  URGE_BINDING()
  static ATTR(bool, DefaultSolid);
  URGE_BINDING()
  static ATTR(RefPtr<Color>, DefaultColor);
  URGE_BINDING()
  static ATTR(RefPtr<Color>, DefaultOutColor);
  URGE_BINDING()
  static ATTR(RefPtr<Color>, DefaultGradientColor);

  TTF_Font* ttf_font();

  SDL_Surface* RenderText(const std::string& text, uint8_t* font_opacity);

  bool MeasureText(const std::string& text, int32_t* width, int32_t* height);

  std::vector<std::string> name_;
  int32_t size_ = 24;
  bool bold_ = false, italic_ = false, outline_ = true, shadow_ = false,
       solid_ = false;
  RefPtr<Color> color_, out_color_, gradient_color_;
};

}  // namespace urge

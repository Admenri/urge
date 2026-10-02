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
#include "core/refptr.h"
#include "core/utility.h"

namespace urge {

class Font : public Object {
 public:
  /*-export.begin-*/
  Font(std::vector<std::string> names = {}, int32_t size = 24);
  Font(RefPtr<Font> other);
  ~Font() override = default;

  URGE_BINDING(Name : "exist?")
  static bool Existed(std::string name);

  ATTR(std::vector<std::string>, Name);
  ATTR(int32_t, Size);
  ATTR(bool, Bold);
  ATTR(bool, Italic);
  ATTR(bool, Outline);
  ATTR(bool, Shadow);
  ATTR(bool, Solid);
  ATTR(RefPtr<Color>, Color);
  ATTR(RefPtr<Color>, OutColor);
  ATTR(RefPtr<Color>, GradientColor);

  static ATTR(std::vector<std::string>, DefaultName);
  static ATTR(int32_t, DefaultSize);
  static ATTR(bool, DefaultBold);
  static ATTR(bool, DefaultItalic);
  static ATTR(bool, DefaultOutline);
  static ATTR(bool, DefaultShadow);
  static ATTR(bool, DefaultSolid);
  static ATTR(RefPtr<Color>, DefaultColor);
  static ATTR(RefPtr<Color>, DefaultOutColor);
  static ATTR(RefPtr<Color>, DefaultGradientColor);
  /*-export.end-*/

  //! Underlying TTF handle of the current name and size, never null in a
  //! bitmap, which keeps the caller from having to special case a failure.
  TTF_Font* ttf_font();

  /*! Renders \p text into a freshly allocated surface.
   *
   *  \param font_opacity receives the alpha of the text color, which a caller
   *         applies as a separate opacity when it composites the result, so
   *         that the color stays intact.
   *  \returns an owned ABGR8888 surface of straight (not premultiplied) alpha,
   *           or nullptr when the render failed.
   */
  SDL_Surface* RenderText(const std::string& text, uint8_t* font_opacity);

  //! Pixel extent of \p text without rendering it.
  bool MeasureText(const std::string& text, int32_t* width, int32_t* height);

  //! The face this Font resolved to, mainly for diagnostics.
  std::vector<std::string> name_;
  int32_t size_ = 24;
  bool bold_ = false, italic_ = false, outline_ = true, shadow_ = false,
       solid_ = false;
  RefPtr<Color> color_, out_color_, gradient_color_;
};

}  // namespace urge

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

#include "core/common.h"
#include "core/refptr.h"
#include "core/utility.h"

namespace urge {

class Font : public Object {
 public:
  /*-export.begin-*/
  Font(std::vector<std::string> names = {}, int32_t size = 24);
  Font(RefPtr<Font> other);

  static bool Exist(std::string name);

  ATTR(std::vector<std::string>, Name);
  ATTR(int32_t, Size);
  ATTR(bool, Bold);
  ATTR(bool, Italic);
  ATTR(bool, Outline);
  ATTR(bool, Shadow);
  ATTR(RefPtr<Color>, Color);
  ATTR(RefPtr<Color>, OutColor);

  static ATTR(std::vector<std::string>, DefaultName);
  static ATTR(int32_t, DefaultSize);
  static ATTR(bool, DefaultBold);
  static ATTR(bool, DefaultItalic);
  static ATTR(bool, DefaultOutline);
  static ATTR(bool, DefaultShadow);
  static ATTR(RefPtr<Color>, DefaultColor);
  static ATTR(RefPtr<Color>, DefaultOutColor);
  /*-export.end-*/

 private:
  std::vector<std::string> name_;
  int32_t size_ = 24;
  bool bold_ = false, italic_ = false, outline_ = true, shadow_ = false;
  RefPtr<Color> color_, out_color_;
};

}  // namespace urge

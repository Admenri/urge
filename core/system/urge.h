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

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/refptr.h"

namespace urge {

class Image;
class Rect;

URGE_BINDING()
class URGE : public Singleton<URGE> {
 public:
  URGE();
  ~URGE();

  URGE_BINDING(Name : "platform")
  std::string GetPlatform();

  URGE_BINDING()
  void SetWindowTitle(std::string title);

  URGE_BINDING()
  void SetWindowIcon(RefPtr<Image> icon);

  URGE_BINDING()
  void MoveWindow(int32_t width, int32_t height);
  URGE_BINDING()
  void MoveWindow(int32_t x, int32_t y, int32_t width, int32_t height);

  URGE_BINDING()
  RefPtr<Rect> GetWindowRect();

  URGE_BINDING()
  void OpenURL(std::string url);

  URGE_BINDING()
  ATTR(std::string, Clipboard);
};

}

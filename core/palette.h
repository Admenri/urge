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

#include "SDL3/SDL_surface.h"

#include "core/disposable.h"
#include "core/refptr.h"
#include "core/utility.h"

namespace urge {

class Palette : public Disposable {
 public:
  Palette(SDL_Surface* image);

  /*-export.begin-*/
  Palette(int32_t width, int32_t height);
  Palette(std::string filename);
  ~Palette() override;

  RefPtr<Color> GetPixel(int32_t x, int32_t y);
  void SetPixel(int32_t x, int32_t y, RefPtr<Color> color);
  void SaveFile(std::string filename);
  /*-export.end-*/

 public:
  SDL_Surface* image() { return image_; }

 private:
  void DisposeObject() override;

  SDL_Surface* image_ = nullptr;
};

}  // namespace urge

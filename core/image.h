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

#include "core/definition.h"
#include "core/disposable.h"
#include "core/refptr.h"
#include "core/utility.h"

namespace urge {

URGE_BINDING()
class Image : public Disposable {
 public:
  Image(SDL_Surface* image);

  URGE_BINDING()
  Image(int32_t width, int32_t height);
  URGE_BINDING()
  Image(std::string filename);
  URGE_BINDING()
  Image(RefPtr<Image> other);
  URGE_BINDING()
  ~Image() override;

  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();

  URGE_BINDING()
  RefPtr<Color> GetPixel(int32_t x, int32_t y);
  URGE_BINDING()
  void SetPixel(int32_t x, int32_t y, RefPtr<Color> color);

  URGE_BINDING()
  void FillRect(int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                RefPtr<Color> color);
  URGE_BINDING()
  void FillRect(RefPtr<Rect> rect, RefPtr<Color> color);

  URGE_BINDING()
  void Blt(int32_t x,
           int32_t y,
           RefPtr<Image> src_image,
           RefPtr<Rect> src_rect,
           int32_t opacity = 255);
  URGE_BINDING()
  void StretchBlt(RefPtr<Rect> dst_rect,
                  RefPtr<Image> src_image,
                  RefPtr<Rect> src_rect,
                  int32_t opacity = 255);

  URGE_BINDING()
  void GradientFillRect(int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        RefPtr<Color> color1,
                        RefPtr<Color> color2,
                        bool vertical = false);
  URGE_BINDING()
  void GradientFillRect(RefPtr<Rect> rect,
                        RefPtr<Color> color1,
                        RefPtr<Color> color2,
                        bool vertical = false);

  URGE_BINDING()
  void Clear();
  URGE_BINDING()
  void ClearRect(int32_t x, int32_t y, int32_t width, int32_t height);
  URGE_BINDING()
  void ClearRect(RefPtr<Rect> rect);

  URGE_BINDING()
  static RefPtr<Image> FromDump(std::string data);
  URGE_BINDING()
  std::string ToDump();

  URGE_BINDING()
  void SaveFile(std::string filename);

 public:
  SDL_Surface* image() { return image_; }

 private:
  void DisposeObject() override;

  SDL_Surface* image_ = nullptr;
};

}  // namespace urge

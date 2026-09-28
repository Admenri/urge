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

#include "core/disposable.h"
#include "core/font.h"
#include "core/gpu.h"
#include "core/palette.h"
#include "core/utility.h"

namespace urge {

class Bitmap : public Disposable {
 public:
  /*-export.begin-*/
  Bitmap(std::string filename);
  Bitmap(int32_t width, int32_t height);
  Bitmap(RefPtr<Bitmap> other);
  ~Bitmap() override;

  int32_t GetWidth();
  int32_t GetHeight();
  RefPtr<Rect> GetRect();
  void Blt(int32_t x,
           int32_t y,
           RefPtr<Bitmap> src_bitmap,
           RefPtr<Rect> src_rect,
           int32_t opacity = 255);
  void StretchBlt(RefPtr<Rect> dst_rect,
                  RefPtr<Bitmap> src_bitmap,
                  RefPtr<Rect> src_rect,
                  int32_t opacity = 255);
  void FillRect(int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                RefPtr<Color> color);
  void FillRect(RefPtr<Rect> rect, RefPtr<Color> color);
  void GradientFillRect(int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        RefPtr<Color> color1,
                        RefPtr<Color> color2,
                        bool vertical = false);
  void GradientFillRect(RefPtr<Rect> rect,
                        RefPtr<Color> color1,
                        RefPtr<Color> color2,
                        bool vertical = false);
  void Clear();
  void ClearRect(int32_t x, int32_t y, int32_t width, int32_t height);
  void ClearRect(RefPtr<Rect> rect);
  RefPtr<Color> GetPixel(int32_t x, int32_t y);
  void SetPixel(int32_t x, int32_t y, RefPtr<Color> color);
  void HueChange(int32_t hue);
  void Blur();
  void RadialBlur(int32_t angle, int32_t division);
  void DrawText(int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                std::string str,
                int32_t align = 0);
  void DrawText(RefPtr<Rect> rect, std::string str, int32_t align = 0);
  RefPtr<Rect> TextSize(std::string str);

  RefPtr<Palette> ToPalette();
  void UpdateWithPalette(RefPtr<Palette> palette);

  ATTR(RefPtr<Font>, Font);
  /*-export.end-*/

  wgpu::Texture texture() { return texture_; }
  wgpu::Texture depth_stencil() { return depth_stencil_; }

 private:
  void DisposeObject() override;
  void CreateInternal(SDL_Surface* data);

  RefPtr<Font> font_;

  Vec2i size_;
  wgpu::Texture texture_;
  wgpu::Texture depth_stencil_;

  wgpu::BindGroup scene_group_, object_group_;
  wgpu::Buffer scene_uniform_, object_uniform_;
};

}  // namespace urge

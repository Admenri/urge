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

#include "core/definition.h"
#include "core/device.h"
#include "core/disposable.h"
#include "core/font.h"
#include "core/font_renderer.h"
#include "core/palette.h"
#include "core/primitive.h"
#include "core/utility.h"

namespace urge {

URGE_BINDING()
class Bitmap : public Disposable {
 public:
  URGE_BINDING()
  Bitmap(std::string filename);
  URGE_BINDING()
  Bitmap(int32_t width, int32_t height);
  URGE_BINDING()
  Bitmap(RefPtr<Bitmap> other);
  URGE_BINDING()
  ~Bitmap() override;

  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();
  URGE_BINDING(Name : "rect")
  RefPtr<Rect> GetRect();
  URGE_BINDING()
  void Blt(int32_t x,
           int32_t y,
           RefPtr<Bitmap> src_bitmap,
           RefPtr<Rect> src_rect,
           int32_t opacity = 255);
  URGE_BINDING()
  void StretchBlt(RefPtr<Rect> dst_rect,
                  RefPtr<Bitmap> src_bitmap,
                  RefPtr<Rect> src_rect,
                  int32_t opacity = 255);
  URGE_BINDING()
  void MaskBlt(RefPtr<Rect> dst_rect,
               RefPtr<Bitmap> src_bitmap,
               RefPtr<Rect> src_rect,
               RefPtr<Bitmap> mask);
  URGE_BINDING()
  void FillRect(int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                RefPtr<Color> color);
  URGE_BINDING()
  void FillRect(RefPtr<Rect> rect, RefPtr<Color> color);
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
  RefPtr<Color> GetPixel(int32_t x, int32_t y);
  URGE_BINDING()
  void SetPixel(int32_t x, int32_t y, RefPtr<Color> color);
  URGE_BINDING()
  void HueChange(int32_t hue);
  URGE_BINDING()
  void Blur();
  URGE_BINDING()
  void RadialBlur(int32_t angle, int32_t division);
  URGE_BINDING()
  void DrawText(int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                std::string str,
                int32_t align = 0);
  URGE_BINDING()
  void DrawText(RefPtr<Rect> rect, std::string str, int32_t align = 0);
  URGE_BINDING()
  RefPtr<Rect> TextSize(std::string str);

  URGE_BINDING()
  RefPtr<Palette> ToPalette();
  URGE_BINDING()
  void UpdateWithPalette(RefPtr<Palette> palette);

  URGE_BINDING()
  ATTR(RefPtr<Font>, Font);

 public:
  void UpdateWithPixels(const void* pixels, uint32_t bytes_per_row);

  wgpu::RenderPassEncoder BeginRendering(
      wgpu::CommandEncoder encoder,
      std::optional<glm::vec4> clear = std::nullopt);

  glm::ivec2 size() const { return size_; }

  wgpu::Texture texture() { return texture_; }
  wgpu::TextureView texture_view() { return texture_view_; }
  wgpu::Texture depth_stencil() { return depth_stencil_; }
  wgpu::TextureView depth_stencil_view() { return depth_stencil_view_; }
  wgpu::Sampler sampler() { return sampler_; }

  wgpu::BindGroup scene_group() { return scene_group_; }
  wgpu::BindGroup object_group() { return object_group_; }
  wgpu::BindGroup texture_group() { return texture_group_; }

 private:
  void DisposeObject() override;
  void CreateInternal(SDL_Surface* data);
  void CreateGroup();

  RefPtr<Font> font_;
  FontRenderer text_renderer_;

  glm::ivec2 size_;
  wgpu::Texture texture_, depth_stencil_;
  wgpu::TextureView texture_view_, depth_stencil_view_;

  wgpu::Buffer scene_uniform_, object_uniform_;
  wgpu::Sampler sampler_;

  PrimitiveEmitter primitive_;

  wgpu::BindGroup scene_group_, object_group_, texture_group_;
};

}  // namespace urge

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

#include "core/canvas/image.h"
#include "core/common/definition.h"
#include "core/common/disposable.h"
#include "core/common/utility.h"
#include "core/font/font.h"
#include "core/font/font_renderer.h"
#include "core/render/device.h"
#include "core/render/primitive.h"

namespace urge {

class GPUTexture;
class GPUTextureView;

URGE_BINDING()
class Bitmap : public Disposable {
 public:
  URGE_BINDING()
  Bitmap(URGE_EXCEPT, std::string filename);
  URGE_BINDING()
  Bitmap(int32_t width, int32_t height);
  URGE_BINDING()
  Bitmap(RefPtr<Bitmap> other);

  static RefPtr<Bitmap> FromSurface(SDL_Surface* surface);
  ~Bitmap() override;

  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();
  URGE_BINDING(Name : "rect")
  RefPtr<Rect> GetRect();
  URGE_BINDING()
  void Blt(URGE_EXCEPT,
           int32_t x,
           int32_t y,
           RefPtr<Bitmap> src_bitmap,
           RefPtr<Rect> src_rect,
           int32_t opacity = 255);
  URGE_BINDING()
  void StretchBlt(URGE_EXCEPT,
                  RefPtr<Rect> dst_rect,
                  RefPtr<Bitmap> src_bitmap,
                  RefPtr<Rect> src_rect,
                  int32_t opacity = 255);
  URGE_BINDING()
  void MaskBlt(URGE_EXCEPT,
               RefPtr<Rect> dst_rect,
               RefPtr<Bitmap> src_bitmap,
               RefPtr<Rect> src_rect,
               RefPtr<Bitmap> mask);
  URGE_BINDING()
  void FillRect(URGE_EXCEPT,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                RefPtr<Color> color);
  URGE_BINDING()
  void FillRect(URGE_EXCEPT, RefPtr<Rect> rect, RefPtr<Color> color);
  URGE_BINDING()
  void GradientFillRect(URGE_EXCEPT,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        RefPtr<Color> color1,
                        RefPtr<Color> color2,
                        bool vertical = false);
  URGE_BINDING()
  void GradientFillRect(URGE_EXCEPT,
                        RefPtr<Rect> rect,
                        RefPtr<Color> color1,
                        RefPtr<Color> color2,
                        bool vertical = false);
  URGE_BINDING()
  void Clear(URGE_EXCEPT);
  URGE_BINDING()
  void ClearRect(URGE_EXCEPT,
                 int32_t x,
                 int32_t y,
                 int32_t width,
                 int32_t height);
  URGE_BINDING()
  void ClearRect(URGE_EXCEPT, RefPtr<Rect> rect);
  URGE_BINDING()
  RefPtr<Color> GetPixel(URGE_EXCEPT, int32_t x, int32_t y);
  URGE_BINDING()
  void SetPixel(URGE_EXCEPT, int32_t x, int32_t y, RefPtr<Color> color);
  URGE_BINDING()
  void HueChange(URGE_EXCEPT, int32_t hue);
  URGE_BINDING()
  void Blur(URGE_EXCEPT);
  URGE_BINDING()
  void RadialBlur(URGE_EXCEPT, int32_t angle, int32_t division);
  URGE_BINDING()
  void DrawText(URGE_EXCEPT,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                std::string str,
                int32_t align = 0);
  URGE_BINDING()
  void DrawText(URGE_EXCEPT,
                RefPtr<Rect> rect,
                std::string str,
                int32_t align = 0);
  URGE_BINDING()
  RefPtr<Rect> TextSize(URGE_EXCEPT, std::string str);

  URGE_BINDING(Name : "texture_view")
  RefPtr<GPUTextureView> GetTextureView();
  URGE_BINDING(Name : "texture")
  RefPtr<GPUTexture> GetTexture();
  URGE_BINDING(Name : "depth_stencil_view")
  RefPtr<GPUTextureView> GetDepthStencilView();

  URGE_BINDING()
  RefPtr<Image> ToImage(URGE_EXCEPT);
  URGE_BINDING()
  void UpdateWithImage(URGE_EXCEPT, RefPtr<Image> image);

  URGE_BINDING()
  static RefPtr<Bitmap> FromImage(URGE_EXCEPT, RefPtr<Image> image);

  URGE_BINDING()
  ATTR(RefPtr<Font>, Font);

 public:
  void UpdateWithPixels(URGE_EXCEPT,
                        const void* pixels,
                        uint32_t bytes_per_row);

  void AcquireHandle();

  wgpu::RenderPassEncoder BeginRendering(
      wgpu::CommandEncoder encoder,
      std::optional<glm::vec4> clear = std::nullopt,
      bool clear_depth = true);

  glm::ivec2 size() const { return size_; }

  wgpu::Texture texture() {
    AcquireHandle();
    return texture_;
  }
  wgpu::TextureView texture_view() {
    AcquireHandle();
    return texture_view_;
  }
  wgpu::Texture depth_stencil() {
    AcquireHandle();
    return depth_stencil_;
  }
  wgpu::TextureView depth_stencil_view() {
    AcquireHandle();
    return depth_stencil_view_;
  }
  wgpu::Sampler sampler() {
    AcquireHandle();
    return sampler_;
  }

  wgpu::BindGroup scene_group() {
    AcquireHandle();
    return scene_group_;
  }
  wgpu::BindGroup object_group() {
    AcquireHandle();
    return object_group_;
  }
  wgpu::BindGroup texture_group() {
    AcquireHandle();
    return texture_group_;
  }

 private:
  void DisposeObject() override;
  void CreateInternal(SDL_Surface* data);
  void CreateResources(const glm::ivec2& size);
  void UploadSurface(SDL_Surface* data);
  void ClearToTransparent();
  void CreateGroup();

  RefPtr<Font> font_;
  FontRenderer text_renderer_;

  glm::ivec2 size_;
  wgpu::Texture texture_, depth_stencil_;
  wgpu::TextureView texture_view_, depth_stencil_view_;

  wgpu::Buffer scene_uniform_, object_uniform_;
  wgpu::Sampler sampler_;

  wgpu::BindGroup scene_group_, object_group_, texture_group_;
};

}

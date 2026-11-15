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

#include "core/canvas/image.h"
#include "core/common/definition.h"
#include "core/common/disposable.h"
#include "core/common/utility.h"
#include "core/font/font.h"
#include "core/font/font_renderer.h"
#include "core/graphics/gpu.h"
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
  Bitmap(URGE_EXCEPT, int32_t width, int32_t height);
  URGE_BINDING()
  Bitmap(URGE_EXCEPT, Bitmap* other);

  static Bitmap* FromSurface(URGE_EXCEPT, SDL_Surface* surface);
  ~Bitmap() override;
  void Trace(GCVisitor& visitor) const override;

  URGE_BINDING()
  int32_t Width();
  URGE_BINDING()
  int32_t Height();
  URGE_BINDING(Name : "rect")
  Rect* GetRect();
  URGE_BINDING()
  void Blt(URGE_EXCEPT,
           int32_t x,
           int32_t y,
           Bitmap* src_bitmap,
           Rect* src_rect,
           int32_t opacity = 255,
           int32_t blend_type = 0);
  URGE_BINDING()
  void StretchBlt(URGE_EXCEPT,
                  Rect* dst_rect,
                  Bitmap* src_bitmap,
                  Rect* src_rect,
                  int32_t opacity = 255,
                  int32_t blend_type = 0);
  URGE_BINDING()
  void MaskBlt(URGE_EXCEPT,
               Rect* dst_rect,
               Bitmap* src_bitmap,
               Rect* src_rect,
               Bitmap* mask,
               int32_t blend_type = 0);
  URGE_BINDING()
  void FillRect(URGE_EXCEPT,
                int32_t x,
                int32_t y,
                int32_t width,
                int32_t height,
                Color* color);
  URGE_BINDING()
  void FillRect(URGE_EXCEPT, Rect* rect, Color* color);
  URGE_BINDING()
  void GradientFillRect(URGE_EXCEPT,
                        int32_t x,
                        int32_t y,
                        int32_t width,
                        int32_t height,
                        Color* color1,
                        Color* color2,
                        bool vertical = false);
  URGE_BINDING()
  void GradientFillRect(URGE_EXCEPT,
                        Rect* rect,
                        Color* color1,
                        Color* color2,
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
  void ClearRect(URGE_EXCEPT, Rect* rect);
  URGE_BINDING()
  Color* GetPixel(URGE_EXCEPT, int32_t x, int32_t y);
  URGE_BINDING()
  void SetPixel(URGE_EXCEPT, int32_t x, int32_t y, Color* color);
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
  void DrawText(URGE_EXCEPT, Rect* rect, std::string str, int32_t align = 0);
  URGE_BINDING()
  Rect* TextSize(URGE_EXCEPT, std::string str);

  URGE_BINDING(Name : "texture_view")
  GPUTextureView* GetTextureView();
  URGE_BINDING(Name : "texture")
  GPUTexture* GetTexture();
  URGE_BINDING(Name : "depth_stencil_view")
  GPUTextureView* GetDepthStencilView();

  URGE_BINDING()
  Image* ToImage(URGE_EXCEPT);
  URGE_BINDING()
  void UpdateWithImage(URGE_EXCEPT, Image* image);

  URGE_BINDING()
  static Bitmap* FromImage(URGE_EXCEPT, Image* image);

  URGE_BINDING()
  ATTR(Font*, Font);
  URGE_BINDING()
  ATTR(GPUSampler*, Sampler);

 public:
  void UpdateWithPixels(URGE_EXCEPT,
                        const void* pixels,
                        uint32_t bytes_per_row);

  void AcquireHandle();

  void TouchContent() { ++content_serial_; }

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
  wgpu::Sampler sampler();

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
  bool CreateResources(URGE_EXCEPT, const glm::ivec2& size);
  void UploadSurface(SDL_Surface* data);
  void ClearToTransparent();
  void CreateGroup();

  Member<Font> font_;
  FontRenderer text_renderer_;

  glm::ivec2 size_ = glm::ivec2(0);
  wgpu::Texture texture_, depth_stencil_;
  wgpu::TextureView texture_view_, depth_stencil_view_;

  wgpu::Buffer scene_uniform_, object_uniform_;
  wgpu::Sampler sampler_;
  Member<GPUSampler> sampler_resource_;

  wgpu::BindGroup scene_group_, object_group_, texture_group_;

  std::uint64_t content_serial_ = 1;
  std::uint64_t read_serial_ = 0;
  std::uint64_t probe_serial_ = 0;
  bool read_snapshot_ = false;
  std::vector<std::uint8_t> read_pixels_;
};

}

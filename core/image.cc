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

#include "core/image.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include "SDL3_image/SDL_image.h"

#include "core/exception.h"
#include "core/filesystem.h"

namespace urge {

namespace {

const SDL_PixelFormat kInternalPixelFormat = SDL_PIXELFORMAT_ABGR8888;

Uint32 MapColor(SDL_Surface* surface, const RefPtr<Color>& color) {
  const SDL_PixelFormatDetails* details =
      SDL_GetPixelFormatDetails(surface->format);
  return SDL_MapRGBA(details, nullptr, static_cast<Uint8>(color->data.r),
                     static_cast<Uint8>(color->data.g),
                     static_cast<Uint8>(color->data.b),
                     static_cast<Uint8>(color->data.a));
}

}  // namespace

Image::Image(SDL_Surface* image) : image_(image) {}

Image::Image(int32_t width, int32_t height) {
  image_ = SDL_CreateSurface(width, height, kInternalPixelFormat);
}

Image::Image(std::string filename) {
  IOService::Get().OpenRead(
      filename, [&](SDL_IOStream* stream, const std::string& ext) {
        image_ = IMG_LoadTyped_IO(stream, true, ext.c_str());
        return !!image_;
      });

  if (!image_)
    throw Exception(Exception::kRGSSError, "failed to load image: {}",
                    filename);

  if (image_->format != kInternalPixelFormat) {
    auto* converted_image = SDL_ConvertSurface(image_, kInternalPixelFormat);
    SDL_DestroySurface(image_);
    image_ = converted_image;

    if (!image_)
      throw Exception(Exception::kRGSSError, SDL_GetError());
  }
}

Image::Image(RefPtr<Image> other) {
  image_ = SDL_ConvertSurface(other->image_, kInternalPixelFormat);
}

Image::~Image() {
  Disposable::Dispose();
}

int32_t Image::Width() {
  return image_->w;
}

int32_t Image::Height() {
  return image_->h;
}

RefPtr<Color> Image::GetPixel(int32_t x, int32_t y) {
  Disposable::Guard();

  const auto* pixel_detail = SDL_GetPixelFormatDetails(image_->format);
  const int32_t bpp = pixel_detail->bytes_per_pixel;
  const uint8_t* pixel = static_cast<uint8_t*>(image_->pixels) +
                         static_cast<size_t>(y) * image_->pitch +
                         static_cast<size_t>(x) * bpp;

  uint8_t color[4];
  SDL_GetRGBA(*reinterpret_cast<const uint32_t*>(pixel), pixel_detail, nullptr,
              &color[0], &color[1], &color[2], &color[3]);

  return MakeRefCounted<Color>(
      static_cast<float>(color[0]), static_cast<float>(color[1]),
      static_cast<float>(color[2]), static_cast<float>(color[3]));
}

void Image::SetPixel(int32_t x, int32_t y, RefPtr<Color> color) {
  Disposable::Guard();

  if (!color)
    throw Exception(Exception::kRGSSError, "invalid color.");

  const auto* pixel_detail = SDL_GetPixelFormatDetails(image_->format);
  const int32_t bpp = pixel_detail->bytes_per_pixel;
  uint8_t* pixel =
      static_cast<uint8_t*>(image_->pixels) + y * image_->pitch + x * bpp;
  *reinterpret_cast<uint32_t*>(pixel) = SDL_MapRGBA(
      pixel_detail, nullptr, static_cast<Uint8>(color->data.r),
      static_cast<Uint8>(color->data.g), static_cast<Uint8>(color->data.b),
      static_cast<Uint8>(color->data.a));
}

void Image::FillRect(int32_t x,
                     int32_t y,
                     int32_t width,
                     int32_t height,
                     RefPtr<Color> color) {
  Disposable::Guard();

  if (!color)
    throw Exception(Exception::kRGSSError, "invalid color value.");

  SDL_Rect rect = {x, y, width, height};
  if (!SDL_FillSurfaceRect(image_, &rect, MapColor(image_, color)))
    throw Exception(Exception::kRGSSError, SDL_GetError());
}

void Image::FillRect(RefPtr<Rect> rect, RefPtr<Color> color) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  FillRect(rect->data.x, rect->data.y, rect->data.width, rect->data.height,
           color);
}

void Image::Blt(int32_t x,
                int32_t y,
                RefPtr<Image> src_image,
                RefPtr<Rect> src_rect,
                int32_t opacity) {
  Disposable::Guard();

  if (!src_rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  StretchBlt(
      MakeRefCounted<Rect>(x, y, src_rect->data.width, src_rect->data.height),
      src_image, src_rect, opacity);
}

void Image::StretchBlt(RefPtr<Rect> dst_rect,
                       RefPtr<Image> src_image,
                       RefPtr<Rect> src_rect,
                       int32_t opacity) {
  Disposable::Guard();

  if (!dst_rect || !src_image || !src_rect)
    throw Exception(Exception::kRGSSError, "invalid rect or image value.");

  SDL_Surface* source = src_image->image();
  SDL_Rect source_rect = {src_rect->data.x, src_rect->data.y,
                          src_rect->data.width, src_rect->data.height};
  SDL_Rect target_rect = {dst_rect->data.x, dst_rect->data.y,
                          dst_rect->data.width, dst_rect->data.height};

  Uint8 previous_alpha = 255;
  SDL_GetSurfaceAlphaMod(source, &previous_alpha);
  SDL_BlendMode previous_blend = SDL_BLENDMODE_NONE;
  SDL_GetSurfaceBlendMode(source, &previous_blend);

  const Uint8 alpha = static_cast<Uint8>(std::clamp(opacity, 0, 255));
  SDL_SetSurfaceAlphaMod(source, alpha);
  SDL_SetSurfaceBlendMode(source, SDL_BLENDMODE_BLEND);

  const bool scaled =
      target_rect.w != source_rect.w || target_rect.h != source_rect.h;
  const bool result =
      scaled ? SDL_BlitSurfaceScaled(source, &source_rect, image_, &target_rect,
                                     SDL_SCALEMODE_NEAREST)
             : SDL_BlitSurface(source, &source_rect, image_, &target_rect);

  SDL_SetSurfaceAlphaMod(source, previous_alpha);
  SDL_SetSurfaceBlendMode(source, previous_blend);

  if (!result)
    throw Exception(Exception::kRGSSError, SDL_GetError());
}

void Image::GradientFillRect(int32_t x,
                             int32_t y,
                             int32_t width,
                             int32_t height,
                             RefPtr<Color> color1,
                             RefPtr<Color> color2,
                             bool vertical) {
  Disposable::Guard();

  if (!color1 || !color2)
    throw Exception(Exception::kRGSSError, "invalid color value.");

  const int32_t steps = vertical ? height : width;
  if (steps <= 0)
    return;

  const SDL_PixelFormatDetails* details =
      SDL_GetPixelFormatDetails(image_->format);
  const int32_t divisor = steps > 1 ? steps - 1 : 1;

  for (int32_t i = 0; i < steps; ++i) {
    const float t = steps > 1 ? static_cast<float>(i) / divisor : 0.0f;
    const Uint32 color =
        SDL_MapRGBA(details, nullptr,
                    static_cast<Uint8>(color1->data.r +
                                       (color2->data.r - color1->data.r) * t),
                    static_cast<Uint8>(color1->data.g +
                                       (color2->data.g - color1->data.g) * t),
                    static_cast<Uint8>(color1->data.b +
                                       (color2->data.b - color1->data.b) * t),
                    static_cast<Uint8>(color1->data.a +
                                       (color2->data.a - color1->data.a) * t));

    SDL_Rect line =
        vertical ? SDL_Rect{x, y + i, width, 1} : SDL_Rect{x + i, y, 1, height};
    if (!SDL_FillSurfaceRect(image_, &line, color))
      throw Exception(Exception::kRGSSError, SDL_GetError());
  }
}

void Image::GradientFillRect(RefPtr<Rect> rect,
                             RefPtr<Color> color1,
                             RefPtr<Color> color2,
                             bool vertical) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  GradientFillRect(rect->data.x, rect->data.y, rect->data.width,
                   rect->data.height, color1, color2, vertical);
}

void Image::Clear() {
  Disposable::Guard();

  ClearRect(0, 0, image_->w, image_->h);
}

void Image::ClearRect(int32_t x, int32_t y, int32_t width, int32_t height) {
  Disposable::Guard();

  FillRect(x, y, width, height, MakeRefCounted<Color>());
}

void Image::ClearRect(RefPtr<Rect> rect) {
  Disposable::Guard();

  if (!rect)
    throw Exception(Exception::kRGSSError, "invalid rect value.");

  ClearRect(rect->data.x, rect->data.y, rect->data.width, rect->data.height);
}

RefPtr<Image> Image::FromDump(std::string data) {
  auto* stream = SDL_IOFromConstMem(data.data(), data.size());
  if (!stream)
    throw Exception(Exception::kRGSSError, SDL_GetError());

  auto* image = IMG_LoadTyped_IO(stream, true, "PNG");
  if (!image)
    throw Exception(Exception::kRGSSError, SDL_GetError());

  if (image->format != kInternalPixelFormat) {
    auto* converted_image = SDL_ConvertSurface(image, kInternalPixelFormat);
    SDL_DestroySurface(image);

    if (!converted_image)
      throw Exception(Exception::kRGSSError, SDL_GetError());

    image = converted_image;
  }

  return MakeRefCounted<Image>(image);
}

std::string Image::ToDump() {
  Disposable::Guard();

  auto* stream = SDL_IOFromDynamicMem();
  if (!stream)
    throw Exception(Exception::kRGSSError, SDL_GetError());

  if (!IMG_SavePNG_IO(image_, stream, false)) {
    SDL_CloseIO(stream);
    throw Exception(Exception::kRGSSError, SDL_GetError());
  }

  std::string data(static_cast<size_t>(SDL_GetIOSize(stream)), '\0');
  SDL_SeekIO(stream, 0, SDL_IO_SEEK_SET);
  SDL_ReadIO(stream, data.data(), data.size());
  SDL_CloseIO(stream);

  return data;
}

void Image::SaveFile(std::string filename) {
  Disposable::Guard();

  auto* stream = IOService::Get().OpenWrite(filename);
  if (!IMG_SavePNG_IO(image_, stream, true))
    throw Exception(Exception::kRGSSError, SDL_GetError());
}

void Image::DisposeObject() {
  SDL_DestroySurface(image_);
  image_ = nullptr;
}

}  // namespace urge

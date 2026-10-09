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

#include "core/palette.h"

#include "SDL3_image/SDL_image.h"

#include "core/exception.h"
#include "core/filesystem.h"

namespace urge {

static const SDL_PixelFormat kInternalPixelFormat = SDL_PIXELFORMAT_ABGR8888;

Palette::Palette(SDL_Surface* image) : image_(image) {}

Palette::Palette(int32_t width, int32_t height) {
  image_ = SDL_CreateSurface(width, height, kInternalPixelFormat);
}

Palette::Palette(std::string filename) {
  IOService::Get().OpenRead(
      filename, [&](SDL_IOStream* stream, const std::string& ext) {
        image_ = IMG_LoadTyped_IO(stream, true, ext.c_str());
        return !!image_;
      });

  if (image_->format != kInternalPixelFormat) {
    auto* converted_image = SDL_ConvertSurface(image_, kInternalPixelFormat);
    SDL_DestroySurface(image_);
    image_ = converted_image;
  }
}

Palette::Palette(RefPtr<Palette> other) {
  image_ = SDL_ConvertSurface(other->image_, kInternalPixelFormat);
}

Palette::~Palette() {
  Disposable::Dispose();
}

RefPtr<Color> Palette::GetPixel(int32_t x, int32_t y) {
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

void Palette::SetPixel(int32_t x, int32_t y, RefPtr<Color> color) {
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

RefPtr<Palette> Palette::FromDump(std::string data) {
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

  return MakeRefCounted<Palette>(image);
}

std::string Palette::ToDump() {
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

void Palette::SaveFile(std::string filename) {
  Disposable::Guard();

  auto* stream = IOService::Get().OpenWrite(filename);
  if (!IMG_SavePNG_IO(image_, stream, true))
    throw Exception(Exception::kRGSSError, SDL_GetError());
}

void Palette::DisposeObject() {
  SDL_DestroySurface(image_);
  image_ = nullptr;
}

}  // namespace urge

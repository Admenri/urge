// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/surface_impl.h"

#include "SDL3_image/SDL_image.h"

#include "components/filesystem/io_service.h"
#include "content/canvas/image_utils.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"
#include "content/io/iostream_impl.h"
#include "content/screen/renderscreen_impl.h"

namespace content {

// static
scoped_refptr<Surface> Surface::New(ExecutionContext* execution_context,
                                    uint32_t width,
                                    uint32_t height,
                                    ExceptionState& exception_state) {
  auto* surface_obj = SDL_CreateSurface(width, height, kSurfaceInternalFormat);
  if (!surface_obj) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<SurfaceImpl>(execution_context, surface_obj);
}

// static
scoped_refptr<Surface> Surface::New(ExecutionContext* execution_context,
                                    const std::string& filename,
                                    ExceptionState& exception_state) {
  SDL_Surface* surface_obj = nullptr;
  auto file_handler = base::BindRepeating(
      [](SDL_Surface** out, SDL_IOStream* ops, const std::string& ext) {
        *out = IMG_LoadTyped_IO(ops, true, ext.c_str());
        return !!*out;
      },
      &surface_obj);

  filesystem::IOState io_state;
  execution_context->io_service->OpenRead(filename, file_handler, &io_state);
  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::IO_ERROR, "%s (%s)",
                               io_state.error_message.c_str(),
                               filename.c_str());
    return nullptr;
  }

  if (!surface_obj) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                               SDL_GetError());
    return nullptr;
  }

  // Make format correct for data required
  MakeSureSurfaceFormat(surface_obj);

  return base::MakeRefCounted<SurfaceImpl>(execution_context, surface_obj);
}

// static
scoped_refptr<Surface> Surface::FromDump(ExecutionContext* execution_context,
                                         const std::string& dump_data,
                                         ExceptionState& exception_state) {
  return Surface::Deserialize(execution_context, dump_data, exception_state);
}

// static
scoped_refptr<Surface> Surface::FromStream(ExecutionContext* execution_context,
                                           scoped_refptr<IOStream> stream,
                                           const std::string& extname,
                                           ExceptionState& exception_state) {
  auto stream_obj = IOStreamImpl::From(stream);
  if (!Disposable::IsValid(stream_obj.get())) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid iostream input");
    return nullptr;
  }

  SDL_Surface* memory_texture =
      IMG_LoadTyped_IO(**stream_obj, false, extname.c_str());
  if (!memory_texture) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load image from iostream. (%s)",
                               SDL_GetError());
    return nullptr;
  }

  // Make format correct for data required
  MakeSureSurfaceFormat(memory_texture);

  return base::MakeRefCounted<SurfaceImpl>(execution_context, memory_texture);
}

// static
scoped_refptr<Surface> Surface::Copy(ExecutionContext* execution_context,
                                     scoped_refptr<Surface> other,
                                     ExceptionState& exception_state) {
  auto* src_surface = **SurfaceImpl::From(other);
  if (!src_surface) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid copy surface target");
    return nullptr;
  }

  auto* dst_surface = SDL_DuplicateSurface(src_surface);
  if (!dst_surface) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<SurfaceImpl>(execution_context, dst_surface);
}

// static
scoped_refptr<Surface> Surface::Deserialize(ExecutionContext* execution_context,
                                            const std::string& data,
                                            ExceptionState& exception_state) {
  // Surface dump data structure:
  //  [4Bytes uint32le width][4Bytes uint32le height][(w * h)Bytes void* pixels]
  //  Pixel Format: ABGR8888
  if (data.size() < sizeof(uint32_t) * 2) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid surface header dump data");
    return nullptr;
  }

  const auto* raw_data = reinterpret_cast<const uint32_t*>(data.data());
  const uint32_t width = *(raw_data + 0), height = *(raw_data + 1);
  const uint32_t pixel_size = width * height * 4;

  if (data.size() < sizeof(uint32_t) * 2 + pixel_size) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "invalid surface body dump data");
    return nullptr;
  }

  auto* surface_data = SDL_CreateSurface(width, height, kSurfaceInternalFormat);
  if (!surface_data) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  if (pixel_size)
    std::memcpy(surface_data->pixels, raw_data + 2, pixel_size);

  return base::MakeRefCounted<SurfaceImpl>(execution_context, surface_data);
}

// static
std::string Surface::Serialize(ExecutionContext* execution_context,
                               scoped_refptr<Surface> value,
                               ExceptionState& exception_state) {
  // Assumed value is not null.
  SDL_Surface* raw_surface = **SurfaceImpl::From(value);

  const uint32_t pixel_size = raw_surface->w * raw_surface->h * 4;
  std::string serialized_data(sizeof(uint32_t) * 2 + pixel_size, 0);

  const uint32_t surface_width = raw_surface->w,
                 surface_height = raw_surface->h;
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 0, &surface_width,
              sizeof(uint32_t));
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 1, &surface_height,
              sizeof(uint32_t));
  if (pixel_size)
    std::memcpy(serialized_data.data() + sizeof(uint32_t) * 2,
                raw_surface->pixels, pixel_size);

  return serialized_data;
}

///////////////////////////////////////////////////////////////////////////////
// Surface Implement

SurfaceImpl::SurfaceImpl(ExecutionContext* context, SDL_Surface* surface)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      surface_(surface),
      font_(base::MakeRefCounted<FontImpl>(context->font_context)) {
  DCHECK(surface_);
}

DISPOSABLE_DEFINITION(SurfaceImpl);

scoped_refptr<SurfaceImpl> SurfaceImpl::From(scoped_refptr<Surface> host) {
  return static_cast<SurfaceImpl*>(host.get());
}

uint32_t SurfaceImpl::Width(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);
  return surface_->w;
}

uint32_t SurfaceImpl::Height(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);
  return surface_->h;
}

scoped_refptr<Rect> SurfaceImpl::GetRect(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);
  return base::MakeRefCounted<RectImpl>(
      base::Rect(0, 0, surface_->w, surface_->h));
}

void SurfaceImpl::Blt(int32_t x,
                      int32_t y,
                      scoped_refptr<Surface> src_surface,
                      scoped_refptr<Rect> src_rect,
                      uint32_t opacity,
                      ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (!src_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid source rect");

  auto src_surface_obj = SurfaceImpl::From(src_surface);
  if (!Disposable::IsValid(src_surface_obj.get()))
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid source surface");

  auto* src_raw_surface = **src_surface_obj;
  const auto src_region = RectImpl::From(src_rect)->AsSDLRect();
  const SDL_Rect dest_pos = {x, y, 0, 0};

  SDL_SetSurfaceAlphaMod(src_raw_surface, static_cast<Uint8>(opacity));
  SDL_SetSurfaceBlendMode(src_raw_surface, SDL_BLENDMODE_BLEND);
  SDL_BlitSurface(src_raw_surface, &src_region, surface_, &dest_pos);
}

void SurfaceImpl::StretchBlt(scoped_refptr<Rect> dest_rect,
                             scoped_refptr<Surface> src_surface,
                             scoped_refptr<Rect> src_rect,
                             uint32_t opacity,
                             ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (!dest_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid destination rect");

  if (!src_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid source rect");

  auto src_surface_obj = SurfaceImpl::From(src_surface);
  if (!Disposable::IsValid(src_surface_obj.get()))
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid source surface");

  auto src_raw_surface = **src_surface_obj;
  const auto src_region = RectImpl::From(src_rect)->AsSDLRect();
  const auto dest_region = RectImpl::From(dest_rect)->AsSDLRect();

  SDL_SetSurfaceAlphaMod(src_raw_surface, std::clamp<Uint8>(opacity, 0, 255));
  SDL_SetSurfaceBlendMode(src_raw_surface, SDL_BLENDMODE_BLEND);
  SDL_BlitSurfaceScaled(src_raw_surface, &src_region, surface_, &dest_region,
                        SDL_SCALEMODE_LINEAR);
}

void SurfaceImpl::FillRect(int32_t x,
                           int32_t y,
                           uint32_t width,
                           uint32_t height,
                           scoped_refptr<Color> color,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (!color)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid color object");

  const auto sdl_color = ColorImpl::From(color)->AsSDLColor();
  const auto color32 =
      SDL_MapRGBA(SDL_GetPixelFormatDetails(surface_->format), nullptr,
                  sdl_color.r, sdl_color.g, sdl_color.b, sdl_color.a);
  const SDL_Rect rect{x, y, static_cast<int32_t>(width),
                      static_cast<int32_t>(height)};

  SDL_FillSurfaceRect(surface_, &rect, color32);
}

void SurfaceImpl::FillRect(scoped_refptr<Rect> rect,
                           scoped_refptr<Color> color,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid rect object");

  FillRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           color, exception_state);
}

void SurfaceImpl::Clear(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  SDL_ClearSurface(surface_, 0, 0, 0, 0);
}

void SurfaceImpl::ClearRect(int32_t x,
                            int32_t y,
                            uint32_t width,
                            uint32_t height,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  const SDL_Rect rect{x, y, static_cast<int32_t>(width),
                      static_cast<int32_t>(height)};
  SDL_FillSurfaceRect(surface_, &rect, 0);
}

void SurfaceImpl::ClearRect(scoped_refptr<Rect> rect,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid rect object");

  ClearRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
            rect->Get_Width(exception_state), rect->Get_Height(exception_state),
            exception_state);
}

scoped_refptr<Color> SurfaceImpl::GetPixel(int32_t x,
                                           int32_t y,
                                           ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  const auto* pixel_detail = SDL_GetPixelFormatDetails(surface_->format);
  const int32_t bpp = pixel_detail->bytes_per_pixel;
  const uint8_t* pixel = static_cast<uint8_t*>(surface_->pixels) +
                         static_cast<size_t>(y) * surface_->pitch +
                         static_cast<size_t>(x) * bpp;

  uint8_t color[4];
  SDL_GetRGBA(*reinterpret_cast<const uint32_t*>(pixel), pixel_detail, nullptr,
              &color[0], &color[1], &color[2], &color[3]);

  return base::MakeRefCounted<ColorImpl>(
      base::Vec4(color[0], color[1], color[2], color[3]));
}

void SurfaceImpl::SetPixel(int32_t x,
                           int32_t y,
                           scoped_refptr<Color> color,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

  if (!color)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid color object");

  scoped_refptr<ColorImpl> color_obj = ColorImpl::From(color.get());
  const SDL_Color color_unorm = color_obj->AsSDLColor();
  const auto* pixel_detail = SDL_GetPixelFormatDetails(surface_->format);
  const int32_t bpp = pixel_detail->bytes_per_pixel;
  uint8_t* pixel =
      static_cast<uint8_t*>(surface_->pixels) + y * surface_->pitch + x * bpp;
  *reinterpret_cast<uint32_t*>(pixel) =
      SDL_MapRGBA(pixel_detail, nullptr, color_unorm.r, color_unorm.g,
                  color_unorm.b, color_unorm.a);
}

void SurfaceImpl::DrawText(int32_t x,
                           int32_t y,
                           uint32_t width,
                           uint32_t height,
                           const std::string& str,
                           int32_t align,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* font_object = static_cast<FontImpl*>(font_.get());
  if (!font_object->GetCanonicalFont(exception_state))
    return;

  uint8_t font_opacity = 0;
  auto* text_surface =
      font_object->RenderText(str, &font_opacity, exception_state);
  if (!text_surface)
    return;

  SDL_Rect dest_rect{
      x,
      y,
      static_cast<int32_t>(width),
      static_cast<int32_t>(height),
  };

  SDL_SetSurfaceAlphaMod(text_surface, font_opacity);
  SDL_SetSurfaceBlendMode(text_surface, SDL_BLENDMODE_BLEND);
  SDL_BlitSurfaceScaled(text_surface, nullptr, surface_, &dest_rect,
                        SDL_SCALEMODE_LINEAR);
  SDL_DestroySurface(text_surface);
}

void SurfaceImpl::DrawText(int32_t x,
                           int32_t y,
                           uint32_t width,
                           uint32_t height,
                           const std::string& str,
                           ExceptionState& exception_state) {
  DrawText(x, y, width, height, str, 0, exception_state);
}

void SurfaceImpl::DrawText(scoped_refptr<Rect> rect,
                           const std::string& str,
                           int32_t align,
                           ExceptionState& exception_state) {
  DrawText(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           str, align, exception_state);
}

void SurfaceImpl::DrawText(scoped_refptr<Rect> rect,
                           const std::string& str,
                           ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "invalid rect object");

  DrawText(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           str, 0, exception_state);
}

scoped_refptr<Rect> SurfaceImpl::TextSize(const std::string& str,
                                          ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  TTF_Font* font =
      static_cast<FontImpl*>(font_.get())->GetCanonicalFont(exception_state);
  if (!font)
    return nullptr;

  int32_t w, h;
  TTF_GetStringSize(font, str.c_str(), str.size(), &w, &h);
  return base::MakeRefCounted<RectImpl>(base::Rect(0, 0, w, h));
}

std::string SurfaceImpl::DumpData(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(std::string());

  return Surface::Serialize(context(), this, exception_state);
}

void SurfaceImpl::SavePNG(const std::string& filename,
                          ExceptionState& exception_state) {
  DISPOSE_CHECK;

  filesystem::IOState io_state;
  auto* out_stream = context()->io_service->OpenWrite(filename, &io_state);
  if (io_state.error_count)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                                      io_state.error_message.c_str());

  if (!IMG_SavePNG_IO(surface_, out_stream, true))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                               SDL_GetError());
}

std::string SurfaceImpl::SavePNGData(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(std::string());

  auto* out_stream = SDL_IOFromDynamicMem();
  if (!IMG_SavePNG_IO(surface_, out_stream, false))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                               SDL_GetError());

  auto image_size = SDL_GetIOSize(out_stream);
  std::string result(image_size, 0);
  SDL_ReadIO(out_stream, result.data(), image_size);

  return result;
}

scoped_refptr<Font> SurfaceImpl::Get_Font(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return font_;
}

void SurfaceImpl::Put_Font(const scoped_refptr<Font>& value,
                           ExceptionState& exception_state) {
  DISPOSE_CHECK;

  *font_ = *FontImpl::From(value);
}

void SurfaceImpl::OnObjectDisposed() {
  SDL_DestroySurface(surface_);
  surface_ = nullptr;
}

}  // namespace content

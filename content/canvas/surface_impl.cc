// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/surface_impl.h"

#include "SDL3_image/SDL_image.h"

#include "components/filesystem/io_service.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/io/iostream_impl.h"
#include "content/screen/renderscreen_impl.h"

namespace content {

constexpr SDL_PixelFormat kSurfaceInternalFormat = SDL_PIXELFORMAT_ABGR8888;

scoped_refptr<Surface> Surface::New(ExecutionContext* execution_context,
                                    uint32_t width,
                                    uint32_t height,
                                    ExceptionState& exception_state) {
  auto* surface_data = SDL_CreateSurface(width, height, kSurfaceInternalFormat);
  if (!surface_data) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<SurfaceImpl>(
      execution_context->graphics, surface_data, execution_context->io_service);
}

scoped_refptr<Surface> Surface::New(ExecutionContext* execution_context,
                                    const base::String& filename,
                                    ExceptionState& exception_state) {
  SDL_Surface* surface_data = nullptr;
  auto file_handler = base::BindRepeating(
      [](SDL_Surface** surf, SDL_IOStream* ops, const base::String& ext) {
        *surf = IMG_LoadTyped_IO(ops, true, ext.c_str());
        return !!*surf;
      },
      &surface_data);

  filesystem::IOState io_state;
  execution_context->io_service->OpenRead(filename, file_handler, &io_state);

  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::IO_ERROR, "%s",
                               io_state.error_message.c_str());
    return nullptr;
  }

  if (!surface_data) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                               SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<SurfaceImpl>(
      execution_context->graphics, surface_data, execution_context->io_service);
}

scoped_refptr<Surface> Surface::FromDump(ExecutionContext* execution_context,
                                         const base::String& dump_data,
                                         ExceptionState& exception_state) {
  if (dump_data.size() < sizeof(uint32_t) * 2) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid surface header dump data.");
    return nullptr;
  }

  auto* raw_data = reinterpret_cast<const uint32_t*>(dump_data.data());
  uint32_t width = *(raw_data + 0), height = *(raw_data + 1);

  if (dump_data.size() < sizeof(uint32_t) * 2 + width * height * 4) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid surface body dump data.");
    return nullptr;
  }

  auto* surface_data = SDL_CreateSurface(width, height, kSurfaceInternalFormat);
  if (!surface_data) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  std::memcpy(surface_data->pixels, raw_data + 2,
              surface_data->pitch * surface_data->h);

  return base::MakeRefCounted<SurfaceImpl>(
      execution_context->graphics, surface_data, execution_context->io_service);
}

scoped_refptr<Surface> Surface::FromStream(ExecutionContext* execution_context,
                                           scoped_refptr<IOStream> stream,
                                           const base::String& extname,
                                           ExceptionState& exception_state) {
  auto stream_obj = IOStreamImpl::From(stream);
  if (!stream_obj || !stream_obj->GetRawStream()) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid iostream input.");
    return nullptr;
  }

  SDL_Surface* memory_texture =
      IMG_LoadTyped_IO(stream_obj->GetRawStream(), false, extname.c_str());
  if (!memory_texture) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load image from iostream. (%s)",
                               SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<SurfaceImpl>(execution_context->graphics,
                                           memory_texture,
                                           execution_context->io_service);
}

scoped_refptr<Surface> Surface::Copy(ExecutionContext* execution_context,
                                     scoped_refptr<Surface> other,
                                     ExceptionState& exception_state) {
  auto* src_surf = SurfaceImpl::From(other)->GetRawSurface();
  if (!src_surf) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid copy surface target.");
    return nullptr;
  }

  auto* dst_surf =
      SDL_CreateSurface(src_surf->w, src_surf->h, src_surf->format);
  std::memcpy(dst_surf->pixels, src_surf->pixels,
              src_surf->pitch * src_surf->h);
  if (!dst_surf) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<SurfaceImpl>(
      execution_context->graphics, dst_surf, execution_context->io_service);
}

scoped_refptr<Surface> Surface::Deserialize(ExecutionContext* execution_context,
                                            const base::String& data,
                                            ExceptionState& exception_state) {
  const uint8_t* raw_data = reinterpret_cast<const uint8_t*>(data.data());

  if (data.size() < sizeof(uint32_t) * 2) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid bitmap header data.");
    return nullptr;
  }

  uint32_t surface_width = 0, surface_height = 0;
  std::memcpy(&surface_width, raw_data + sizeof(uint32_t) * 0,
              sizeof(uint32_t));
  std::memcpy(&surface_height, raw_data + sizeof(uint32_t) * 1,
              sizeof(uint32_t));

  if (data.size() < sizeof(uint32_t) * 2 + surface_width * surface_height * 4) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid bitmap dump data.");
    return nullptr;
  }

  SDL_Surface* surface =
      SDL_CreateSurface(surface_width, surface_height, kSurfaceInternalFormat);
  std::memcpy(surface->pixels, raw_data + sizeof(uint32_t) * 2,
              surface->pitch * surface->h);

  return base::MakeRefCounted<SurfaceImpl>(execution_context->graphics, surface,
                                           execution_context->io_service);
}

base::String Surface::Serialize(ExecutionContext* execution_context,
                               scoped_refptr<Surface> value,
                               ExceptionState& exception_state) {
  scoped_refptr<SurfaceImpl> surface = SurfaceImpl::From(value);
  SDL_Surface* raw_surface = surface->GetRawSurface();

  base::String serialized_data(
      sizeof(uint32_t) * 2 + raw_surface->pitch * raw_surface->h, 0);

  uint32_t surface_width = raw_surface->w, surface_height = raw_surface->h;
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 0, &surface_width,
              sizeof(uint32_t));
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 1, &surface_height,
              sizeof(uint32_t));
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 2,
              raw_surface->pixels, raw_surface->pitch * raw_surface->h);

  return serialized_data;
}

SurfaceImpl::SurfaceImpl(DisposableCollection* parent,
                         SDL_Surface* surface,
                         filesystem::IOService* io_service)
    : Disposable(parent), surface_(surface), io_service_(io_service) {}

SurfaceImpl::~SurfaceImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

scoped_refptr<SurfaceImpl> SurfaceImpl::From(scoped_refptr<Surface> host) {
  return static_cast<SurfaceImpl*>(host.get());
}

void SurfaceImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool SurfaceImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint32_t SurfaceImpl::Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return surface_->w;
}

uint32_t SurfaceImpl::Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return surface_->h;
}

scoped_refptr<Rect> SurfaceImpl::GetRect(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return base::MakeRefCounted<RectImpl>(
      base::Rect(0, 0, surface_->w, surface_->h));
}

void SurfaceImpl::Blt(int32_t x,
                      int32_t y,
                      scoped_refptr<Surface> src_surface,
                      scoped_refptr<Rect> src_rect,
                      uint32_t opacity,
                      ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  auto* src_raw_surface = SurfaceImpl::From(src_surface)->GetRawSurface();
  if (!src_raw_surface)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid blt source.");

  if (!src_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid source rect.");

  auto src_region = RectImpl::From(src_rect)->AsSDLRect();
  SDL_Rect dest_pos = {x, y, 0, 0};

  SDL_BlitSurface(src_raw_surface, &src_region, surface_, &dest_pos);
}

void SurfaceImpl::StretchBlt(scoped_refptr<Rect> dest_rect,
                             scoped_refptr<Surface> src_surface,
                             scoped_refptr<Rect> src_rect,
                             uint32_t opacity,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (!dest_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid destination rect.");

  if (!src_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid source rect.");

  auto* src_raw_surface = SurfaceImpl::From(src_surface)->GetRawSurface();
  if (!src_raw_surface) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid blt source.");
    return;
  }

  auto src_region = RectImpl::From(src_rect)->AsSDLRect();
  auto dest_region = RectImpl::From(dest_rect)->AsSDLRect();

  SDL_BlitSurfaceScaled(src_raw_surface, &src_region, surface_, &dest_region,
                        SDL_SCALEMODE_LINEAR);
}

void SurfaceImpl::FillRect(int32_t x,
                           int32_t y,
                           uint32_t width,
                           uint32_t height,
                           scoped_refptr<Color> color,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (!color)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid color object.");

  auto sdl_color = ColorImpl::From(color)->AsSDLColor();
  auto color32 =
      SDL_MapRGBA(SDL_GetPixelFormatDetails(surface_->format), nullptr,
                  sdl_color.r, sdl_color.g, sdl_color.b, sdl_color.a);

  SDL_Rect rect{x, y, static_cast<int32_t>(width),
                static_cast<int32_t>(height)};
  SDL_FillSurfaceRect(surface_, &rect, color32);
}

void SurfaceImpl::FillRect(scoped_refptr<Rect> rect,
                           scoped_refptr<Color> color,
                           ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  FillRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           color, exception_state);
}

void SurfaceImpl::Clear(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  SDL_ClearSurface(surface_, 0, 0, 0, 0);
}

void SurfaceImpl::ClearRect(int32_t x,
                            int32_t y,
                            uint32_t width,
                            uint32_t height,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  SDL_Rect rect{x, y, static_cast<int32_t>(width),
                static_cast<int32_t>(height)};
  SDL_FillSurfaceRect(surface_, &rect, 0);
}

void SurfaceImpl::ClearRect(scoped_refptr<Rect> rect,
                            ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  ClearRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
            rect->Get_Width(exception_state), rect->Get_Height(exception_state),
            exception_state);
}

scoped_refptr<Color> SurfaceImpl::GetPixel(int32_t x,
                                           int32_t y,
                                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  auto* pixel_detail = SDL_GetPixelFormatDetails(surface_->format);
  int32_t bpp = pixel_detail->bytes_per_pixel;
  uint8_t* pixel = static_cast<uint8_t*>(surface_->pixels) +
                   static_cast<size_t>(y) * surface_->pitch +
                   static_cast<size_t>(x) * bpp;

  uint8_t color[4];
  SDL_GetRGBA(*reinterpret_cast<uint32_t*>(pixel), pixel_detail, nullptr,
              &color[0], &color[1], &color[2], &color[3]);

  return base::MakeRefCounted<ColorImpl>(
      base::Vec4(color[0], color[1], color[2], color[3]));
}

void SurfaceImpl::SetPixel(int32_t x,
                           int32_t y,
                           scoped_refptr<Color> color,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (!color)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid color object.");

  scoped_refptr<ColorImpl> color_obj = ColorImpl::From(color.get());
  const SDL_Color color_unorm = color_obj->AsSDLColor();
  auto* pixel_detail = SDL_GetPixelFormatDetails(surface_->format);
  int bpp = pixel_detail->bytes_per_pixel;
  uint8_t* pixel =
      static_cast<uint8_t*>(surface_->pixels) + y * surface_->pitch + x * bpp;
  *reinterpret_cast<uint32_t*>(pixel) =
      SDL_MapRGBA(pixel_detail, nullptr, color_unorm.r, color_unorm.g,
                  color_unorm.b, color_unorm.a);
}

base::String SurfaceImpl::DumpData(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return base::String();

  base::String data;
  size_t data_size = surface_->pitch * surface_->h;
  data.resize(sizeof(uint32_t) * 2 + data_size);

  auto* raw_data = reinterpret_cast<uint32_t*>(data.data());
  *(raw_data + 0) = surface_->w;
  *(raw_data + 1) = surface_->h;
  std::memcpy(data.data(), surface_->pixels, data_size);

  return data;
}

void SurfaceImpl::SavePNG(const base::String& filename,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  filesystem::IOState io_state;
  auto* out_stream = io_service_->OpenWrite(filename, &io_state);
  if (io_state.error_count)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                                      io_state.error_message.c_str());

  if (!IMG_SavePNG_IO(surface_, out_stream, true))
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, "%s",
                               SDL_GetError());
}

void SurfaceImpl::OnObjectDisposed() {
  SDL_DestroySurface(surface_);
  surface_ = nullptr;
}

}  // namespace content

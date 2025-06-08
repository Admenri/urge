// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/canvas_impl.h"

#include "SDL3_image/SDL_image.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "content/canvas/canvas_scheduler.h"
#include "content/canvas/surface_impl.h"
#include "content/common/color_impl.h"
#include "content/common/rect_impl.h"
#include "content/context/execution_context.h"
#include "content/io/iostream_impl.h"
#include "content/screen/renderscreen_impl.h"
#include "renderer/utils/texture_utils.h"

namespace content {

static constexpr SDL_PixelFormat kCanvasInternalFormat =
    SDL_PIXELFORMAT_ABGR8888;

scoped_refptr<Bitmap> Bitmap::New(ExecutionContext* execution_context,
                                  const base::String& filename,
                                  ExceptionState& exception_state) {
  return CanvasImpl::Create(execution_context, filename, exception_state);
}

scoped_refptr<Bitmap> Bitmap::New(ExecutionContext* execution_context,
                                  uint32_t width,
                                  uint32_t height,
                                  ExceptionState& exception_state) {
  return CanvasImpl::Create(execution_context, base::Vec2i(width, height),
                            exception_state);
}

scoped_refptr<Bitmap> Bitmap::FromSurface(ExecutionContext* execution_context,
                                          scoped_refptr<Surface> target,
                                          ExceptionState& exception_state) {
  auto surface_target = SurfaceImpl::From(target);
  auto* surface_data =
      surface_target ? surface_target->GetRawSurface() : nullptr;
  if (!surface_data) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid surface target.");
    return nullptr;
  }

  auto* surface_duplicate =
      SDL_CreateSurface(surface_data->w, surface_data->h, surface_data->format);
  std::memcpy(surface_duplicate->pixels, surface_data->pixels,
              surface_data->pitch * surface_data->h);
  if (!surface_duplicate) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to copy surface data.");
    return nullptr;
  }

  return base::MakeRefCounted<CanvasImpl>(execution_context, surface_duplicate,
                                          "SurfaceDataBitmap");
}

scoped_refptr<Bitmap> Bitmap::FromStream(ExecutionContext* execution_context,
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

  return base::MakeRefCounted<CanvasImpl>(execution_context, memory_texture,
                                          "IOStreamBitmap");
}

scoped_refptr<Bitmap> Bitmap::Copy(ExecutionContext* execution_context,
                                   scoped_refptr<Bitmap> other,
                                   ExceptionState& exception_state) {
  scoped_refptr<Bitmap> duplicate_bitmap =
      Bitmap::New(execution_context, other->Width(exception_state),
                  other->Height(exception_state), exception_state);
  if (!duplicate_bitmap)
    return nullptr;

  duplicate_bitmap->Blt(0, 0, other, other->GetRect(exception_state), 255, 0,
                        exception_state);

  return duplicate_bitmap;
}

scoped_refptr<Bitmap> Bitmap::Deserialize(ExecutionContext* execution_context,
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
      SDL_CreateSurface(surface_width, surface_height, kCanvasInternalFormat);
  std::memcpy(surface->pixels, raw_data + sizeof(uint32_t) * 2,
              surface->pitch * surface->h);

  return base::MakeRefCounted<CanvasImpl>(execution_context, surface,
                                          "MemoryData");
}

base::String Bitmap::Serialize(ExecutionContext* execution_context,
                               scoped_refptr<Bitmap> value,
                               ExceptionState& exception_state) {
  scoped_refptr<CanvasImpl> bitmap = CanvasImpl::FromBitmap(value);
  SDL_Surface* surface = bitmap->RequireMemorySurface();

  base::String serialized_data(
      sizeof(uint32_t) * 2 + surface->pitch * surface->h, 0);

  uint32_t surface_width = surface->w, surface_height = surface->h;
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 0, &surface_width,
              sizeof(uint32_t));
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 1, &surface_height,
              sizeof(uint32_t));
  std::memcpy(serialized_data.data() + sizeof(uint32_t) * 2, surface->pixels,
              surface->pitch * surface->h);

  return serialized_data;
}

scoped_refptr<CanvasImpl> CanvasImpl::Create(
    ExecutionContext* execution_context,
    const base::Vec2i& size,
    ExceptionState& exception_state) {
  if (size.x <= 0 || size.y <= 0) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Invalid bitmap size: %dx%d", size.x, size.y);
    return nullptr;
  }

  auto* empty_surface =
      SDL_CreateSurface(size.x, size.y, kCanvasInternalFormat);
  if (!empty_surface) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR, SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<CanvasImpl>(execution_context, empty_surface,
                                          "SizeBitmap");
}

scoped_refptr<CanvasImpl> CanvasImpl::Create(
    ExecutionContext* execution_context,
    const base::String& filename,
    ExceptionState& exception_state) {
  SDL_Surface* memory_texture = nullptr;
  auto file_handler = base::BindRepeating(
      [](SDL_Surface** surf, SDL_IOStream* ops, const base::String& ext) {
        *surf = IMG_LoadTyped_IO(ops, true, ext.c_str());
        return !!*surf;
      },
      &memory_texture);

  filesystem::IOState io_state;
  execution_context->io_service->OpenRead(filename, file_handler, &io_state);

  if (io_state.error_count) {
    exception_state.ThrowError(ExceptionCode::IO_ERROR,
                               "Failed to read file: %s (%s)", filename.c_str(),
                               io_state.error_message.c_str());
    return nullptr;
  }

  if (!memory_texture) {
    exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                               "Failed to load image: %s (%s)",
                               filename.c_str(), SDL_GetError());
    return nullptr;
  }

  return base::MakeRefCounted<CanvasImpl>(execution_context, memory_texture,
                                          filename);
}

CanvasImpl::CanvasImpl(ExecutionContext* execution_context,
                       SDL_Surface* memory_surface,
                       const base::String& debug_name)
    : EngineObject(execution_context),
      Disposable(execution_context->disposable_parent),
      name_(debug_name),
      surface_(memory_surface),
      font_(base::MakeRefCounted<FontImpl>(execution_context->font_context)) {
  // Add link in scheduler node
  context()->canvas_scheduler->children_.Append(this);

  // Create renderer texture
  GPUCreateTextureWithDataInternal();
}

CanvasImpl::~CanvasImpl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

scoped_refptr<CanvasImpl> CanvasImpl::FromBitmap(scoped_refptr<Bitmap> host) {
  return static_cast<CanvasImpl*>(host.get());
}

SDL_Surface* CanvasImpl::RequireMemorySurface() {
  if (!surface_) {
    // Create empty cache
    surface_ =
        SDL_CreateSurface(agent_.size.x, agent_.size.y, kCanvasInternalFormat);

    // Submit pending commands
    SubmitQueuedCommands();

    // Sync fetch pixels to memory
    GPUFetchTexturePixelsDataInternal();
  }

  return surface_;
}

void CanvasImpl::InvalidateSurfaceCache() {
  // Notify children observers
  observers_.Notify();

  if (surface_) {
    // Set cache ptr to null
    SDL_DestroySurface(surface_);
    surface_ = nullptr;
  }
}

void CanvasImpl::SubmitQueuedCommands() {
  Command* command_sequence = commands_;
  // Execute pending commands,
  // encode draw command in wgpu encoder.
  while (command_sequence) {
    switch (command_sequence->id) {
      case CommandID::CLEAR: {
        GPUCanvasClearInternal();
      } break;
      case CommandID::GRADIENT_FILL_RECT: {
        const auto* c =
            static_cast<Command_GradientFillRect*>(command_sequence);
        GPUCanvasGradientFillRectInternal(c->region, c->color1, c->color2,
                                          c->vertical);
      } break;
      case CommandID::HUE_CHANGE: {
        const auto* c = static_cast<Command_HueChange*>(command_sequence);
        GPUCanvasHueChange(c->hue);
      } break;
      case CommandID::RADIAL_BLUR: {
        const auto* c = static_cast<Command_RadialBlur*>(command_sequence);
        // TODO:
        std::ignore = c;
      } break;
      case CommandID::DRAW_TEXT: {
        const auto* c = static_cast<Command_DrawText*>(command_sequence);
        GPUCanvasDrawTextSurfaceInternal(c->region, c->text, c->opacity,
                                         c->align);
      } break;
      default:
        break;
    }

    command_sequence = command_sequence->next;
  }

  // Clear command pool,
  // no memory release.
  ClearPendingCommands();
}

void CanvasImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool CanvasImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint32_t CanvasImpl::Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return agent_.size.x;
}

uint32_t CanvasImpl::Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return agent_.size.y;
}

scoped_refptr<Rect> CanvasImpl::GetRect(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return base::MakeRefCounted<RectImpl>(agent_.size);
}

void CanvasImpl::Blt(int32_t x,
                     int32_t y,
                     scoped_refptr<Bitmap> src_bitmap,
                     scoped_refptr<Rect> src_rect,
                     uint32_t opacity,
                     int32_t blend_type,
                     ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CanvasImpl* src_canvas = static_cast<CanvasImpl*>(src_bitmap.get());
  if (!src_canvas || !src_canvas->GetAgent())
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid blt source.");

  if (!src_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid source rect object.");

  BlitTextureInternal(base::Rect(x, y, src_rect->Get_Width(exception_state),
                                 src_rect->Get_Height(exception_state)),
                      src_canvas, RectImpl::From(src_rect)->AsBaseRect(),
                      blend_type, opacity);
}

void CanvasImpl::StretchBlt(scoped_refptr<Rect> dest_rect,
                            scoped_refptr<Bitmap> src_bitmap,
                            scoped_refptr<Rect> src_rect,
                            uint32_t opacity,
                            int32_t blend_type,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  CanvasImpl* src_canvas = static_cast<CanvasImpl*>(src_bitmap.get());
  if (!src_canvas || !src_canvas->GetAgent())
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid blt source.");

  if (!dest_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid destination rect object.");

  if (!src_rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid source rect object.");

  BlitTextureInternal(RectImpl::From(dest_rect)->AsBaseRect(), src_canvas,
                      RectImpl::From(src_rect)->AsBaseRect(), blend_type,
                      opacity);
}

void CanvasImpl::FillRect(int32_t x,
                          int32_t y,
                          uint32_t width,
                          uint32_t height,
                          scoped_refptr<Color> color,
                          ExceptionState& exception_state) {
  GradientFillRect(x, y, width, height, color, color, false, exception_state);
}

void CanvasImpl::FillRect(scoped_refptr<Rect> rect,
                          scoped_refptr<Color> color,
                          ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  FillRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           color, exception_state);
}

void CanvasImpl::GradientFillRect(int32_t x,
                                  int32_t y,
                                  uint32_t width,
                                  uint32_t height,
                                  scoped_refptr<Color> color1,
                                  scoped_refptr<Color> color2,
                                  bool vertical,
                                  ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (width <= 0 || height <= 0)
    return;

  if (!color1 || !color2)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid color object.");

  auto* command = AllocateCommand<Command_GradientFillRect>();
  command->region = base::Rect(x, y, width, height);
  command->color1 = ColorImpl::From(color1.get())->AsNormColor();
  command->color2 = ColorImpl::From(color2.get())->AsNormColor();
  command->vertical = vertical;

  InvalidateSurfaceCache();
}

void CanvasImpl::GradientFillRect(int32_t x,
                                  int32_t y,
                                  uint32_t width,
                                  uint32_t height,
                                  scoped_refptr<Color> color1,
                                  scoped_refptr<Color> color2,
                                  ExceptionState& exception_state) {
  GradientFillRect(x, y, width, height, color1, color2, false, exception_state);
}

void CanvasImpl::GradientFillRect(scoped_refptr<Rect> rect,
                                  scoped_refptr<Color> color1,
                                  scoped_refptr<Color> color2,
                                  bool vertical,
                                  ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  GradientFillRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
                   rect->Get_Width(exception_state),
                   rect->Get_Height(exception_state), color1, color2, vertical,
                   exception_state);
}

void CanvasImpl::GradientFillRect(scoped_refptr<Rect> rect,
                                  scoped_refptr<Color> color1,
                                  scoped_refptr<Color> color2,
                                  ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  GradientFillRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
                   rect->Get_Width(exception_state),
                   rect->Get_Height(exception_state), color1, color2, false,
                   exception_state);
}

void CanvasImpl::Clear(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  AllocateCommand<Command_Clear>();
  InvalidateSurfaceCache();
}

void CanvasImpl::ClearRect(int32_t x,
                           int32_t y,
                           uint32_t width,
                           uint32_t height,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  auto* command = AllocateCommand<Command_GradientFillRect>();
  command->region = base::Rect(x, y, width, height);
  command->color1 = base::Vec4(0);
  command->color2 = command->color1;
  command->vertical = false;

  InvalidateSurfaceCache();
}

void CanvasImpl::ClearRect(scoped_refptr<Rect> rect,
                           ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  ClearRect(rect->Get_X(exception_state), rect->Get_Y(exception_state),
            rect->Get_Width(exception_state), rect->Get_Height(exception_state),
            exception_state);
}

scoped_refptr<Color> CanvasImpl::GetPixel(int32_t x,
                                          int32_t y,
                                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  if (x < 0 || x >= agent_.size.x || y < 0 || y >= agent_.size.y)
    return nullptr;

  SDL_Surface* surface = RequireMemorySurface();
  auto* pixel_detail = SDL_GetPixelFormatDetails(surface->format);
  int32_t bpp = pixel_detail->bytes_per_pixel;
  uint8_t* pixel = static_cast<uint8_t*>(surface->pixels) +
                   static_cast<size_t>(y) * surface->pitch +
                   static_cast<size_t>(x) * bpp;

  uint8_t color[4];
  SDL_GetRGBA(*reinterpret_cast<uint32_t*>(pixel), pixel_detail, nullptr,
              &color[0], &color[1], &color[2], &color[3]);

  return base::MakeRefCounted<ColorImpl>(
      base::Vec4(color[0], color[1], color[2], color[3]));
}

void CanvasImpl::SetPixel(int32_t x,
                          int32_t y,
                          scoped_refptr<Color> color,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (x < 0 || x >= agent_.size.x || y < 0 || y >= agent_.size.y)
    return;

  if (!color)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid color object.");

  scoped_refptr<ColorImpl> color_obj = ColorImpl::From(color.get());

  auto* command = AllocateCommand<Command_GradientFillRect>();
  command->region = base::Rect(x, y, 1, 1);
  command->color1 = color_obj->AsNormColor();
  command->color2 = command->color1;
  command->vertical = false;

  SDL_Surface* surface = RequireMemorySurface();
  if (surface) {
    const SDL_Color color_unorm = color_obj->AsSDLColor();
    auto* pixel_detail = SDL_GetPixelFormatDetails(surface->format);
    int bpp = pixel_detail->bytes_per_pixel;
    uint8_t* pixel =
        static_cast<uint8_t*>(surface->pixels) + y * surface->pitch + x * bpp;
    *reinterpret_cast<uint32_t*>(pixel) =
        SDL_MapRGBA(pixel_detail, nullptr, color_unorm.r, color_unorm.g,
                    color_unorm.b, color_unorm.a);
  }
}

void CanvasImpl::HueChange(int32_t hue, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (hue % 360 == 0)
    return;

  while (hue < 0)
    hue += 359;
  hue %= 359;

  auto* command = AllocateCommand<Command_HueChange>();
  command->hue = hue;

  InvalidateSurfaceCache();
}

void CanvasImpl::Blur(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  auto* command = AllocateCommand<Command_RadialBlur>();
  command->blur = true;

  InvalidateSurfaceCache();
}

void CanvasImpl::RadialBlur(int32_t angle,
                            int32_t division,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  auto* command = AllocateCommand<Command_RadialBlur>();
  command->blur = false;
  command->angle = angle;
  command->division = division;

  InvalidateSurfaceCache();
}

void CanvasImpl::DrawText(int32_t x,
                          int32_t y,
                          uint32_t width,
                          uint32_t height,
                          const base::String& str,
                          int32_t align,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  auto* font_object = static_cast<FontImpl*>(font_.get());
  if (!font_object->GetCanonicalFont(exception_state))
    return;

  uint8_t font_opacity = 0;
  auto* text_surface =
      font_object->RenderText(str, &font_opacity, exception_state);
  if (!text_surface)
    return;

  // Destroy memory surface in queue deferrer.
  auto* command = AllocateCommand<Command_DrawText>();
  command->region = base::Rect(x, y, width, height);
  command->text = text_surface;  // Transient surface object
  command->opacity = static_cast<float>(font_opacity);
  command->align = align;

  InvalidateSurfaceCache();
}

void CanvasImpl::DrawText(int32_t x,
                          int32_t y,
                          uint32_t width,
                          uint32_t height,
                          const base::String& str,
                          ExceptionState& exception_state) {
  DrawText(x, y, width, height, str, 0, exception_state);
}

void CanvasImpl::DrawText(scoped_refptr<Rect> rect,
                          const base::String& str,
                          int32_t align,
                          ExceptionState& exception_state) {
  DrawText(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           str, align, exception_state);
}

void CanvasImpl::DrawText(scoped_refptr<Rect> rect,
                          const base::String& str,
                          ExceptionState& exception_state) {
  if (!rect)
    return exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                                      "Invalid rect object.");

  DrawText(rect->Get_X(exception_state), rect->Get_Y(exception_state),
           rect->Get_Width(exception_state), rect->Get_Height(exception_state),
           str, 0, exception_state);
}

scoped_refptr<Rect> CanvasImpl::TextSize(const base::String& str,
                                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  TTF_Font* font =
      static_cast<FontImpl*>(font_.get())->GetCanonicalFont(exception_state);
  if (!font)
    return nullptr;

  int32_t w, h;
  TTF_GetStringSize(font, str.c_str(), str.size(), &w, &h);
  return base::MakeRefCounted<RectImpl>(base::Rect(0, 0, w, h));
}

scoped_refptr<Surface> CanvasImpl::GetSurface(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  SDL_Surface* surface = RequireMemorySurface();
  return base::MakeRefCounted<SurfaceImpl>(context(), surface);
}

void CanvasImpl::OnObjectDisposed() {
  // Unlink from canvas scheduler
  base::LinkNode<CanvasImpl>::RemoveFromList();

  // Destroy GPU texture
  Agent empty_agent;
  std::swap(agent_, empty_agent);

  // Free memory surface cache
  InvalidateSurfaceCache();
}

void CanvasImpl::BlitTextureInternal(const base::Rect& dst_rect,
                                     CanvasImpl* src_texture,
                                     const base::Rect& src_rect,
                                     int32_t blend_type,
                                     uint32_t alpha) {
  // Synchronize pending queue immediately,
  // blit the sourcetexture to destination texture immediately.
  src_texture->SubmitQueuedCommands();
  SubmitQueuedCommands();

  // Clamp blend type
  blend_type =
      std::clamp<int32_t>(blend_type, 0, renderer::BLEND_TYPE_NUMS - 1);

  // Execute blit immediately.
  GPUBlendBlitTextureInternal(dst_rect, &src_texture->agent_, src_rect,
                              blend_type, alpha);

  // Invalidate memory cache
  InvalidateSurfaceCache();
}

scoped_refptr<Font> CanvasImpl::Get_Font(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return font_;
}

void CanvasImpl::Put_Font(const scoped_refptr<Font>& value,
                          ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  *font_ = *FontImpl::From(value);
}

void CanvasImpl::GPUCreateTextureWithDataInternal() {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();

  // Make sure correct texture format
  if (surface_->format != kCanvasInternalFormat) {
    SDL_Surface* conv = SDL_ConvertSurface(surface_, kCanvasInternalFormat);
    SDL_DestroySurface(surface_);
    surface_ = conv;
  }

  // Setup GPU texture
  agent_.name = "bitmap.texture<\"" + name_ + "\">";
  agent_.size = base::Vec2i(surface_->w, surface_->h);
  renderer::CreateTexture2D(
      *render_device, &agent_.data, agent_.name, surface_,
      Diligent::USAGE_DEFAULT,
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE);

  // Setup access texture view
  agent_.resource =
      agent_.data->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  agent_.target =
      agent_.data->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);

  // Make bitmap draw transform
  // Make transform matrix uniform for discrete draw command
  renderer::WorldTransform world_matrix;
  renderer::MakeProjectionMatrix(world_matrix.projection, agent_.size);
  renderer::MakeIdentityMatrix(world_matrix.transform);

  // Per bitmap create uniform once
  Diligent::CreateUniformBuffer(
      *render_device, sizeof(world_matrix), "bitmap.binding.world",
      &agent_.world_buffer, Diligent::USAGE_IMMUTABLE,
      Diligent::BIND_UNIFORM_BUFFER, Diligent::CPU_ACCESS_WRITE, &world_matrix);
}

void CanvasImpl::GPUResetEffectLayerIfNeed() {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();

  // Create an intermediate layer if bitmap need filter effect,
  // the bitmap is fixed size, it is unnecessary to reset effect layer.
  if (!agent_.effect_layer)
    renderer::CreateTexture2D(*render_device, &agent_.effect_layer,
                              "bitmap.filter.intermediate", agent_.size);
}

void CanvasImpl::GPUBlendBlitTextureInternal(const base::Rect& dst_region,
                                             Agent* src_texture,
                                             const base::Rect& src_region,
                                             int32_t blend_type,
                                             uint32_t blit_alpha) {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();
  auto& render_context = *scheduler->GetDiscreteRenderContext();

  // Custom blend blit pipeline
  auto& pipeline_set = render_device.GetPipelines()->base;
  auto* pipeline =
      pipeline_set.GetPipeline(static_cast<renderer::BlendType>(blend_type));

  // Norm opacity value
  base::Vec4 blend_alpha;
  blend_alpha.w = static_cast<float>(blit_alpha) / 255.0f;

  // Make drawing vertices
  renderer::Quad transient_quad;
  renderer::Quad::SetTexCoordRect(&transient_quad, src_region,
                                  src_texture->size);
  renderer::Quad::SetPositionRect(&transient_quad, dst_region);
  renderer::Quad::SetColor(&transient_quad, blend_alpha);
  scheduler->quad_batch().QueueWrite(*render_context, &transient_quad);

  // Setup render target
  scheduler->SetupRenderTarget(agent_.target, false);

  // Push scissor
  render_context.ScissorState()->Push(agent_.size);

  // Setup uniform params
  scheduler->base_binding().u_transform->Set(agent_.world_buffer);
  scheduler->base_binding().u_texture->Set(src_texture->resource);

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *scheduler->base_binding(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *scheduler->quad_batch();
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **render_device.GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  render_context->DrawIndexed(draw_indexed_attribs);

  // Pop scissor region
  render_context.ScissorState()->Pop();
}

void CanvasImpl::GPUFetchTexturePixelsDataInternal() {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();
  auto& render_context = *scheduler->GetDiscreteRenderContext();

  // Create transient read stage texture
  Diligent::TextureDesc stage_buffer_desc = agent_.data->GetDesc();
  stage_buffer_desc.Name = "bitmap.readstage.buffer";
  stage_buffer_desc.Usage = Diligent::USAGE_STAGING;
  stage_buffer_desc.BindFlags = Diligent::BIND_NONE;
  stage_buffer_desc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;

  RRefPtr<Diligent::ITexture> stage_read_buffer;
  render_device->CreateTexture(stage_buffer_desc, nullptr, &stage_read_buffer);

  // Reset render target
  scheduler->SetupRenderTarget(nullptr, false);

  // Copy textrue to temp buffer
  Diligent::CopyTextureAttribs copy_texture_attribs(
      agent_.data.RawPtr(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      stage_read_buffer.RawPtr(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->CopyTexture(copy_texture_attribs);
  render_context->WaitForIdle();

  // Read buffer data from mapping buffer
  Diligent::MappedTextureSubresource MappedData;
  render_context->MapTextureSubresource(
      stage_read_buffer, 0, 0, Diligent::MAP_READ,
      Diligent::MAP_FLAG_DO_NOT_WAIT, nullptr, MappedData);

  if (MappedData.pData) {
    // Fill in memory surface
    uint8_t* dst_data = static_cast<uint8_t*>(surface_->pixels);
    uint8_t* src_data = static_cast<uint8_t*>(MappedData.pData);
    for (int32_t i = 0; i < surface_->h; ++i) {
      std::memcpy(dst_data, src_data, surface_->pitch);
      dst_data += surface_->pitch;
      src_data += MappedData.Stride;
    }
  }

  // End of mapping
  render_context->UnmapTextureSubresource(stage_read_buffer, 0, 0);
}

void CanvasImpl::GPUCanvasClearInternal() {
  // Clear all data in texture
  context()->canvas_scheduler->SetupRenderTarget(agent_.target, true);
}

void CanvasImpl::GPUCanvasGradientFillRectInternal(const base::Rect& region,
                                                   const base::Vec4& color1,
                                                   const base::Vec4& color2,
                                                   bool vertical) {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();
  auto& render_context = *scheduler->GetDiscreteRenderContext();

  auto& pipeline_set = render_device.GetPipelines()->color;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND);

  // Make transient vertices data
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad, region);
  if (vertical) {
    transient_quad.vertices[0].color = color1;
    transient_quad.vertices[1].color = color1;
    transient_quad.vertices[2].color = color2;
    transient_quad.vertices[3].color = color2;
  } else {
    transient_quad.vertices[0].color = color1;
    transient_quad.vertices[1].color = color2;
    transient_quad.vertices[2].color = color2;
    transient_quad.vertices[3].color = color1;
  }
  scheduler->quad_batch().QueueWrite(*render_context, &transient_quad);

  // Setup render target
  scheduler->SetupRenderTarget(agent_.target, false);

  // Push scissor
  render_context.ScissorState()->Push(agent_.size);

  // Setup uniform params
  scheduler->color_binding().u_transform->Set(agent_.world_buffer);

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *scheduler->color_binding(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *scheduler->quad_batch();
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **render_device.GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  render_context->DrawIndexed(draw_indexed_attribs);

  // Pop scissor
  render_context.ScissorState()->Pop();
}

void CanvasImpl::GPUCanvasDrawTextSurfaceInternal(const base::Rect& region,
                                                  SDL_Surface* text,
                                                  float opacity,
                                                  int32_t align) {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();
  auto& render_context = *scheduler->GetDiscreteRenderContext();

  auto& pipeline_set = render_device.GetPipelines()->base;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NORMAL);

  // Reset text upload stage buffer if need
  if (!agent_.text_cache_texture || agent_.text_cache_size.x < text->w ||
      agent_.text_cache_size.y < text->h) {
    agent_.text_cache_size.x =
        std::max<int32_t>(agent_.text_cache_size.x, text->w);
    agent_.text_cache_size.y =
        std::max<int32_t>(agent_.text_cache_size.y, text->h);

    agent_.text_cache_texture.Release();
    renderer::CreateTexture2D(*render_device, &agent_.text_cache_texture,
                              "textdraw.cache", agent_.text_cache_size);

    if ((*render_device)->GetDeviceInfo().IsGLDevice())
      render_context->TransitionShaderResources(*scheduler->base_binding());
  }

  // Update text texture cache
  Diligent::TextureSubResData texture_sub_res_data(text->pixels, text->pitch);
  Diligent::Box box(0, text->w, 0, text->h);

  render_context->UpdateTexture(
      agent_.text_cache_texture, 0, 0, box, texture_sub_res_data,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Render text stage data to current texture
  base::Vec4 blend_alpha;
  blend_alpha.w = opacity / 255.0f;

  int32_t align_x = region.x,
          align_y = region.y + (region.height - text->h) / 2;
  switch (align) {
    default:
    case 0:  // Left
      break;
    case 1:  // Center
      align_x += (region.width - text->w) / 2;
      break;
    case 2:  // Right
      align_x += region.width - text->w;
      break;
  }

  // Assemble attributes
  float zoom_x = static_cast<float>(region.width) / text->w;
  zoom_x = std::min(zoom_x, 1.0f);
  const base::Rect compose_position(align_x, align_y, text->w * zoom_x,
                                    text->h);
  const base::Vec2i text_cache_size(
      agent_.text_cache_texture->GetDesc().Width,
      agent_.text_cache_texture->GetDesc().Height);

  // Make render vertices
  renderer::Quad transient_quad;
  renderer::Quad::SetTexCoordRect(&transient_quad, base::Vec2(text->w, text->h),
                                  text_cache_size);
  renderer::Quad::SetPositionRect(&transient_quad, compose_position);
  renderer::Quad::SetColor(&transient_quad, blend_alpha);
  scheduler->quad_batch().QueueWrite(*render_context, &transient_quad);

  // Setup render target
  scheduler->SetupRenderTarget(agent_.target, false);

  // Push scissor
  render_context.ScissorState()->Push(agent_.size);

  // Setup uniform params
  scheduler->base_binding().u_transform->Set(agent_.world_buffer);
  scheduler->base_binding().u_texture->Set(
      agent_.text_cache_texture->GetDefaultView(
          Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *scheduler->base_binding(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *scheduler->quad_batch();
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **render_device.GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  render_context->DrawIndexed(draw_indexed_attribs);

  // Pop scissor
  render_context.ScissorState()->Pop();
}

void CanvasImpl::GPUCanvasHueChange(int32_t hue) {
  auto* scheduler = context()->canvas_scheduler;
  auto& render_device = *scheduler->GetRenderDevice();
  auto& render_context = *scheduler->GetDiscreteRenderContext();

  auto& pipeline_set = render_device.GetPipelines()->bitmaphue;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BLEND_TYPE_NO_BLEND);

  GPUResetEffectLayerIfNeed();

  // Copy current texture to stage intermediate texture
  scheduler->SetupRenderTarget(nullptr, false);

  Diligent::CopyTextureAttribs copy_texture_attribs(
      agent_.data, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      agent_.effect_layer, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->CopyTexture(copy_texture_attribs);

  // Make transient vertices
  renderer::Quad transient_quad;
  renderer::Quad::SetPositionRect(&transient_quad,
                                  base::RectF(-1.0f, 1.0f, 2.0f, -2.0f));
  renderer::Quad::SetTexCoordRectNorm(&transient_quad,
                                      base::RectF(0.0f, 0.0f, 1.0f, 1.0f));
  renderer::Quad::SetColor(&transient_quad, base::Vec4(hue / 360.0f));
  scheduler->quad_batch().QueueWrite(*render_context, &transient_quad);

  // Setup render target
  scheduler->SetupRenderTarget(agent_.target, false);

  // Push scissor
  render_context.ScissorState()->Push(agent_.size);

  // Setup uniform params
  scheduler->hue_binding().u_texture->Set(agent_.effect_layer->GetDefaultView(
      Diligent::TEXTURE_VIEW_SHADER_RESOURCE));

  // Apply pipeline state
  render_context->SetPipelineState(pipeline);
  render_context->CommitShaderResources(
      *scheduler->hue_binding(),
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Apply vertex index
  Diligent::IBuffer* const vertex_buffer = *scheduler->quad_batch();
  render_context->SetVertexBuffers(
      0, 1, &vertex_buffer, nullptr,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  render_context->SetIndexBuffer(
      **render_device.GetQuadIndex(), 0,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  // Execute render command
  Diligent::DrawIndexedAttribs draw_indexed_attribs;
  draw_indexed_attribs.NumIndices = 6;
  draw_indexed_attribs.IndexType = renderer::QuadIndexCache::kValueType;
  render_context->DrawIndexed(draw_indexed_attribs);

  // Pop scissor
  render_context.ScissorState()->Pop();
}

}  // namespace content

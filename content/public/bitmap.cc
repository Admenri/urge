// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#include "content/public/bitmap.h"

#include <array>

#include "base/exception/exception.h"
#include "components/filesystem/filesystem.h"
#include "content/public/font.h"

#include "SDL3_image/SDL_image.h"

namespace content {

namespace {

uint16_t utf8_to_ucs2(const char* _input, const char** end_ptr) {
  const unsigned char* input = reinterpret_cast<const unsigned char*>(_input);
  *end_ptr = _input;

  if (input[0] == 0)
    return -1;

  if (input[0] < 0x80) {
    *end_ptr = _input + 1;

    return input[0];
  }

  if ((input[0] & 0xE0) == 0xE0) {
    if (input[1] == 0 || input[2] == 0)
      return -1;

    *end_ptr = _input + 3;

    return (input[0] & 0x0F) << 12 | (input[1] & 0x3F) << 6 | (input[2] & 0x3F);
  }

  if ((input[0] & 0xC0) == 0xC0) {
    if (input[1] == 0)
      return -1;

    *end_ptr = _input + 2;

    return (input[0] & 0x1F) << 6 | (input[1] & 0x3F);
  }

  return -1;
}

Diligent::TextureDesc MakeTextureDesc(Diligent::TEXTURE_FORMAT texfmt) {
  Diligent::TextureDesc TexDesc;
  TexDesc.Type = Diligent::RESOURCE_DIM_TEX_2D;
  TexDesc.Format = texfmt;
  TexDesc.Usage = Diligent::USAGE_DEFAULT;
  TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_WRITE;
  TexDesc.BindFlags =
      Diligent::BIND_RENDER_TARGET | Diligent::BIND_SHADER_RESOURCE;

  return TexDesc;
}

}  // namespace

Bitmap::Bitmap(scoped_refptr<Graphics> host, const base::Vec2i& size)
    : GraphicsElement(host.get()),
      Disposable(host.get()),
      font_(new Font(host->font_manager())),
      surface_buffer_(nullptr) {
  if (size.x <= 0 || size.y <= 0) {
    throw base::Exception(base::Exception::ContentError,
                          "Invalid bitmap create size: (%dx%d)", size.x,
                          size.y);
  }

  uint32_t size_limit = host->renderer()
                            ->device()
                            ->GetAdapterInfo()
                            .Texture.MaxTexture2DDimension;
  if (size.x > size_limit || size.y > size_limit) {
    throw base::Exception(base::Exception::RendererError,
                          "Unable to load large image: (%dx%d)",
                          surface_buffer_->w, surface_buffer_->h);
  }

  Diligent::TextureDesc TexDesc = MakeTextureDesc(host->tex_format());
  TexDesc.Name = "Bitmap texture";
  TexDesc.Width = size.x;
  TexDesc.Height = size.y;

  host->renderer()->device()->CreateTexture(TexDesc, nullptr, &texture_);
}

Bitmap::Bitmap(scoped_refptr<Graphics> host,
               filesystem::Filesystem* io,
               const std::string& filename)
    : GraphicsElement(host.get()),
      Disposable(host.get()),
      font_(new Font(host->font_manager())),
      surface_buffer_(nullptr) {
  auto file_handler = base::BindRepeating(
      [](SDL_Surface** surf, SDL_IOStream* ops, const std::string& ext) {
        *surf = IMG_LoadTyped_IO(ops, true, ext.c_str());

        return !!*surf;
      },
      &surface_buffer_);
  io->OpenRead(filename, file_handler);

  if (!surface_buffer_) {
    throw base::Exception(base::Exception::ContentError,
                          "Failed to load image: '%s': %s", filename.c_str(),
                          SDL_GetError());
  }

  uint32_t size_limit = host->renderer()
                            ->device()
                            ->GetAdapterInfo()
                            .Texture.MaxTexture2DDimension;
  if (surface_buffer_->w > size_limit || surface_buffer_->h > size_limit) {
    throw base::Exception(base::Exception::RendererError,
                          "Unable to load large image: (%dx%d)",
                          surface_buffer_->w, surface_buffer_->h);
  }

  if (surface_buffer_->format != SDL_PIXELFORMAT_ABGR8888) {
    SDL_Surface* conv =
        SDL_ConvertSurface(surface_buffer_, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surface_buffer_);
    surface_buffer_ = conv;
  }

  size_t img_size = surface_buffer_->w * surface_buffer_->h * 4;

  Diligent::TextureDesc TexDesc = MakeTextureDesc(host->tex_format());
  TexDesc.Name = "Bitmap texture";
  TexDesc.Width = surface_buffer_->w;
  TexDesc.Height = surface_buffer_->h;

  Diligent::TextureData TexData;
  Diligent::TextureSubResData TexSubData(surface_buffer_->pixels,
                                         surface_buffer_->pitch);
  TexData.pContext = screen()->renderer()->context();
  TexData.NumSubresources = 1;
  TexData.pSubResources = &TexSubData;

  host->renderer()->device()->CreateTexture(TexDesc, &TexData, &texture_);

  SDL_DestroySurface(surface_buffer_);
  surface_buffer_ = nullptr;
}

Bitmap::~Bitmap() {
  Dispose();
}

scoped_refptr<Bitmap> Bitmap::Clone() {
  CheckIsDisposed();

  scoped_refptr<Bitmap> new_bitmap =
      new Bitmap(static_cast<Graphics*>(screen()), GetSize());
  new_bitmap->Blt(base::Vec2i(), this, GetSize());
  *new_bitmap->font_ = *font_;

  return new_bitmap;
}

base::Vec2i Bitmap::GetSize() const {
  return base::Vec2i(texture_->GetDesc().Width, texture_->GetDesc().Height);
}

void Bitmap::Blt(const base::Vec2i& pos,
                 scoped_refptr<Bitmap> src_bitmap,
                 const base::Rect& src_rect,
                 int opacity) {
  CheckIsDisposed();

  if (src_rect.width <= 0 || src_rect.height <= 0)
    return;

  base::Rect rect = src_rect;
  if (rect.x + rect.width > src_bitmap->GetSize().x)
    rect.width = src_bitmap->GetSize().x - rect.x;

  if (rect.y + rect.height > src_bitmap->GetSize().y)
    rect.height = src_bitmap->GetSize().y - rect.y;

  rect.width = std::max(0, rect.width);
  rect.height = std::max(0, rect.height);

  StretchBlt(base::Rect(pos, rect.Size()), src_bitmap, rect, opacity);
}

void Bitmap::StretchBlt(const base::Rect& dest_rect,
                        scoped_refptr<Bitmap> src_bitmap,
                        const base::Rect& src_rect,
                        int opacity) {
  CheckIsDisposed();

  if (dest_rect.width <= 0 || dest_rect.height <= 0)
    return;
  if (src_rect.width <= 0 || src_rect.height <= 0)
    return;

  opacity = std::clamp(opacity, 0, 255);
  if (!IsObjectValid(src_bitmap.get()))
    return;

  base::Vec2i size = GetSize();
  base::Vec2i src_size = src_bitmap->GetSize();
  auto dst_tex = screen()->renderer()->MakeGenericFramebuffer(
      dest_rect.Size(), screen()->tex_format());

  renderer::CopyTexture(screen()->renderer()->context(), texture_, dest_rect,
                        dst_tex, base::Vec2i());

  /*
   * (texCoord - src_offset) * src_dst_factor
   */
  base::Vec4 offset_scale;
  offset_scale.x = static_cast<float>(src_rect.x) / src_size.x;
  offset_scale.y = static_cast<float>(src_rect.y) / src_size.y;
  offset_scale.z =
      (static_cast<float>(src_size.x) / src_rect.width) *
      (static_cast<float>(dest_rect.width) / dst_tex->GetDesc().Width);
  offset_scale.w =
      (static_cast<float>(src_size.y) / src_rect.height) *
      (static_cast<float>(dest_rect.height) / dst_tex->GetDesc().Height);

  auto* RTV = texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto& shader = screen()->renderer()->GetPipelines()->blt;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::NoBlend);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);

  auto* src_texture_view = src_bitmap->GetHandle()->GetDefaultView(
      Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  auto* dst_texture_view =
      dst_tex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
  shader.SetTexture(src_texture_view);
  shader.SetDstTexture(dst_texture_view);
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::Rect scissor;
  scissor.right = size.x;
  scissor.bottom = size.y;
  screen()->renderer()->context()->SetScissorRects(
      1, &scissor, 1, scissor.bottom + scissor.left);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Blt::VSUniform> Constants(
        screen()->renderer()->context(), shader.GetVSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, GetSize(),
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->texSize = base::MakeInvert(src_size);
    Constants->transOffset = base::Vec2i(0);
  }

  {
    Diligent::MapHelper<renderer::PipelineInstance_Blt::PSUniform> Constants(
        screen()->renderer()->context(), shader.GetPSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    Constants->offsetScale = offset_scale;
    Constants->opacity = opacity / 255.0f;
  }

  auto* quad = screen()->renderer()->common_quad();
  quad->SetPosition(dest_rect);
  quad->SetTexcoord(src_rect);
  quad->Draw(screen()->renderer()->context());

  screen()->renderer()->context()->Flush();

  NeedUpdateSurface();
}

void Bitmap::FillRect(const base::Rect& rect, scoped_refptr<Color> color) {
  CheckIsDisposed();

  if (rect.width <= 0 || rect.height <= 0)
    return;

  auto* RTV = texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto& shader = screen()->renderer()->GetPipelines()->color;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::NoBlend);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::Rect scissor;
  scissor.right = GetSize().x;
  scissor.bottom = GetSize().y;
  screen()->renderer()->context()->SetScissorRects(
      1, &scissor, 1, scissor.bottom + scissor.left);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Color::VSUniform> Constants(
        screen()->renderer()->context(), shader.GetVSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, GetSize(),
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->transOffset = base::Vec2i(0);
  }

  auto* quad = screen()->renderer()->common_quad();
  quad->SetPosition(rect);
  quad->SetColor(color->AsBase());
  quad->Draw(screen()->renderer()->context());

  screen()->renderer()->context()->Flush();

  NeedUpdateSurface();
}

void Bitmap::GradientFillRect(const base::Rect& rect,
                              scoped_refptr<Color> color1,
                              scoped_refptr<Color> color2,
                              bool vertical) {
  CheckIsDisposed();

  if (rect.width <= 0 || rect.height <= 0)
    return;

  auto* RTV = texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto& shader = screen()->renderer()->GetPipelines()->color;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::NoBlend);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::Rect scissor;
  scissor.right = GetSize().x;
  scissor.bottom = GetSize().y;
  screen()->renderer()->context()->SetScissorRects(
      1, &scissor, 1, scissor.bottom + scissor.left);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Color::VSUniform> Constants(
        screen()->renderer()->context(), shader.GetVSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, GetSize(),
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->transOffset = base::Vec2i(0);
  }

  auto* quad = screen()->renderer()->common_quad();
  quad->SetPosition(rect);
  if (vertical) {
    quad->SetColor(color1->AsBase(), 0);
    quad->SetColor(color1->AsBase(), 1);
    quad->SetColor(color2->AsBase(), 2);
    quad->SetColor(color2->AsBase(), 3);
  } else {
    quad->SetColor(color1->AsBase(), 0);
    quad->SetColor(color2->AsBase(), 1);
    quad->SetColor(color2->AsBase(), 2);
    quad->SetColor(color1->AsBase(), 3);
  }

  quad->Draw(screen()->renderer()->context());
  screen()->renderer()->context()->Flush();

  NeedUpdateSurface();
}

void Bitmap::Clear() {
  ClearRect(GetSize());
}

void Bitmap::ClearRect(const base::Rect& rect) {
  CheckIsDisposed();

  auto* RTV = texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
  screen()->renderer()->context()->SetRenderTargets(
      1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  auto& shader = screen()->renderer()->GetPipelines()->color;
  auto* pipeline = shader.GetPSOFor(renderer::BlendType::NoBlend);
  screen()->renderer()->context()->SetPipelineState(pipeline->pso);
  screen()->renderer()->context()->CommitShaderResources(
      pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::Rect scissor;
  scissor.right = GetSize().x;
  scissor.bottom = GetSize().y;
  screen()->renderer()->context()->SetScissorRects(
      1, &scissor, 1, scissor.bottom + scissor.left);

  {
    Diligent::MapHelper<renderer::PipelineInstance_Color::VSUniform> Constants(
        screen()->renderer()->context(), shader.GetVSUniform(),
        Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
    renderer::MakeProjectionMatrix(
        Constants->projMat, GetSize(),
        screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
    Constants->transOffset = base::Vec2i(0);
  }

  auto* quad = screen()->renderer()->common_quad();
  quad->SetPosition(rect);
  quad->SetColor(base::Vec4(0));
  quad->Draw(screen()->renderer()->context());

  screen()->renderer()->context()->Flush();

  NeedUpdateSurface();
}

scoped_refptr<Color> Bitmap::GetPixel(const base::Vec2i& pos) {
  CheckIsDisposed();

  if (pos.x < 0 || pos.x >= GetSize().x || pos.y < 0 || pos.y >= GetSize().y)
    return nullptr;

  SurfaceRequired();
  auto* pixel_detail = SDL_GetPixelFormatDetails(surface_buffer_->format);
  int bpp = pixel_detail->bytes_per_pixel;
  uint8_t* pixel = static_cast<uint8_t*>(surface_buffer_->pixels) +
                   static_cast<size_t>(pos.y) * surface_buffer_->pitch +
                   static_cast<size_t>(pos.x) * bpp;

  uint8_t color[4];
  SDL_GetRGBA(*reinterpret_cast<uint32_t*>(pixel), pixel_detail, nullptr,
              &color[0], &color[1], &color[2], &color[3]);

  return new Color(color[0], color[1], color[2], color[3]);
}

void Bitmap::SetPixel(const base::Vec2i& pos, scoped_refptr<Color> color) {
  CheckIsDisposed();

  if (pos.x < 0 || pos.x >= GetSize().x || pos.y < 0 || pos.y >= GetSize().y)
    return;

  SDL_Color color_u8 = color->AsSDLColor();
  Diligent::TextureSubResData TexSubData(&color_u8, sizeof(color_u8));

  Diligent::Box DstBox(pos.x, pos.x + 1, pos.y, pos.y + 1);
  screen()->renderer()->context()->UpdateTexture(
      texture_, 0, 0, DstBox, TexSubData,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  NeedUpdateSurface();
}

void Bitmap::HueChange(int hue) {
  CheckIsDisposed();

  if (hue % 360 == 0)
    return;

  while (hue < 0)
    hue += 359;
  hue %= 359;

  // TODO:

  NeedUpdateSurface();
}

void Bitmap::Blur() {
  CheckIsDisposed();

  // TODO:

  NeedUpdateSurface();
}

void Bitmap::RadialBlur(int angle, int division) {
  CheckIsDisposed();

  // TODO:

  NeedUpdateSurface();
}

void Bitmap::DrawText(const base::Rect& rect,
                      const std::string& str,
                      TextAlign align) {
  CheckIsDisposed();

  font_->EnsureLoadFont();
  uint8_t fopacity;
  auto* txt_surf = font_->RenderText(str, &fopacity);
  if (txt_surf) {
    // Update text cache
    if (!text_cache_ || text_cache_->GetDesc().Width < txt_surf->w ||
        text_cache_->GetDesc().Height < txt_surf->h) {
      Diligent::TextureDesc TexDesc = MakeTextureDesc(screen()->tex_format());
      TexDesc.Name = "Text cache texture";
      TexDesc.Width = std::max<uint32_t>(
          text_cache_ ? text_cache_->GetDesc().Width : 0, txt_surf->w);
      TexDesc.Height = std::max<uint32_t>(
          text_cache_ ? text_cache_->GetDesc().Height : 0, txt_surf->h);

      text_cache_.Release();
      screen()->renderer()->device()->CreateTexture(TexDesc, nullptr,
                                                    &text_cache_);
    }

    // Calculate position
    int align_x = rect.x, align_y = rect.y + (rect.height - txt_surf->h) / 2;
    switch (align) {
      default:
      case TextAlign::Left:
        break;
      case TextAlign::Center:
        align_x += (rect.width - txt_surf->w) / 2;
        break;
      case TextAlign::Right:
        align_x += rect.width - txt_surf->w;
        break;
    }

    float zoom_x = static_cast<float>(rect.width) / txt_surf->w;
    zoom_x = std::min(zoom_x, 1.0f);
    base::Rect pos(align_x, align_y, txt_surf->w * zoom_x, txt_surf->h);

    screen()->renderer()->context()->SetRenderTargets(
        0, nullptr, nullptr,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Upload text raster data
    Diligent::TextureSubResData TexSubData(txt_surf->pixels, txt_surf->pitch);
    Diligent::Box DstBox(0, txt_surf->w, 0, txt_surf->h);
    screen()->renderer()->context()->UpdateTexture(
        text_cache_, 0, 0, DstBox, TexSubData,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw text on target
    auto dst_tex = screen()->renderer()->MakeGenericFramebuffer(
        pos.Size(), screen()->tex_format());

    renderer::CopyTexture(screen()->renderer()->context(), texture_, pos,
                          dst_tex, base::Vec2i());

    base::Vec4 offset_scale(
        0, 0,
        static_cast<float>(text_cache_->GetDesc().Width * zoom_x) /
            dst_tex->GetDesc().Width,
        static_cast<float>(text_cache_->GetDesc().Height) /
            dst_tex->GetDesc().Height);

    auto* RTV = texture_->GetDefaultView(Diligent::TEXTURE_VIEW_RENDER_TARGET);
    screen()->renderer()->context()->SetRenderTargets(
        1, &RTV, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    auto& shader = screen()->renderer()->GetPipelines()->blt;
    auto* pipeline = shader.GetPSOFor(renderer::BlendType::NoBlend);
    screen()->renderer()->context()->SetPipelineState(pipeline->pso);

    auto* src_texture_view =
        text_cache_->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    auto* dst_texture_view =
        dst_tex->GetDefaultView(Diligent::TEXTURE_VIEW_SHADER_RESOURCE);
    shader.SetTexture(src_texture_view);
    shader.SetDstTexture(dst_texture_view);
    screen()->renderer()->context()->CommitShaderResources(
        pipeline->srb, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    Diligent::Rect scissor;
    scissor.right = GetSize().x;
    scissor.bottom = GetSize().y;
    screen()->renderer()->context()->SetScissorRects(
        1, &scissor, 1, scissor.bottom + scissor.left);

    {
      Diligent::MapHelper<renderer::PipelineInstance_Blt::VSUniform> Constants(
          screen()->renderer()->context(), shader.GetVSUniform(),
          Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      renderer::MakeProjectionMatrix(
          Constants->projMat, GetSize(),
          screen()->renderer()->device()->GetDeviceInfo().IsGLDevice());
      Constants->texSize = base::MakeInvert(base::Vec2i(
          text_cache_->GetDesc().Width, text_cache_->GetDesc().Height));
      Constants->transOffset = base::Vec2i(0);
    }

    {
      Diligent::MapHelper<renderer::PipelineInstance_Blt::PSUniform> Constants(
          screen()->renderer()->context(), shader.GetPSUniform(),
          Diligent::MAP_WRITE, Diligent::MAP_FLAG_DISCARD);
      Constants->offsetScale = offset_scale;
      Constants->opacity = fopacity / 255.0f;
    }

    auto* quad = screen()->renderer()->common_quad();
    quad->SetPosition(pos);
    quad->SetTexcoord(base::Vec2(txt_surf->w, txt_surf->h));
    quad->Draw(screen()->renderer()->context());

    screen()->renderer()->context()->Flush();
    SDL_DestroySurface(txt_surf);
  }

  NeedUpdateSurface();
}

scoped_refptr<Rect> Bitmap::TextSize(const std::string& str) {
  CheckIsDisposed();

  TTF_Font* font = font_->AsSDLFont();
  std::string src_text = font_->FixupString(str);

  int w, h;
  TTF_SizeUTF8(font, src_text.c_str(), &w, &h);

  const char* end_char = nullptr;
  uint16_t ucs2 = utf8_to_ucs2(str.c_str(), &end_char);
  if (font_->GetItalic() && *end_char == '\0')
    TTF_GlyphMetrics(font, ucs2, 0, 0, 0, 0, &w);

  return new Rect(base::Rect(0, 0, w, h));
}

scoped_refptr<Font> Bitmap::GetFont() const {
  CheckIsDisposed();
  return font_;
}

void Bitmap::SetFont(scoped_refptr<Font> font) {
  CheckIsDisposed();
  *font_ = *font;
}

SDL_Surface* Bitmap::SurfaceRequired() {
  CheckIsDisposed();

  if (surface_buffer_)
    return surface_buffer_;
  surface_buffer_ =
      SDL_CreateSurface(GetSize().x, GetSize().y, SDL_PIXELFORMAT_ABGR8888);

  if (!read_cache_) {
    Diligent::TextureDesc TexDesc = texture_->GetDesc();
    TexDesc.Name = "Bitmap read stage buffer";
    TexDesc.Usage = Diligent::USAGE_STAGING;
    TexDesc.BindFlags = Diligent::BIND_NONE;
    TexDesc.CPUAccessFlags = Diligent::CPU_ACCESS_READ;
    screen()->renderer()->device()->CreateTexture(TexDesc, nullptr,
                                                  &read_cache_);
  }

  screen()->renderer()->context()->SetRenderTargets(
      0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

  Diligent::CopyTextureAttribs CPTex(
      texture_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
      read_cache_, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  screen()->renderer()->context()->CopyTexture(CPTex);
  screen()->renderer()->context()->WaitForIdle();

  Diligent::MappedTextureSubresource MappedData;
  {
    screen()->renderer()->context()->MapTextureSubresource(
        read_cache_, 0, 0, Diligent::MAP_READ, Diligent::MAP_FLAG_DO_NOT_WAIT,
        nullptr, MappedData);

    uint8_t* dst_data = static_cast<uint8_t*>(surface_buffer_->pixels);
    uint8_t* src_data = static_cast<uint8_t*>(MappedData.pData);
    for (size_t i = 0; i < surface_buffer_->h; ++i) {
      memcpy(dst_data, src_data, surface_buffer_->pitch);
      dst_data += surface_buffer_->pitch;
      src_data += MappedData.Stride;
    }

    screen()->renderer()->context()->UnmapTextureSubresource(read_cache_, 0, 0);
  }

  return surface_buffer_;
}

void Bitmap::UpdateSurface() {
  CheckIsDisposed();

  if (surface_buffer_ && surface_buffer_->pixels) {
    Diligent::TextureSubResData TexSubData(surface_buffer_->pixels,
                                           surface_buffer_->pitch);
    Diligent::Box DstBox(0, surface_buffer_->w, 0, surface_buffer_->h);
    screen()->renderer()->context()->UpdateTexture(
        texture_, 0, 0, DstBox, TexSubData,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
        Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
  }

  NeedUpdateSurface();
}

void Bitmap::OnObjectDisposed() {
  observers_.Notify();

  if (surface_buffer_) {
    SDL_DestroySurface(surface_buffer_);
    surface_buffer_ = nullptr;
  }

  texture_.Release();
  read_cache_.Release();
}

void Bitmap::NeedUpdateSurface() {
  observers_.Notify();

  if (surface_buffer_) {
    SDL_DestroySurface(surface_buffer_);
    surface_buffer_ = nullptr;
  }
}

}  // namespace content

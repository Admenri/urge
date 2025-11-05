// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/font_impl.h"

#include <map>

#include "content/context/execution_context.h"

namespace content {

constexpr int32_t kOutlineSize = 1;
constexpr float kFontRealScale = 0.9f;

namespace {

void RenderShadowSurface(SDL_Surface*& in, const SDL_Color& color) {
  auto* dest = SDL_CreateSurface(in->w, in->h, in->format);

  SDL_Rect dest_rect{1, 1, 0, 0};
  SDL_SetSurfaceBlendMode(dest, SDL_BLENDMODE_NONE);
  SDL_BlitSurface(in, nullptr, dest, &dest_rect);

  auto* pixels = static_cast<uint32_t*>(dest->pixels);
  auto pitch = dest->pitch / 4;
  for (int32_t y = 0; y < dest->h; ++y)
    for (int32_t x = 0; x < dest->w; ++x)
      pixels[x + y * pitch] &= 0xFF000000;

  SDL_SetSurfaceBlendMode(dest, SDL_BLENDMODE_BLEND);
  SDL_BlitSurface(in, nullptr, dest, nullptr);

  SDL_DestroySurface(in);
  in = dest;
}

TTF_Font* ReadFontFromMemory(
    const std::map<std::string, std::pair<int64_t, void*>>& mem_fonts,
    const std::string& path,
    int32_t size) {
  auto it = mem_fonts.find(path);
  if (it != mem_fonts.end()) {
    SDL_IOStream* io = SDL_IOFromConstMem(it->second.second, it->second.first);
    return TTF_OpenFontIO(io, true, size * kFontRealScale);
  }

  return nullptr;
}

}  // namespace

// static
scoped_refptr<Font> Font::New(ExecutionContext* execution_context,
                              const std::string& name,
                              uint32_t size,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<FontImpl>(name, size,
                                        execution_context->font_context);
}

// static
scoped_refptr<Font> Font::Copy(ExecutionContext* execution_context,
                               scoped_refptr<Font> other,
                               ExceptionState& exception_state) {
  return base::MakeRefCounted<FontImpl>(*static_cast<FontImpl*>(other.get()));
}

// static
bool Font::IsExisted(ExecutionContext* execution_context,
                     const std::string& name,
                     ExceptionState& exception_state) {
  return execution_context->font_context->IsFontExisted(name);
}

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultName,
    std::vector<std::string>,
    { return execution_context->font_context->default_name; },
    { execution_context->font_context->default_name = value; });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultSize,
    uint32_t,
    { return execution_context->font_context->default_size; },
    {
      execution_context->font_context->default_size =
          std::max<int32_t>(6, value);
    });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultBold,
    bool,
    { return execution_context->font_context->default_bold; },
    { execution_context->font_context->default_bold = value; });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultItalic,
    bool,
    { return execution_context->font_context->default_italic; },
    { execution_context->font_context->default_italic = value; });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultShadow,
    bool,
    { return execution_context->font_context->default_shadow; },
    { execution_context->font_context->default_shadow = value; });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultOutline,
    bool,
    { return execution_context->font_context->default_outline; },
    { execution_context->font_context->default_outline = value; });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultSolid,
    bool,
    { return execution_context->font_context->default_solid; },
    { execution_context->font_context->default_solid = value; });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultColor,
    scoped_refptr<Color>,
    { return execution_context->font_context->default_color; },
    {
      CHECK_ATTRIBUTE_VALUE;
      *execution_context->font_context->default_color = *ColorImpl::From(value);
    });

URGE_DEFINE_STATIC_ATTRIBUTE(
    Font,
    DefaultOutColor,
    scoped_refptr<Color>,
    { return execution_context->font_context->default_out_color; },
    {
      CHECK_ATTRIBUTE_VALUE;
      *execution_context->font_context->default_out_color =
          *ColorImpl::From(value);
    });

///////////////////////////////////////////////////////////////////////////////
// Font Implement

FontImpl::FontImpl(ScopedFontData* parent)
    : name_(parent->default_name),
      size_(parent->default_size),
      bold_(parent->default_bold),
      italic_(parent->default_italic),
      outline_(parent->default_outline),
      shadow_(parent->default_shadow),
      solid_(parent->default_solid),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      out_color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      parent_(parent),
      font_(nullptr) {
  *color_ = *parent->default_color;
  *out_color_ = *parent->default_out_color;
}

FontImpl::FontImpl(const std::string& name,
                   uint32_t size,
                   ScopedFontData* parent)
    : name_({name}),
      size_(size),
      bold_(parent->default_bold),
      italic_(parent->default_italic),
      outline_(parent->default_outline),
      shadow_(parent->default_shadow),
      solid_(parent->default_solid),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      out_color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      parent_(parent),
      font_(nullptr) {
  *color_ = *parent->default_color;
  *out_color_ = *parent->default_out_color;
}

FontImpl::FontImpl(const FontImpl& other)
    : name_(other.name_),
      size_(other.size_),
      bold_(other.bold_),
      italic_(other.italic_),
      outline_(other.outline_),
      shadow_(other.shadow_),
      solid_(other.solid_),
      color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      out_color_(base::MakeRefCounted<ColorImpl>(base::Vec4())),
      parent_(other.parent_),
      font_(nullptr) {
  *color_ = *other.color_;
  *out_color_ = *other.out_color_;
}

FontImpl& FontImpl::operator=(const FontImpl& other) {
  name_ = other.name_;
  size_ = other.size_;
  bold_ = other.bold_;
  italic_ = other.italic_;
  outline_ = other.outline_;
  shadow_ = other.shadow_;
  solid_ = other.solid_;
  *color_ = *other.color_;
  *out_color_ = *other.out_color_;
  font_ = nullptr;
  return *this;
}

scoped_refptr<FontImpl> FontImpl::From(scoped_refptr<Font> host) {
  return static_cast<FontImpl*>(host.get());
}

TTF_Font* FontImpl::GetCanonicalFont(ExceptionState& exception_state) {
  if (!font_) {
    LoadFontInternal(exception_state);
    if (exception_state.HadException())
      return nullptr;

    // Setup font rendering fallback
    SetupFontFallbackInternal();
  }

  int32_t font_style = TTF_STYLE_NORMAL;
  if (bold_)
    font_style |= TTF_STYLE_BOLD;
  if (italic_)
    font_style |= TTF_STYLE_ITALIC;
  TTF_SetFontStyle(font_, font_style);

  return font_;
}

SDL_Surface* FontImpl::RenderText(const std::string& text,
                                  uint8_t* font_opacity,
                                  ExceptionState& exception_state) {
  TTF_Font* font = GetCanonicalFont(exception_state);
  if (!font)
    return nullptr;

  auto color_impl = ColorImpl::From(color_);
  auto out_color_impl = ColorImpl::From(out_color_);

  SDL_Color font_color = color_impl->AsSDLColor();
  SDL_Color outline_color = out_color_impl->AsSDLColor();
  if (font_opacity)
    *font_opacity = font_color.a;

  font_color.a = 255;
  outline_color.a = 255;

  SDL_Surface* text_surface =
      solid_
          ? TTF_RenderText_Solid(font, text.c_str(), text.size(), font_color)
          : TTF_RenderText_Blended(font, text.c_str(), text.size(), font_color);
  if (!text_surface)
    return nullptr;

  if (outline_) {
    // Outline shape render
    SDL_Surface* outline_surface = nullptr;
    TTF_SetFontOutline(font, kOutlineSize);
    outline_surface = solid_
                          ? TTF_RenderText_Solid(font, text.c_str(),
                                                 text.size(), outline_color)
                          : TTF_RenderText_Blended(font, text.c_str(),
                                                   text.size(), outline_color);
    TTF_SetFontOutline(font, 0);

    if (!outline_surface) {
      SDL_DestroySurface(text_surface);
      return nullptr;
    }

    // Make outline text surface
    SDL_Rect out_rect = {
        kOutlineSize,
        kOutlineSize,
        text_surface->w,
        text_surface->h,
    };
    SDL_SetSurfaceBlendMode(text_surface, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(text_surface, nullptr, outline_surface, &out_rect);

    SDL_DestroySurface(text_surface);
    text_surface = outline_surface;
  }

  EnsureFontSurfaceFormatInternal(text_surface);

  if (shadow_)
    RenderShadowSurface(text_surface, font_color);

  return text_surface;
}

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Name,
    std::vector<std::string>,
    FontImpl,
    { return name_; },
    {
      name_ = value;
      font_ = nullptr;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Size,
    uint32_t,
    FontImpl,
    { return size_; },
    {
      size_ = std::max<int32_t>(6, value);
      font_ = nullptr;
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Bold,
    bool,
    FontImpl,
    { return bold_; },
    { bold_ = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Italic,
    bool,
    FontImpl,
    { return italic_; },
    { italic_ = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Outline,
    bool,
    FontImpl,
    { return outline_; },
    { outline_ = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Shadow,
    bool,
    FontImpl,
    { return shadow_; },
    { shadow_ = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Solid,
    bool,
    FontImpl,
    { return solid_; },
    { solid_ = value; });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    Color,
    scoped_refptr<Color>,
    FontImpl,
    { return color_; },
    {
      CHECK_ATTRIBUTE_VALUE;
      *color_ = *ColorImpl::From(value);
    });

URGE_DEFINE_OVERRIDE_ATTRIBUTE(
    OutColor,
    scoped_refptr<Color>,
    FontImpl,
    { return out_color_; },
    {
      CHECK_ATTRIBUTE_VALUE;
      *out_color_ = *ColorImpl::From(value);
    });

void FontImpl::LoadFontInternal(ExceptionState& exception_state) {
  std::vector<std::string> load_names(name_);
  load_names.push_back(parent_->default_font);

  auto& font_cache = parent_->font_cache;
  if (size_ >= 6 && size_ <= 96) {
    for (const auto& font_name : load_names) {
      // Find existed font object
      auto it = font_cache.find(std::make_pair(font_name, size_));
      if (it != font_cache.end()) {
        font_ = it->second;
        return;
      }

      // Load new font object
      TTF_Font* font_obj =
          ReadFontFromMemory(parent_->data_cache, font_name, size_);
      if (font_obj) {
        // Storage new font object
        font_ = font_obj;
        font_cache.emplace(std::make_pair(font_name, size_), font_);
        return;
      }
    }
  }

  // Failed to load font
  std::string font_names;
  for (auto& it : name_)
    font_names = font_names + it + " ";

  // Throw font not find error
  exception_state.ThrowError(ExceptionCode::CONTENT_ERROR,
                             "failed to load font: %s", font_names.c_str());
}

void FontImpl::EnsureFontSurfaceFormatInternal(SDL_Surface*& surf) {
  SDL_Surface* format_surf = nullptr;
  if (surf->format != SDL_PIXELFORMAT_ABGR8888) {
    format_surf = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(surf);
    surf = format_surf;
  }
}

void FontImpl::SetupFontFallbackInternal() {
  TTF_ClearFallbackFonts(font_);
  auto& font_cache = parent_->font_cache;
  auto it = font_cache.find(std::make_pair(parent_->default_font, size_));
  if (it != font_cache.end()) {
    if (it->second == font_)
      return;

    // Add fallback option
    TTF_AddFallbackFont(font_, it->second);
  } else {
    // Create new size font object
    TTF_Font* font_obj =
        ReadFontFromMemory(parent_->data_cache, parent_->default_font, size_);
    if (font_obj) {
      TTF_AddFallbackFont(font_, font_obj);
      font_cache.emplace(std::make_pair(parent_->default_font, size_),
                         font_obj);
    }
  }
}

}  // namespace content

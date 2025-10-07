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
  // TODO:
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

scoped_refptr<Font> Font::New(ExecutionContext* execution_context,
                              const std::string& name,
                              uint32_t size,
                              ExceptionState& exception_state) {
  return base::MakeRefCounted<FontImpl>(name, size,
                                        execution_context->font_context);
}

scoped_refptr<Font> Font::Copy(ExecutionContext* execution_context,
                               scoped_refptr<Font> other,
                               ExceptionState& exception_state) {
  return base::MakeRefCounted<FontImpl>(*static_cast<FontImpl*>(other.get()));
}

bool Font::IsExisted(ExecutionContext* execution_context,
                     const std::string& name,
                     ExceptionState& exception_state) {
  return execution_context->font_context->IsFontExisted(name);
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font,
                                   DefaultName,
                                   std::vector<std::string>) {
  return execution_context->font_context->default_name;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font,
                                    DefaultName,
                                    std::vector<std::string>) {
  execution_context->font_context->default_name = value;
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultSize, uint32_t) {
  return execution_context->font_context->default_size;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultSize, uint32_t) {
  execution_context->font_context->default_size = std::max<int32_t>(6, value);
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultBold, bool) {
  return execution_context->font_context->default_bold;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultBold, bool) {
  execution_context->font_context->default_bold = value;
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultItalic, bool) {
  return execution_context->font_context->default_italic;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultItalic, bool) {
  execution_context->font_context->default_italic = value;
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultShadow, bool) {
  return execution_context->font_context->default_shadow;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultShadow, bool) {
  execution_context->font_context->default_shadow = value;
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultOutline, bool) {
  return execution_context->font_context->default_outline;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultOutline, bool) {
  execution_context->font_context->default_outline = value;
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultSolid, bool) {
  return execution_context->font_context->default_solid;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultSolid, bool) {
  execution_context->font_context->default_solid = value;
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font, DefaultColor, scoped_refptr<Color>) {
  return execution_context->font_context->default_color;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font, DefaultColor, scoped_refptr<Color>) {
  CHECK_ATTRIBUTE_VALUE;
  *execution_context->font_context->default_color = *ColorImpl::From(value);
}

URGE_DECLARE_STATIC_ATTRIBUTE_READ(Font,
                                   DefaultOutColor,
                                   scoped_refptr<Color>) {
  return execution_context->font_context->default_out_color;
}

URGE_DECLARE_STATIC_ATTRIBUTE_WRITE(Font,
                                    DefaultOutColor,
                                    scoped_refptr<Color>) {
  CHECK_ATTRIBUTE_VALUE;
  *execution_context->font_context->default_out_color = *ColorImpl::From(value);
}

std::vector<std::string> FontImpl::Get_Name(ExceptionState& exception_state) {
  return name_;
}

void FontImpl::Put_Name(const std::vector<std::string>& value,
                        ExceptionState& exception_state) {
  name_ = value;
  font_ = nullptr;
}

uint32_t FontImpl::Get_Size(ExceptionState& exception_state) {
  return size_;
}

void FontImpl::Put_Size(const uint32_t& value,
                        ExceptionState& exception_state) {
  size_ = std::max<int32_t>(6, value);
  font_ = nullptr;
}

bool FontImpl::Get_Bold(ExceptionState& exception_state) {
  return bold_;
}

void FontImpl::Put_Bold(const bool& value, ExceptionState& exception_state) {
  bold_ = value;
}

bool FontImpl::Get_Italic(ExceptionState& exception_state) {
  return italic_;
}

void FontImpl::Put_Italic(const bool& value, ExceptionState& exception_state) {
  italic_ = value;
}

bool FontImpl::Get_Outline(ExceptionState& exception_state) {
  return outline_;
}

void FontImpl::Put_Outline(const bool& value, ExceptionState& exception_state) {
  outline_ = value;
}

bool FontImpl::Get_Shadow(ExceptionState& exception_state) {
  return shadow_;
}

void FontImpl::Put_Shadow(const bool& value, ExceptionState& exception_state) {
  shadow_ = value;
}

bool FontImpl::Get_Solid(ExceptionState& exception_state) {
  return solid_;
}

void FontImpl::Put_Solid(const bool& value, ExceptionState& exception_state) {
  solid_ = value;
}

scoped_refptr<Color> FontImpl::Get_Color(ExceptionState& exception_state) {
  return color_;
}

void FontImpl::Put_Color(const scoped_refptr<Color>& value,
                         ExceptionState& exception_state) {
  CHECK_ATTRIBUTE_VALUE;
  *color_ = *ColorImpl::From(value);
}

scoped_refptr<Color> FontImpl::Get_OutColor(ExceptionState& exception_state) {
  return out_color_;
}

void FontImpl::Put_OutColor(const scoped_refptr<Color>& value,
                            ExceptionState& exception_state) {
  CHECK_ATTRIBUTE_VALUE;
  *out_color_ = *ColorImpl::From(value);
}

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
  ExceptionState exception_state;
  color_->Set(parent->default_color, exception_state);
  out_color_->Set(parent->default_out_color, exception_state);
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
  ExceptionState exception_state;
  color_->Set(parent->default_color, exception_state);
  out_color_->Set(parent->default_out_color, exception_state);
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
  ExceptionState exception_state;
  color_->Set(other.color_, exception_state);
  out_color_->Set(other.out_color_, exception_state);
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
  if (!font_)
    LoadFontInternal(exception_state);

  if (exception_state.HadException())
    return nullptr;

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
  // Get font from current attribute
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

  SDL_Surface* raw_surf;
  if (solid_)
    raw_surf =
        TTF_RenderText_Solid(font, text.c_str(), text.size(), font_color);
  else
    raw_surf =
        TTF_RenderText_Blended(font, text.c_str(), text.size(), font_color);
  if (!raw_surf)
    return nullptr;

  EnsureFontSurfaceFormatInternal(raw_surf);

  if (shadow_)
    RenderShadowSurface(raw_surf, font_color);

  if (outline_) {
    SDL_Surface* outline = nullptr;
    TTF_SetFontOutline(font, kOutlineSize);
    if (solid_)
      outline =
          TTF_RenderText_Solid(font, text.c_str(), text.size(), outline_color);
    else
      outline = TTF_RenderText_Blended(font, text.c_str(), text.size(),
                                       outline_color);
    EnsureFontSurfaceFormatInternal(outline);
    SDL_Rect outRect = {kOutlineSize, kOutlineSize, raw_surf->w, raw_surf->h};
    SDL_SetSurfaceBlendMode(raw_surf, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(raw_surf, NULL, outline, &outRect);
    SDL_DestroySurface(raw_surf);
    raw_surf = outline;
    TTF_SetFontOutline(font, 0);
  }

  return raw_surf;
}

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
        // Storage new font obj
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

}  // namespace content

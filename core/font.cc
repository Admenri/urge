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

#include "core/font.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "SDL3_ttf/SDL_ttf.h"

#include "core/filesystem.h"
#include "core/font_context.h"

namespace urge {

namespace {

constexpr int32_t kOutlineSize = 1;

constexpr SDL_PixelFormat kInternalPixelFormat = SDL_PIXELFORMAT_ABGR8888;

SDL_Color ToSDLColor(const RefPtr<Color>& color) {
  if (!color)
    return SDL_Color{255, 255, 255, 255};

  return SDL_Color{
      static_cast<uint8_t>(std::clamp(color->data.r, 0.0f, 255.0f)),
      static_cast<uint8_t>(std::clamp(color->data.g, 0.0f, 255.0f)),
      static_cast<uint8_t>(std::clamp(color->data.b, 0.0f, 255.0f)),
      static_cast<uint8_t>(std::clamp(color->data.a, 0.0f, 255.0f)),
  };
}

void ConvertSurfaceFormat(SDL_Surface*& surface) {
  if (surface->format == kInternalPixelFormat)
    return;

  SDL_Surface* converted = SDL_ConvertSurface(surface, kInternalPixelFormat);
  SDL_DestroySurface(surface);
  surface = converted;
}

void RenderShadowSurface(SDL_Surface*& surface) {
  if (surface->w < 4 || surface->h < 4)
    return;

  SDL_Surface* shadow =
      SDL_CreateSurface(surface->w, surface->h, surface->format);
  if (!shadow)
    return;

  SDL_Rect dest_rect{1, 1, 0, 0};
  SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_NONE);
  SDL_BlitSurface(surface, nullptr, shadow, &dest_rect);

  auto* pixels = static_cast<uint32_t*>(shadow->pixels);
  const int32_t pitch = shadow->pitch / 4;
  for (int32_t y = 0; y < shadow->h; ++y)
    for (int32_t x = 0; x < shadow->w; ++x)
      pixels[x + y * pitch] &= 0xFF000000;

  SDL_SetSurfaceBlendMode(shadow, SDL_BLENDMODE_BLEND);
  SDL_BlitSurface(surface, nullptr, shadow, nullptr);

  SDL_DestroySurface(surface);
  surface = shadow;
}

}  // namespace

Font::Font(std::vector<std::string> names, int32_t size)
    : name_(names),
      size_(size),

      bold_(*Attr_DefaultBold()),
      italic_(*Attr_DefaultItalic()),
      outline_(*Attr_DefaultOutline()),
      shadow_(*Attr_DefaultShadow()),
      solid_(*Attr_DefaultSolid()),
      color_(*Attr_DefaultColor()),
      out_color_(*Attr_DefaultOutColor()),
      gradient_color_(*Attr_DefaultGradientColor()) {
  if (name_.empty()) {
    auto default_name = Attr_DefaultName();
    if (default_name.has_value() && !default_name->empty())
      name_ = *default_name;
  }
}

Font::Font(RefPtr<Font> other)
    : name_(other->name_),
      size_(other->size_),
      bold_(other->bold_),
      italic_(other->italic_),
      outline_(other->outline_),
      shadow_(other->shadow_),
      solid_(other->solid_),
      color_(other->color_),
      out_color_(other->out_color_),
      gradient_color_(other->gradient_color_) {}

bool Font::Existed(std::string name) {
  return FontContext::Get().FontExists(name);
}

ATTR_DEF(Font, std::vector<std::string>, Name) {
  if (value.has_value()) {
    name_ = *value;
    return std::nullopt;
  } else {
    return name_;
  }
}

ATTR_DEF(Font, int32_t, Size) {
  if (value.has_value()) {
    size_ = *value;
    return std::nullopt;
  } else {
    return size_;
  }
}

ATTR_DEF(Font, bool, Bold) {
  if (value.has_value()) {
    bold_ = *value;
    return std::nullopt;
  } else {
    return bold_;
  }
}

ATTR_DEF(Font, bool, Italic) {
  if (value.has_value()) {
    italic_ = *value;
    return std::nullopt;
  } else {
    return italic_;
  }
}

ATTR_DEF(Font, bool, Outline) {
  if (value.has_value()) {
    outline_ = *value;
    return std::nullopt;
  } else {
    return outline_;
  }
}

ATTR_DEF(Font, bool, Shadow) {
  if (value.has_value()) {
    shadow_ = *value;
    return std::nullopt;
  } else {
    return shadow_;
  }
}

ATTR_DEF(Font, bool, Solid) {
  if (value.has_value()) {
    solid_ = *value;
    return std::nullopt;
  } else {
    return solid_;
  }
}

ATTR_DEF(Font, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_ = *value;
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Font, RefPtr<Color>, OutColor) {
  if (value.has_value()) {
    out_color_ = *value;
    return std::nullopt;
  } else {
    return out_color_;
  }
}

ATTR_DEF(Font, RefPtr<Color>, GradientColor) {
  if (value.has_value()) {
    gradient_color_ = *value;
    return std::nullopt;
  } else {
    return gradient_color_;
  }
}

ATTR_DEF(Font, std::vector<std::string>, DefaultName) {
  static std::vector<std::string> default_names = {"Default.ttf"};
  if (value.has_value()) {
    default_names = *value;
    return std::nullopt;
  } else {
    return default_names;
  }
}

ATTR_DEF(Font, int32_t, DefaultSize) {
  static int32_t default_size = 24;
  if (value.has_value()) {
    default_size = *value;
    return std::nullopt;
  } else {
    return default_size;
  }
}

ATTR_DEF(Font, bool, DefaultBold) {
  static bool default_bold = false;
  if (value.has_value()) {
    default_bold = *value;
    return std::nullopt;
  } else {
    return default_bold;
  }
}

ATTR_DEF(Font, bool, DefaultItalic) {
  static bool default_italic = false;
  if (value.has_value()) {
    default_italic = *value;
    return std::nullopt;
  } else {
    return default_italic;
  }
}

ATTR_DEF(Font, bool, DefaultOutline) {
  static bool default_outline = true;
  if (value.has_value()) {
    default_outline = *value;
    return std::nullopt;
  } else {
    return default_outline;
  }
}

ATTR_DEF(Font, bool, DefaultShadow) {
  static bool default_shadow = false;
  if (value.has_value()) {
    default_shadow = *value;
    return std::nullopt;
  } else {
    return default_shadow;
  }
}

ATTR_DEF(Font, bool, DefaultSolid) {
  static bool default_solid = false;
  if (value.has_value()) {
    default_solid = *value;
    return std::nullopt;
  } else {
    return default_solid;
  }
}

ATTR_DEF(Font, RefPtr<Color>, DefaultColor) {
  static RefPtr<Color> default_color =
      MakeRefCounted<Color>(255.0f, 255.0f, 255.0f, 255.0f);
  if (value.has_value()) {
    default_color = *value;
    return std::nullopt;
  } else {
    return default_color;
  }
}

ATTR_DEF(Font, RefPtr<Color>, DefaultOutColor) {
  static RefPtr<Color> default_out_color =
      MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 128.0f);
  if (value.has_value()) {
    default_out_color = *value;
    return std::nullopt;
  } else {
    return default_out_color;
  }
}

ATTR_DEF(Font, RefPtr<Color>, DefaultGradientColor) {
  static RefPtr<Color> default_gradient_color =
      MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 0.0f);
  if (value.has_value()) {
    default_gradient_color = *value;
    return std::nullopt;
  } else {
    return default_gradient_color;
  }
}

TTF_Font* Font::ttf_font() {
  TTF_Font* font = FontContext::Get().AcquireFont(name_, size_);
  if (!font)
    return nullptr;

  int32_t style = TTF_STYLE_NORMAL;
  if (bold_)
    style |= TTF_STYLE_BOLD;
  if (italic_)
    style |= TTF_STYLE_ITALIC;
  TTF_SetFontStyle(font, style);

  return font;
}

SDL_Surface* Font::RenderText(const std::string& text, uint8_t* font_opacity) {
  TTF_Font* font = ttf_font();
  if (!font)
    return nullptr;

  const SDL_Color text_color = ToSDLColor(color_);
  const SDL_Color outline_color = ToSDLColor(out_color_);

  if (font_opacity)
    *font_opacity = text_color.a;

  SDL_Color render_color = text_color;
  render_color.a = 255;
  SDL_Color render_outline_color = outline_color;
  render_outline_color.a = 255;

  SDL_Surface* surface =
      solid_ ? TTF_RenderText_Solid(font, text.c_str(), text.size(), render_color)
             : TTF_RenderText_Blended(font, text.c_str(), text.size(),
                                      render_color);
  if (!surface)
    return nullptr;

  ConvertSurfaceFormat(surface);

  const SDL_Color gradient_top = ToSDLColor(color_);
  const SDL_Color gradient_bottom = ToSDLColor(gradient_color_);
  if (gradient_bottom.a &&
      (gradient_top.r != gradient_bottom.r ||
       gradient_top.g != gradient_bottom.g ||
       gradient_top.b != gradient_bottom.b)) {
    auto* pixels = static_cast<uint32_t*>(surface->pixels);
    const int32_t pitch = surface->pitch / 4;
    const auto* details = SDL_GetPixelFormatDetails(surface->format);
    const float gradient_alpha = gradient_bottom.a / 255.0f;

    for (int32_t y = 0; y < surface->h; ++y) {
      for (int32_t x = 0; x < surface->w; ++x) {
        uint8_t r, g, b, a;
        SDL_GetRGBA(pixels[x + y * pitch], details, nullptr, &r, &g, &b, &a);
        if (!a)
          continue;

        const float progress =
            (static_cast<float>(y) / surface->h) * gradient_alpha;
        r = static_cast<uint8_t>(gradient_bottom.r * progress +
                                 gradient_top.r * (1.0f - progress));
        g = static_cast<uint8_t>(gradient_bottom.g * progress +
                                 gradient_top.g * (1.0f - progress));
        b = static_cast<uint8_t>(gradient_bottom.b * progress +
                                 gradient_top.b * (1.0f - progress));

        pixels[x + y * pitch] = SDL_MapRGBA(details, nullptr, r, g, b, a);
      }
    }
  }

  if (outline_) {
    TTF_SetFontOutline(font, kOutlineSize);
    SDL_Surface* outline_surface = solid_
                                       ? TTF_RenderText_Solid(font, text.c_str(),
                                                              text.size(),
                                                              render_outline_color)
                                       : TTF_RenderText_Blended(
                                             font, text.c_str(), text.size(),
                                             render_outline_color);
    TTF_SetFontOutline(font, 0);

    if (!outline_surface) {
      SDL_DestroySurface(surface);
      return nullptr;
    }

    const SDL_Rect text_rect{
        kOutlineSize,
        kOutlineSize,
        surface->w,
        surface->h,
    };
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(surface, nullptr, outline_surface, &text_rect);

    SDL_DestroySurface(surface);
    surface = outline_surface;
  }

  ConvertSurfaceFormat(surface);

  if (shadow_)
    RenderShadowSurface(surface);

  return surface;
}

bool Font::MeasureText(const std::string& text, int32_t* width, int32_t* height) {
  TTF_Font* font = ttf_font();
  if (!font)
    return false;

  int32_t measured_width = 0, measured_height = 0;
  if (!TTF_GetStringSize(font, text.c_str(), text.size(), &measured_width,
                         &measured_height))
    return false;

  if (width)
    *width = measured_width;
  if (height)
    *height = measured_height;

  return true;
}

}  // namespace urge

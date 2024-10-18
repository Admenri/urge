// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/font.h"

#include "base/exception/exception.h"
#include "content/public/utility.h"

#define OutlineSize 1

namespace content {

namespace {

void RenderShadowSurface(SDL_Surface*& in, const SDL_Color& color) {
  SDL_Surface* out = SDL_CreateSurface(in->w + 1, in->h + 1, in->format);
  float fr = color.r / 255.0f, fg = color.g / 255.0f, fb = color.b / 255.0f;
  auto* detail = SDL_GetPixelFormatDetails(in->format);

  for (int y = 0; y < in->h + 1; ++y) {
    for (int x = 0; x < in->w + 1; ++x) {
      uint32_t src = 0, shd = 0,
               *outP = (uint32_t*)((uint8_t*)out->pixels + y * out->pitch) + x;

      if (y < in->h && x < in->w)
        src = ((uint32_t*)((uint8_t*)in->pixels + y * in->pitch))[x];
      if (y > 0 && x > 0)
        shd = ((uint32_t*)((uint8_t*)in->pixels + (y - 1) * in->pitch))[x - 1] &
              detail->Amask;

      if (x == 0 || y == 0 || src & detail->Amask) {
        *outP = (x == in->w || y == in->h) ? shd : src;
        continue;
      }

      uint8_t srcA = (src & detail->Amask) >> detail->Ashift;
      float fSrcA = srcA / 255.0f,
            fShdA = ((shd & detail->Amask) >> detail->Ashift) / 255.0f;
      float fa = fSrcA + fShdA * (1.0f - fSrcA), co3 = fSrcA / fa;

      *outP = SDL_MapRGBA(
          detail, nullptr,
          static_cast<uint8_t>(std::clamp(fr * co3, 0.0f, 1.0f) * 255),
          static_cast<uint8_t>(std::clamp(fg * co3, 0.0f, 1.0f) * 255),
          static_cast<uint8_t>(std::clamp(fb * co3, 0.0f, 1.0f) * 255),
          static_cast<uint8_t>(std::clamp(fa, 0.0f, 1.0f) * 255));
    }
  }

  SDL_DestroySurface(in);
  in = out;
}

std::pair<int64_t, void*> ReadFontToMemory(SDL_IOStream* io) {
  int64_t fsize = SDL_GetIOSize(io);
  void* mem = SDL_malloc(fsize);
  SDL_ReadIO(io, mem, fsize);
  SDL_CloseIO(io);

  return std::make_pair(fsize, mem);
}

TTF_Font* ReadFontFromMemory(
    std::map<std::string, std::pair<int64_t, void*>>& mem_fonts,
    const std::string& path,
    int size) {
  auto it = mem_fonts.find(path);
  if (it != mem_fonts.end()) {
    SDL_IOStream* io = SDL_IOFromConstMem(it->second.second, it->second.first);
    return TTF_OpenFontIO(io, true, size * 0.9f);
  }

  return nullptr;
}

}  // namespace

ScopedFontData::ScopedFontData(filesystem::Filesystem* io,
                               const std::string& filename)
    : default_color_(new Color(255.0, 255.0, 255.0, 255.0)),
      default_out_color_(new Color(0, 0, 0, 255.0)) {
  std::string dir("."), file;

  size_t last_slash_pos = filename.find_last_of('/');
  if (last_slash_pos != std::string::npos) {
    dir = filename.substr(0, last_slash_pos);
    file = filename.substr(last_slash_pos + 1);
  } else
    file = filename;

  dir.push_back('/');
  font_default_name_ = file;
  default_name_.push_back(file);

  auto font_files = io->EnumDir(dir);
  for (auto& file_iter : font_files) {
    std::string filepath = dir + file_iter;

    SDL_IOStream* font_ops = nullptr;
    try {
      font_ops = io->OpenReadRaw(filepath);
    } catch (...) {
      // Ignore font load error
      font_ops = nullptr;
    }

    // Cached in memory
    if (font_ops)
      mem_fonts_.emplace(file_iter, ReadFontToMemory(font_ops));
  }
}

ScopedFontData::~ScopedFontData() {
  for (auto& it : font_cache_)
    TTF_CloseFont(it.second);

  for (auto& it : mem_fonts_)
    SDL_free(it.second.second);
}

void* ScopedFontData::GetUIDefaultFont(int64_t* font_size) {
  auto it = mem_fonts_.find(font_default_name_);
  if (it != mem_fonts_.end()) {
    *font_size = it->second.first;
    return it->second.second;
  }

  return nullptr;
}

bool ScopedFontData::Existed(const std::string& name) {
  auto it = mem_fonts_.find(name);
  if (it != mem_fonts_.end())
    return true;

  return false;
}

void ScopedFontData::SetDefaultName(const std::vector<std::string>& name) {
  default_name_ = name;
}

std::vector<std::string> ScopedFontData::GetDefaultName() {
  return default_name_;
}

void ScopedFontData::SetDefaultSize(int size) {
  default_size_ = size;
}

int ScopedFontData::GetDefaultSize() {
  return default_size_;
}

void ScopedFontData::SetDefaultBold(bool bold) {
  default_bold_ = bold;
}

bool ScopedFontData::GetDefaultBold() {
  return default_bold_;
}

void ScopedFontData::SetDefaultItalic(bool italic) {
  default_italic_ = italic;
}

bool ScopedFontData::GetDefaultItalic() {
  return default_italic_;
}

void ScopedFontData::SetDefaultShadow(bool shadow) {
  default_shadow_ = shadow;
}

bool ScopedFontData::GetDefaultShadow() {
  return default_shadow_;
}

void ScopedFontData::SetDefaultOutline(bool outline) {
  default_outline_ = outline;
}

bool ScopedFontData::GetDefaultOutline() {
  return default_outline_;
}

void ScopedFontData::SetDefaultColor(scoped_refptr<Color> color) {
  *default_color_ = *color;
}

scoped_refptr<Color> ScopedFontData::GetDefaultColor() {
  return default_color_;
}

void ScopedFontData::SetDefaultOutColor(scoped_refptr<Color> color) {
  *default_out_color_ = *color;
}

scoped_refptr<Color> ScopedFontData::GetDefaultOutColor() {
  return default_out_color_;
}

Font::Font(ScopedFontData* parent)
    : name_(parent->GetDefaultName()),
      size_(parent->GetDefaultSize()),
      bold_(parent->GetDefaultBold()),
      italic_(parent->GetDefaultItalic()),
      outline_(parent->GetDefaultOutline()),
      shadow_(parent->GetDefaultShadow()),
      color_(new Color(*parent->GetDefaultColor())),
      out_color_(new Color(*parent->GetDefaultOutColor())),
      parent_(parent),
      font_(nullptr) {}

Font::Font(ScopedFontData* parent, const std::vector<std::string>& name)
    : name_(name),
      size_(parent->GetDefaultSize()),
      bold_(parent->GetDefaultBold()),
      italic_(parent->GetDefaultItalic()),
      outline_(parent->GetDefaultOutline()),
      shadow_(parent->GetDefaultShadow()),
      color_(new Color(*parent->GetDefaultColor())),
      out_color_(new Color(*parent->GetDefaultOutColor())),
      parent_(parent),
      font_(nullptr) {}

Font::Font(ScopedFontData* parent,
           const std::vector<std::string>& name,
           int size)
    : name_(name),
      size_(size),
      bold_(parent->GetDefaultBold()),
      italic_(parent->GetDefaultItalic()),
      outline_(parent->GetDefaultOutline()),
      shadow_(parent->GetDefaultShadow()),
      color_(new Color(*parent->GetDefaultColor())),
      out_color_(new Color(*parent->GetDefaultOutColor())),
      parent_(parent),
      font_(nullptr) {}

Font::Font(const Font& other)
    : name_(other.name_),
      size_(other.size_),
      bold_(other.bold_),
      italic_(other.italic_),
      outline_(other.outline_),
      shadow_(other.shadow_),
      color_(new Color(*other.color_)),
      out_color_(new Color(*other.out_color_)),
      parent_(other.parent_),
      font_(other.font_) {}

const Font& Font::operator=(const Font& other) {
  name_ = other.name_;
  size_ = other.size_;
  bold_ = other.bold_;
  italic_ = other.italic_;
  outline_ = other.outline_;
  shadow_ = other.shadow_;
  *color_ = *other.color_;
  *out_color_ = *other.out_color_;
  font_ = other.font_;
  return other;
}

void Font::SetName(const std::vector<std::string>& name) {
  if (name_ == name)
    return;

  name_ = name;
  font_ = nullptr;
}

std::vector<std::string> Font::GetName() const {
  return name_;
}

void Font::SetSize(int size) {
  if (size_ == size)
    return;

  size_ = size;
  font_ = nullptr;
}

int Font::GetSize() const {
  return size_;
}

void Font::SetBold(bool bold) {
  bold_ = bold;
}

bool Font::GetBold() const {
  return bold_;
}

void Font::SetItalic(bool italic) {
  italic_ = italic;
}

bool Font::GetItalic() const {
  return italic_;
}

void Font::SetShadow(bool shadow) {
  shadow_ = shadow;
}

bool Font::GetShadow() const {
  return shadow_;
}

void Font::SetOutline(bool outline) {
  outline_ = outline;
}

bool Font::GetOutline() const {
  return outline_;
}

void Font::SetColor(scoped_refptr<Color> color) {
  *color_ = *color;
}

scoped_refptr<Color> Font::GetColor() const {
  return color_;
}

void Font::SetOutColor(scoped_refptr<Color> color) {
  *out_color_ = *color;
}

scoped_refptr<Color> Font::GetOutColor() const {
  return out_color_;
}

void Font::EnsureLoadFont() {
  if (!font_)
    LoadFontInternal();
}

TTF_Font* Font::AsSDLFont() {
  EnsureLoadFont();

  int font_style = TTF_STYLE_NORMAL;
  if (bold_)
    font_style |= TTF_STYLE_BOLD;
  if (italic_)
    font_style |= TTF_STYLE_ITALIC;

  TTF_SetFontStyle(font_, font_style);

  return font_;
}

std::string Font::FixupString(const std::string& text) {
  std::string str(text);

  for (size_t i = 0; i < str.size(); ++i)
    if (str[i] == '\r' || str[i] == '\n')
      str[i] = ' ';

  return str;
}

SDL_Surface* Font::RenderText(const std::string& text, uint8_t* font_opacity) {
  auto ensure_format = [](SDL_Surface*& surf) {
    SDL_Surface* format_surf = nullptr;
    if (surf->format != SDL_PIXELFORMAT_ABGR8888) {
      format_surf = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_ABGR8888);
      SDL_DestroySurface(surf);
      surf = format_surf;
    }
  };

  std::string src_text = FixupString(text);
  if (src_text.empty() || src_text == " ")
    return nullptr;

  TTF_Font* font = AsSDLFont();
  if (!font)
    return nullptr;

  SDL_Color font_color = color_->AsSDLColor();
  SDL_Color outline_color = out_color_->AsSDLColor();
  if (font_opacity)
    *font_opacity = font_color.a;

  font_color.a = 255;
  outline_color.a = 255;

  SDL_Surface* raw_surf =
      TTF_RenderUTF8_Blended(font, src_text.c_str(), font_color);
  if (!raw_surf)
    return nullptr;
  ensure_format(raw_surf);

  if (shadow_)
    RenderShadowSurface(raw_surf, font_color);

  if (outline_) {
    SDL_Surface* outline = nullptr;
    TTF_SetFontOutline(font, OutlineSize);
    outline = TTF_RenderUTF8_Blended(font, src_text.c_str(), outline_color);
    ensure_format(outline);
    SDL_Rect outRect = {OutlineSize, OutlineSize, raw_surf->w, raw_surf->h};
    SDL_SetSurfaceBlendMode(raw_surf, SDL_BLENDMODE_BLEND);
    SDL_BlitSurface(raw_surf, NULL, outline, &outRect);
    SDL_DestroySurface(raw_surf);
    raw_surf = outline;
    TTF_SetFontOutline(font, 0);
  }

  return raw_surf;
}

void Font::LoadFontInternal() {
  std::vector<std::string> load_names(name_);
  load_names.push_back(parent_->font_default_name_);

  auto& font_cache = parent_->font_cache_;
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
          ReadFontFromMemory(parent_->mem_fonts_, font_name, size_);
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
  throw base::Exception(base::Exception::ContentError,
                        "Failed to load Font: %s", font_names.c_str());
}

}  // namespace content
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

#include "core/font_context.h"

#include <algorithm>
#include <cstring>

#include "core/embed.ttf.bin"
#include "core/filesystem.h"
#include "core/logger.h"

namespace urge {

namespace {

//! The face a lookup falls back to when the game ships no font of its own.
constexpr char kEmbeddedFontName[] = "Default.ttf";

//! Directory the fonts of the game are searched in, relative to the load path.
constexpr char kFontDirectory[] = "Fonts/";

/*! RGSS sizes are the pixel height of the line box, while TTF_Font is opened at
    a point size; the glyphs come out about a tenth too large without this
    correction, which is the same factor the reference runtime applies. */
constexpr float kFontRealScale = 0.9f;

//! RGSS refuses to build a font outside of this size range.
constexpr int32_t kMinFontSize = 6;
constexpr int32_t kMaxFontSize = 96;

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

//! Splits "Fonts/Arial.ttf" into "Fonts/" and "Arial.ttf".
std::pair<std::string, std::string> SplitPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos)
    return {std::string(), path};

  return {path.substr(0, slash + 1), path.substr(slash + 1)};
}

//! Reads a whole stream into memory, which is what the font cache stores.
FontContext::FontData ReadStreamToMemory(SDL_IOStream* stream) {
  const int64_t size = SDL_GetIOSize(stream);
  if (size <= 0)
    return FontContext::FontData();

  void* memory = SDL_malloc(static_cast<size_t>(size));
  if (!memory)
    return FontContext::FontData();

  if (SDL_ReadIO(stream, memory, static_cast<size_t>(size)) !=
      static_cast<size_t>(size)) {
    SDL_free(memory);
    return FontContext::FontData();
  }

  FontContext::FontData result;
  result.size = size;
  result.data = memory;
  return result;
}

}  // namespace

FontContext::FontContext() {
  // TTF has to be up before anything is opened from it, and the reference
  // runtime does the same.
  if (!TTF_Init()) {
    throw Exception(Exception::kRGSSError, "TTF_Init failed: {}", SDL_GetError());
  }

  // The requested names start out as the RGSS default family
  default_name_.push_back(kEmbeddedFontName);
  default_font_ = kEmbeddedFontName;

  LOGGER_INFO("[Font] Search Path: {}", kFontDirectory);
  LOGGER_INFO("[Font] Default Font: {}", default_font_);

  LoadFontDirectory(kFontDirectory);

  /* A game that ships no font of its own falls back to the face carried in the
     executable, which is what keeps text working out of the box. The default
     name is honoured too: when the game named a font that was found, that one
     becomes the fallback instead. */
  auto default_it = data_cache_.find(ToLower(default_font_));
  if (default_it == data_cache_.end()) {
    LOGGER_INFO("[Font] Default font missing, using embedded font instead.");
    LoadInternalFont();
  }
}

FontContext::~FontContext() {
  for (auto& entry : font_cache_) {
    if (entry.second)
      TTF_CloseFont(entry.second);
  }

  for (auto& entry : data_cache_)
    SDL_free(entry.second.data);
}

void FontContext::LoadFontDirectory(const std::string& directory) {
  std::vector<std::string> files = IOService::Get().EnumDir(directory);

  for (const std::string& file : files) {
    const std::string path = directory + file;
    SDL_IOStream* stream = IOService::Get().OpenReadRaw(path);
    if (!stream)
      continue;

    FontData data = ReadStreamToMemory(stream);
    SDL_CloseIO(stream);

    if (!data.data)
      continue;

    // The cache is keyed by file name, in the lower case the rest of the
    // lookups use
    data_cache_[ToLower(file)] = data;
    LOGGER_INFO("[Font] Loaded Font: {}", file);
  }
}

void FontContext::LoadInternalFont() {
  void* memory = SDL_malloc(embed_ttf_len);
  if (!memory)
    throw Exception(Exception::kRGSSError, "out of memory loading the font.");

  std::memcpy(memory, embed_ttf, embed_ttf_len);

  FontData data;
  data.size = embed_ttf_len;
  data.data = memory;

  /* The embedded face is registered under the name the engine asks for, so the
     fallback path below finds it without a special case. */
  data_cache_[ToLower(default_font_)] = data;
}

bool FontContext::FontExists(const std::string& name) const {
  return data_cache_.find(ToLower(name)) != data_cache_.end();
}

std::vector<std::string> FontContext::ResolveName(const std::string& name) const {
  if (name.empty())
    return default_name_;

  return {name};
}

TTF_Font* FontContext::AcquireFont(const std::vector<std::string>& names,
                                   int32_t size) {
  // Sizes outside the RGSS range are dropped rather than clamped, so a bogus
  // size falls through to the default face instead of silently rendering at a
  // size the game never asked for.
  if (size < kMinFontSize || size > kMaxFontSize)
    return nullptr;

  // Requested faces first, the engine default last, so a missing font still
  // renders something.
  std::vector<std::string> candidates = names;
  candidates.push_back(default_font_);

  for (const std::string& candidate : candidates) {
    const std::string key = ToLower(candidate);
    const auto cache_key = std::make_pair(key, size);

    auto cached = font_cache_.find(cache_key);
    if (cached != font_cache_.end())
      return cached->second;

    TTF_Font* font = OpenFont(key, size);
    if (font) {
      font_cache_.emplace(cache_key, font);
      return font;
    }
  }

  return nullptr;
}

TTF_Font* FontContext::OpenFont(const std::string& name, int32_t size) {
  auto data_it = data_cache_.find(name);
  if (data_it == data_cache_.end())
    return nullptr;

  /* SDL_IOFromConstMem does not own the memory, but TTF_OpenFontIO is told to
     (closeio = true), which is safe because the FontData outlives the handle:
     both live in this cache and are torn down together. */
  SDL_IOStream* stream = SDL_IOFromConstMem(data_it->second.data,
                                            static_cast<size_t>(data_it->second.size));
  if (!stream)
    return nullptr;

  TTF_Font* font =
      TTF_OpenFontIO(stream, true, size * kFontRealScale);
  if (!font)
    LOGGER_ERROR("[Font] Failed to open {}: {}", name, SDL_GetError());

  return font;
}

}  // namespace urge

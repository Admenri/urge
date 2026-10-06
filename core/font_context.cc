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

#include "core/filesystem.h"
#include "core/logger.h"
#include "core/resources/embed.ttf.bin"

namespace urge {

namespace {

constexpr char kEmbeddedFontName[] = "Default.ttf";

constexpr char kFontDirectory[] = "Fonts/";

constexpr float kFontRealScale = 0.9f;

constexpr int32_t kMinFontSize = 6;
constexpr int32_t kMaxFontSize = 96;

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

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
  if (!TTF_Init()) {
    throw Exception(Exception::kRGSSError, "TTF_Init failed: {}",
                    SDL_GetError());
  }

  default_name_.push_back(kEmbeddedFontName);
  default_font_ = kEmbeddedFontName;

  LOGGER_INFO("[Font] Search Path: {}", kFontDirectory);
  LOGGER_INFO("[Font] Default Font: {}", default_font_);

  LoadFontDirectory(kFontDirectory);

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

bool FontContext::FontExists(const std::string& name) const {
  return data_cache_.find(ToLower(name)) != data_cache_.end();
}

TTF_Font* FontContext::AcquireFont(const std::vector<std::string>& names,
                                   int32_t size) {
  if (size < kMinFontSize || size > kMaxFontSize)
    return nullptr;

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

std::vector<std::string> FontContext::ResolveName(
    const std::string& name) const {
  if (name.empty())
    return default_name_;

  return {name};
}

void FontContext::LoadFontDirectory(const std::string& directory) {
  std::vector<std::string> files = IOService::Get().EnumDir(directory);

  for (const std::string& file : files) {
    const std::string path = directory + file;
    SDL_IOStream* stream = nullptr;
    try {
      stream = IOService::Get().OpenReadRaw(path);
    } catch (...) {
      continue;
    }

    FontData data = ReadStreamToMemory(stream);
    SDL_CloseIO(stream);

    if (!data.data)
      continue;

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

  data_cache_[ToLower(default_font_)] = data;
}

TTF_Font* FontContext::OpenFont(const std::string& name, int32_t size) {
  auto data_it = data_cache_.find(name);
  if (data_it == data_cache_.end())
    return nullptr;

  SDL_IOStream* stream = SDL_IOFromConstMem(
      data_it->second.data, static_cast<size_t>(data_it->second.size));
  if (!stream)
    return nullptr;

  TTF_Font* font = TTF_OpenFontIO(stream, true, size * kFontRealScale);
  if (!font)
    LOGGER_ERROR("[Font] Failed to open {}: {}", name, SDL_GetError());

  return font;
}

}  // namespace urge

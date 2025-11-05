// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/canvas/font_context.h"

#include "content/resource/embed.ttf.bin"

namespace content {

namespace {

std::pair<int64_t, void*> ReadFontToMemory(SDL_IOStream* io) {
  int64_t font_size = SDL_GetIOSize(io);
  void* font_ptr = SDL_malloc(font_size);
  SDL_ReadIO(io, font_ptr, font_size);
  return std::make_pair(font_size, font_ptr);
}

}  // namespace

ScopedFontData::ScopedFontData(filesystem::IOService* io,
                               const std::string& default_font_name)
    : default_color(base::MakeRefCounted<ColorImpl>(
          base::Vec4(255.0f, 255.0f, 255.0f, 255.0f))),
      default_out_color(
          base::MakeRefCounted<ColorImpl>(base::Vec4(0, 0, 0, 255.0f))),
      default_gradient_color(
          base::MakeRefCounted<ColorImpl>(base::Vec4(0.0f, 0.0f, 0.0f, 0.0f))) {
  // Get font load dir and default font
  std::string filename = default_font_name;
  std::string dir("."), file;

  size_t last_slash_pos = filename.find_last_of('/');
  if (last_slash_pos != std::string::npos) {
    dir = filename.substr(0, last_slash_pos);
    file = filename.substr(last_slash_pos + 1);
  } else
    file = filename;

  dir.push_back('/');
  default_name.push_back(file);
  default_font = file;

  LOG(INFO) << "[Font] Search Path: " << dir;
  LOG(INFO) << "[Font] Default Font: " << file;

  // Load all font to memory as cache
  std::vector<std::string> font_files = io->EnumDir(dir);
  if (font_files.empty())
    font_files.push_back(default_font);

  for (auto& it : font_files) {
    std::string filepath = dir + it;
    SDL_IOStream* font_stream = io->OpenReadRaw(filepath, nullptr);
    if (font_stream) {
      // Cached in memory
      data_cache.emplace(it, ReadFontToMemory(font_stream));

      // Close i/o stream
      SDL_CloseIO(font_stream);

      LOG(INFO) << "[Font] Loaded Font: " << it;
    }
  }

  // Load internal font if default missing
  auto default_font_it = data_cache.find(default_font);
  if (default_font_it == data_cache.end()) {
    LOG(INFO) << "[Font] Default font missing, use internal font for instead.";

    void* internal_duplicate_data = SDL_malloc(embed_ttf_len);
    std::memcpy(internal_duplicate_data, embed_ttf, embed_ttf_len);

    std::pair<int64_t, void*> cache_pair =
        std::make_pair(embed_ttf_len, internal_duplicate_data);
    data_cache.emplace(default_font, std::move(cache_pair));
  }
}

ScopedFontData::~ScopedFontData() {
  for (auto& it : font_cache)
    TTF_CloseFont(it.second);
  for (auto& it : data_cache)
    SDL_free(it.second.second);
}

bool ScopedFontData::IsFontExisted(const std::string& name) {
  return data_cache.find(name) != data_cache.end();
}

const void* ScopedFontData::GetUIDefaultFont(int64_t* font_size) {
  auto it = data_cache.find(default_font);
  if (it != data_cache.end()) {
    *font_size = it->second.first;
    return it->second.second;
  }

  *font_size = embed_ttf_len;
  return embed_ttf;
}

}  // namespace content

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

#include "core/font/font_context.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <tuple>
#include <vector>

#include "SDL3/SDL_stdinc.h"

#include "core/common/logger.h"
#include "core/resources/embed.ttf.bin"
#include "core/system/filesystem.h"

namespace urge {

namespace {

constexpr char kEmbeddedFontName[] = "Default.ttf";

constexpr char kFontDirectory[] = "Fonts/";

constexpr float kFontRealScale = 0.9f;

constexpr int32_t kMinFontSize = 6;
constexpr int32_t kMaxFontSize = 96;

#if defined(_WIN32)
constexpr const char* kWindowsDefaultFaces[] = {
    "msyh.ttc", "msyh.ttf", "Deng.ttf", "simhei.ttf", "simsun.ttc",
};
#endif

constexpr const char* kFontExtensions[] = {".ttf", ".ttc", ".otf"};

constexpr uint32_t kMaxNameTableBytes = 1u << 20;

constexpr uint32_t kMaxSfntTables = 512;

constexpr uint32_t kTtcTag = 0x74746366u;

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

bool HasFontExtension(const std::string& name) {
  const std::string lower = ToLower(name);
  for (const char* extension : kFontExtensions) {
    const size_t length = std::strlen(extension);
    if (lower.size() > length &&
        lower.compare(lower.size() - length, length, extension) == 0)
      return true;
  }
  return false;
}

#if defined(_WIN32)
std::string PathToUtf8(const std::filesystem::path& path) {
  const std::u8string utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

std::filesystem::path FromUtf8(const std::string& text) {
  return std::filesystem::path(std::u8string(text.begin(), text.end()));
}
#endif

uint16_t ReadBE16(const uint8_t* data) {
  return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

uint32_t ReadBE32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

void AppendUtf8(std::string& out, uint32_t code) {
  if (code < 0x80) {
    out.push_back(static_cast<char>(code));
  } else if (code < 0x800) {
    out.push_back(static_cast<char>(0xC0 | (code >> 6)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  } else if (code < 0x10000) {
    out.push_back(static_cast<char>(0xE0 | (code >> 12)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (code >> 18)));
    out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
  }
}

std::string Utf16BeToUtf8(const uint8_t* data, size_t length) {
  std::string out;
  out.reserve(length);

  for (size_t i = 0; i + 1 < length; i += 2) {
    uint32_t code = ReadBE16(data + i);
    if (code >= 0xD800 && code <= 0xDBFF && i + 3 < length) {
      const uint32_t low = ReadBE16(data + i + 2);
      if (low >= 0xDC00 && low <= 0xDFFF) {
        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
        i += 2;
      }
    }
    AppendUtf8(out, code);
  }

  return out;
}

bool FindNameTable(const uint8_t* records,
                   uint32_t num_tables,
                   uint32_t* offset,
                   uint32_t* length) {
  for (uint32_t i = 0; i < num_tables; ++i) {
    const uint8_t* record = records + static_cast<size_t>(i) * 16;
    if (std::memcmp(record, "name", 4) != 0)
      continue;

    *offset = ReadBE32(record + 8);
    *length = ReadBE32(record + 12);
    return *length >= 6 && *length <= kMaxNameTableBytes;
  }

  return false;
}

void ParseNameTable(const uint8_t* table,
                    uint32_t length,
                    std::vector<std::string>* names) {
  const uint32_t count = ReadBE16(table + 2);
  const uint32_t storage = ReadBE16(table + 4);

  for (uint32_t i = 0; i < count; ++i) {
    const size_t at = 6 + static_cast<size_t>(i) * 12;
    if (at + 12 > length)
      break;

    const uint8_t* record = table + at;
    const uint32_t platform = ReadBE16(record);
    const uint32_t name_id = ReadBE16(record + 6);
    const uint32_t text_length = ReadBE16(record + 8);
    const uint32_t text_offset = ReadBE16(record + 10);

    if (name_id != 1 && name_id != 4)
      continue;
    if (!text_length ||
        static_cast<size_t>(storage) + text_offset + text_length > length)
      continue;

    const uint8_t* text = table + storage + text_offset;
    std::string value =
        (platform == 0 || platform == 3)
            ? Utf16BeToUtf8(text, text_length)
            : std::string(reinterpret_cast<const char*>(text), text_length);

    while (!value.empty() && (value.back() == ' ' || value.back() == '\0'))
      value.pop_back();
    if (value.empty())
      continue;

    if (std::find(names->begin(), names->end(), value) == names->end())
      names->push_back(std::move(value));
  }
}

bool ParseFamilyNames(const uint8_t* data,
                      size_t size,
                      std::vector<std::string>* names) {
  if (size < 12)
    return false;

  size_t base = 0;
  if (ReadBE32(data) == kTtcTag) {
    if (size < 16)
      return false;
    base = ReadBE32(data + 12);
    if (base + 12 > size)
      return false;
  }

  const uint8_t* table = data + base;
  const uint32_t num_tables = ReadBE16(table + 4);
  if (!num_tables || num_tables > kMaxSfntTables)
    return false;
  if (base + 12 + static_cast<size_t>(num_tables) * 16 > size)
    return false;

  uint32_t offset = 0, length = 0;
  if (!FindNameTable(table + 12, num_tables, &offset, &length))
    return false;
  if (static_cast<size_t>(offset) + length > size)
    return false;

  ParseNameTable(data + offset, length, names);
  return true;
}

bool ReadFileFamilyNames(const std::string& path,
                         std::vector<std::string>* names) {
  SDL_IOStream* stream = SDL_IOFromFile(path.c_str(), "rb");
  if (!stream)
    return false;

  uint8_t header[12];
  bool ok = SDL_ReadIO(stream, header, sizeof(header)) == sizeof(header);

  if (ok && ReadBE32(header) == kTtcTag) {
    uint8_t face_offset[4];
    ok = SDL_SeekIO(stream, 12, SDL_IO_SEEK_SET) >= 0 &&
         SDL_ReadIO(stream, face_offset, sizeof(face_offset)) ==
             sizeof(face_offset);
    if (ok)
      ok = SDL_SeekIO(stream, static_cast<Sint64>(ReadBE32(face_offset)),
                      SDL_IO_SEEK_SET) >= 0 &&
           SDL_ReadIO(stream, header, sizeof(header)) == sizeof(header);
  }

  std::vector<uint8_t> records;
  uint32_t offset = 0, length = 0;

  if (ok) {
    const uint32_t num_tables = ReadBE16(header + 4);
    ok = num_tables > 0 && num_tables <= kMaxSfntTables;
    if (ok) {
      records.resize(static_cast<size_t>(num_tables) * 16);
      ok = SDL_ReadIO(stream, records.data(), records.size()) == records.size();
    }
    if (ok)
      ok = FindNameTable(records.data(), num_tables, &offset, &length);
  }

  if (ok) {
    records.assign(length, 0);
    ok =
        SDL_SeekIO(stream, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) >= 0 &&
        SDL_ReadIO(stream, records.data(), records.size()) == records.size();
    if (ok)
      ParseNameTable(records.data(), length, names);
  }

  SDL_CloseIO(stream);
  return ok;
}

}

FontContext::FontContext(URGE_EXCEPT) {
  if (!TTF_Init()) {
    exception_state.Throw(ExceptionState::kRGSSError, "TTF_Init failed: {}",
                          SDL_GetError());
    return;
  }

#if defined(_WIN32)
  for (const char* face : kWindowsDefaultFaces)
    default_name_.emplace_back(face);
#else
  default_name_.emplace_back(kEmbeddedFontName);
#endif

  LOGGER_INFO("[Font] Search Path: {}", kFontDirectory);

  LoadFontDirectory(exception_state, kFontDirectory);
  LoadSystemFonts();

  Entry embedded;
  embedded.embedded = true;
  entries_.emplace(ToLower(kEmbeddedFontName), std::move(embedded));

  for (const std::string& candidate : default_name_) {
    if (Resolve(candidate).empty())
      continue;

    default_font_ = candidate;
    break;
  }

  if (default_font_.empty())
    default_font_ = kEmbeddedFontName;

  if (!default_name_.empty() && default_font_ != default_name_.front()) {
    LOGGER_WARN("[Font] {} is unavailable, {} takes its place",
                default_name_.front(), default_font_);
  }

  LOGGER_INFO("[Font] Default Font: {}", default_font_);
  LOGGER_INFO("[Font] {} faces available", entries_.size());
}

FontContext::~FontContext() {
  for (auto& entry : font_cache_) {
    if (entry.second)
      TTF_CloseFont(entry.second);
  }

  for (auto& entry : entries_)
    SDL_free(entry.second.data.data);
}

bool FontContext::FontExists(const std::string& name) {
  return !Resolve(name).empty();
}

TTF_Font* FontContext::AcquireFont(const std::vector<std::string>& names,
                                   int32_t size,
                                   TTF_FontStyleFlags style,
                                   int32_t outline) {
  if (size < kMinFontSize || size > kMaxFontSize)
    return nullptr;

  std::vector<std::string> candidates = names;
  candidates.push_back(default_font_);

  for (const std::string& candidate : candidates) {
    const std::string key = Resolve(candidate);
    if (key.empty())
      continue;

    const auto cache_key =
        std::make_tuple(key, size, static_cast<int32_t>(style), outline);

    auto cached = font_cache_.find(cache_key);
    if (cached != font_cache_.end())
      return cached->second;

    TTF_Font* font = OpenFont(key, size, style, outline);
    if (font) {
      font_cache_.emplace(cache_key, font);
      return font;
    }
  }

  return nullptr;
}

const FontContext::FontData* FontContext::FindFont(const std::string& name) {
  const std::string key = Resolve(name);
  if (key.empty())
    return nullptr;

  auto entry = entries_.find(key);
  if (entry == entries_.end())
    return nullptr;

  return Load(entry->second);
}

std::string FontContext::Resolve(const std::string& name) {
  if (name.empty())
    return {};

  const std::string key = ToLower(name);
  if (entries_.find(key) != entries_.end())
    return key;

  for (const char* extension : kFontExtensions) {
    const std::string with_extension = key + extension;
    if (entries_.find(with_extension) != entries_.end())
      return with_extension;
  }

  BuildFamilyIndex();

  auto family = families_.find(key);
  if (family != families_.end())
    return family->second;

  return {};
}

const FontContext::FontData* FontContext::Load(Entry& entry) {
  if (entry.data.data)
    return &entry.data;

  if (entry.failed)
    return nullptr;

  if (entry.embedded) {
    void* memory = SDL_malloc(embed_ttf_len);
    if (!memory) {
      LOGGER_ERROR("[Font] out of memory for the embedded face");
      entry.failed = true;
      return nullptr;
    }

    std::memcpy(memory, embed_ttf, embed_ttf_len);

    entry.data.size = embed_ttf_len;
    entry.data.data = memory;
    return &entry.data;
  }

  if (entry.path.empty())
    return nullptr;

  SDL_IOStream* stream = SDL_IOFromFile(entry.path.c_str(), "rb");
  if (!stream) {
    LOGGER_ERROR("[Font] cannot open {}: {}", entry.path, SDL_GetError());
    entry.failed = true;
    return nullptr;
  }

  const int64_t size = SDL_GetIOSize(stream);
  void* memory = size > 0 ? SDL_malloc(static_cast<size_t>(size)) : nullptr;

  if (!memory || SDL_ReadIO(stream, memory, static_cast<size_t>(size)) !=
                     static_cast<size_t>(size)) {
    SDL_free(memory);
    SDL_CloseIO(stream);
    LOGGER_ERROR("[Font] cannot read {}: {}", entry.path, SDL_GetError());
    entry.failed = true;
    return nullptr;
  }

  SDL_CloseIO(stream);

  entry.data.size = size;
  entry.data.data = memory;

  LOGGER_INFO("[Font] Loaded system font: {} ({} KiB)", entry.path,
              size / 1024);
  return &entry.data;
}

void FontContext::LoadFontDirectory(URGE_EXCEPT, const std::string& directory) {
  std::vector<std::string> files = IOService::Get().EnumDir(directory);

  for (const std::string& file : files) {
    if (!HasFontExtension(file))
      continue;

    const std::string path = directory + file;
    SDL_IOStream* stream = nullptr;
    ExceptionState open_state;
    stream = IOService::Get().OpenReadRaw(open_state, path);
    if (open_state.HadException())
      continue;

    const int64_t size = SDL_GetIOSize(stream);
    void* memory = size > 0 ? SDL_malloc(static_cast<size_t>(size)) : nullptr;
    const bool read =
        memory && SDL_ReadIO(stream, memory, static_cast<size_t>(size)) ==
                      static_cast<size_t>(size);
    SDL_CloseIO(stream);

    if (!read) {
      SDL_free(memory);
      continue;
    }

    Entry entry;
    entry.data.size = size;
    entry.data.data = memory;

    entries_[ToLower(file)] = std::move(entry);
    LOGGER_INFO("[Font] Loaded Font: {}", file);
  }
}

void FontContext::LoadSystemFonts() {
#if defined(_WIN32)
  std::vector<std::string> directories;

  if (const char* windows = SDL_getenv("WINDIR"))
    directories.push_back(std::string(windows) + "\\Fonts");
  else
    directories.emplace_back("C:\\Windows\\Fonts");

  if (const char* local = SDL_getenv("LOCALAPPDATA"))
    directories.push_back(std::string(local) + "\\Microsoft\\Windows\\Fonts");

  for (const std::string& directory : directories) {
    std::error_code error;
    std::filesystem::directory_iterator iterator(FromUtf8(directory), error);
    if (error) {
      LOGGER_WARN("[Font] cannot list {}: {}", directory, error.message());
      continue;
    }

    size_t added = 0;
    const std::filesystem::directory_iterator end;
    while (iterator != end) {
      const std::filesystem::directory_entry& item = *iterator;

      std::error_code status_error;
      if (item.is_regular_file(status_error)) {
        const std::string name = PathToUtf8(item.path().filename());
        const std::string key = ToLower(name);

        if (HasFontExtension(name) && entries_.find(key) == entries_.end()) {
          Entry entry;
          entry.path = PathToUtf8(item.path());
          entries_.emplace(key, std::move(entry));
          ++added;
        }
      }

      iterator.increment(error);
      if (error)
        break;
    }

    LOGGER_INFO("[Font] System fonts: {} ({} faces)", directory, added);
  }
#endif
}

void FontContext::BuildFamilyIndex() {
  if (family_index_ready_)
    return;

  family_index_ready_ = true;

  const uint64_t start = SDL_GetTicks();
  size_t indexed = 0;

  for (const auto& [key, entry] : entries_) {
    std::vector<std::string> names;
    bool parsed = false;

    if (entry.data.data) {
      parsed = ParseFamilyNames(static_cast<const uint8_t*>(entry.data.data),
                                static_cast<size_t>(entry.data.size), &names);
    } else if (!entry.path.empty()) {
      parsed = ReadFileFamilyNames(entry.path, &names);
    }

    if (!parsed)
      continue;

    ++indexed;
    for (const std::string& name : names)
      families_.emplace(ToLower(name), key);
  }

  LOGGER_INFO("[Font] Indexed {} names from {} faces in {} ms",
              families_.size(), indexed, SDL_GetTicks() - start);
}

TTF_Font* FontContext::OpenFont(const std::string& key,
                                int32_t size,
                                TTF_FontStyleFlags style,
                                int32_t outline) {
  auto entry = entries_.find(key);
  if (entry == entries_.end())
    return nullptr;

  const FontData* data = Load(entry->second);
  if (!data)
    return nullptr;

  SDL_IOStream* stream =
      SDL_IOFromConstMem(data->data, static_cast<size_t>(data->size));
  if (!stream)
    return nullptr;

  TTF_Font* font = TTF_OpenFontIO(stream, true, size * kFontRealScale);
  if (!font) {
    LOGGER_ERROR("[Font] Failed to open {}: {}", key, SDL_GetError());
    return nullptr;
  }

  if (style != TTF_STYLE_NORMAL)
    TTF_SetFontStyle(font, style);

  if (outline > 0)
    TTF_SetFontOutline(font, outline);

  return font;
}

}

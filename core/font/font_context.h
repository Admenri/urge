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

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "SDL3/SDL_surface.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "core/common/object.h"
#include "core/common/utility.h"

namespace urge {

/* The one font source of the engine: the Ruby `Font` class and the debug UI
   both resolve names here, so they always agree on which face they draw with.

   A name is looked up in this order, first hit wins:

     1. `Fonts/` -- the faces shipped with the game, mounted through the load
        paths, so an archive can carry them too;
     2. the platform font directory -- on Windows `%WINDIR%/Fonts` plus the
        per-user `%LOCALAPPDATA%/Microsoft/Windows/Fonts`;
     3. the face compiled into the executable, always present as a last resort.

   Bundled and system faces are registered by file name (`msyh.ttc`,
   `simhei.ttf`).  A face is also reachable by its family name -- "Microsoft
   YaHei", "微软雅黑" -- because the family index reads the `name` table of every
   registered system face.  That index is built on the first lookup that misses
   the file-name map, so a project that only uses bundled faces never pays for
   it.  Payloads are likewise read on demand and then kept for the lifetime of
   the context, which is why callers may borrow the bytes but never free them. */
class FontContext : public Singleton<FontContext> {
 public:
  FontContext();
  ~FontContext();

  FontContext(const FontContext&) = delete;
  FontContext& operator=(const FontContext&) = delete;

  struct FontData {
    int64_t size = 0;
    void* data = nullptr;
  };

  bool FontExists(const std::string& name);

  TTF_Font* AcquireFont(const std::vector<std::string>& names,
                        int32_t size,
                        TTF_FontStyleFlags style = TTF_STYLE_NORMAL,
                        int32_t outline = 0);

  std::vector<std::string> ResolveName(const std::string& name) const;

  /* Raw payload behind `name`, read on demand and owned by this context.
     Consumers with their own face loader -- the debug UI, for one -- take the
     bytes from here instead of opening a file of their own.  Null when the name
     resolves to nothing. */
  const FontData* FindFont(const std::string& name);

  /* Preferred face names, in order: the platform default first.  On Windows
     that is Microsoft YaHei; elsewhere the bundled face stays the default. */
  const std::vector<std::string>& default_name() const { return default_name_; }
  const std::string& default_font() const { return default_font_; }

 private:
  struct Entry {
    FontData data;
    std::string path;  // on-disk source of a system face, empty otherwise
    bool embedded = false;
    bool failed = false;  // unreadable; do not retry on every lookup
  };

  void LoadFontDirectory(const std::string& directory);

  void LoadSystemFonts();

  void BuildFamilyIndex();

  /* Canonical key of `name` in `entries_`, or an empty string. */
  std::string Resolve(const std::string& name);

  /* Materialises `entry`, returning its payload or null when the bytes cannot
     be read. */
  const FontData* Load(Entry& entry);

  TTF_Font* OpenFont(const std::string& key,
                     int32_t size,
                     TTF_FontStyleFlags style,
                     int32_t outline);

  std::vector<std::string> default_name_;

  std::string default_font_;

  std::map<std::string, Entry> entries_;

  /* Lower-cased family / full name -> key in `entries_`. */
  std::map<std::string, std::string> families_;

  bool family_index_ready_ = false;

  std::map<std::tuple<std::string, int32_t, int32_t, int32_t>, TTF_Font*>
      font_cache_;
};

}  // namespace urge

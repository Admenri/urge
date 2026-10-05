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
#include <utility>
#include <vector>

#include "SDL3/SDL_surface.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "core/object.h"
#include "core/utility.h"

namespace urge {

// Font service: owns the TTF data of every font on the load path and the
// handles opened from it, keyed by file name and point size. Faces are looked
// up by file name, the way RGSS names them.
class FontContext : public Singleton<FontContext> {
 public:
  //! Takes over every font of the current load paths and picks a default.
  FontContext();
  ~FontContext();

  FontContext(const FontContext&) = delete;
  FontContext& operator=(const FontContext&) = delete;

  //! Whether a font of that file name was found in the load path.
  bool FontExists(const std::string& name) const;

  //! Returns the handle of a face, opening and caching it on first use; the
  //! requested faces are tried in order and the default is the last resort.
  //! nullptr when no face could be opened at all.
  TTF_Font* AcquireFont(const std::vector<std::string>& names, int32_t size);

  //! Font face a name resolves to, or an empty vector when it is unknown.
  std::vector<std::string> ResolveName(const std::string& name) const;

  const std::vector<std::string>& default_name() const { return default_name_; }
  const std::string& default_font() const { return default_font_; }

  //! Raw TTF data of one font, as an owned block of memory. Public because a
  //! file local helper builds one, not because it is part of the interface.
  struct FontData {
    int64_t size = 0;
    void* data = nullptr;
  };

 private:
  //! Registers every font of \p directory, keyed by its lower case file name.
  void LoadFontDirectory(const std::string& directory);
  //! Falls back to the face embedded in the executable when the default is missing.
  void LoadInternalFont();

  //! Opens a face from the in memory data, or returns nullptr.
  TTF_Font* OpenFont(const std::string& name, int32_t size);

  //! Requested faces, `Font.default_name` before any game code runs.
  std::vector<std::string> default_name_;
  //! The face of `Font.default_name`, i.e. the fallback for every lookup.
  std::string default_font_;

  //! name -> raw TTF data, of every font that was found.
  std::map<std::string, FontData> data_cache_;
  //! (name, size) -> open handle, shared between Font objects.
  std::map<std::pair<std::string, int32_t>, TTF_Font*> font_cache_;
};

}  // namespace urge

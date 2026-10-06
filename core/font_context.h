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

#include "core/object.h"
#include "core/utility.h"

namespace urge {

class FontContext : public Singleton<FontContext> {
 public:

  FontContext();
  ~FontContext();

  FontContext(const FontContext&) = delete;
  FontContext& operator=(const FontContext&) = delete;

  bool FontExists(const std::string& name) const;

  TTF_Font* AcquireFont(const std::vector<std::string>& names,
                        int32_t size,
                        TTF_FontStyleFlags style = TTF_STYLE_NORMAL,
                        int32_t outline = 0);

  std::vector<std::string> ResolveName(const std::string& name) const;

  const std::vector<std::string>& default_name() const { return default_name_; }
  const std::string& default_font() const { return default_font_; }

  struct FontData {
    int64_t size = 0;
    void* data = nullptr;
  };

 private:

  void LoadFontDirectory(const std::string& directory);

  void LoadInternalFont();

  TTF_Font* OpenFont(const std::string& name,
                     int32_t size,
                     TTF_FontStyleFlags style,
                     int32_t outline);

  std::vector<std::string> default_name_;

  std::string default_font_;

  std::map<std::string, FontData> data_cache_;

  std::map<std::tuple<std::string, int32_t, int32_t, int32_t>, TTF_Font*>
      font_cache_;
};

}  // namespace urge

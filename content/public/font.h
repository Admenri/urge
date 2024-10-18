// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_FONT_H_
#define CONTENT_PUBLIC_FONT_H_

#include <array>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "SDL3/SDL_surface.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "base/memory/ref_counted.h"
#include "components/filesystem/filesystem.h"
#include "content/public/utility.h"

namespace content {

class Font;

class ScopedFontData {
 public:
  ScopedFontData(filesystem::Filesystem* io, const std::string& filename);
  ~ScopedFontData();

  ScopedFontData(const ScopedFontData&) = delete;
  ScopedFontData& operator=(const ScopedFontData&) = delete;

  void* GetUIDefaultFont(int64_t* font_size);

  bool Existed(const std::string& name);
  void SetDefaultName(const std::vector<std::string>& name);
  std::vector<std::string> GetDefaultName();
  void SetDefaultSize(int size);
  int GetDefaultSize();
  void SetDefaultBold(bool bold);
  bool GetDefaultBold();
  void SetDefaultItalic(bool italic);
  bool GetDefaultItalic();
  void SetDefaultShadow(bool shadow);
  bool GetDefaultShadow();
  void SetDefaultOutline(bool outline);
  bool GetDefaultOutline();
  void SetDefaultColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetDefaultColor();
  void SetDefaultOutColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetDefaultOutColor();

 private:
  friend class Font;

  std::vector<std::string> default_name_;
  int default_size_ = 24;
  bool default_bold_ = false;
  bool default_italic_ = false;
  bool default_outline_ = true;
  bool default_shadow_ = false;
  scoped_refptr<Color> default_color_ = nullptr;
  scoped_refptr<Color> default_out_color_ = nullptr;

  std::string font_default_name_;
  std::map<std::pair<std::string, int>, TTF_Font*> font_cache_;
  std::map<std::string, std::pair<int64_t, void*>> mem_fonts_;
};

class Font : public base::RefCounted<Font> {
 public:
  Font(ScopedFontData* parent);
  Font(ScopedFontData* parent, const std::vector<std::string>& name);
  Font(ScopedFontData* parent, const std::vector<std::string>& name, int size);
  Font(const Font& other);

  const Font& operator=(const Font& other);

  void SetName(const std::vector<std::string>& name);
  std::vector<std::string> GetName() const;
  void SetSize(int size);
  int GetSize() const;
  void SetBold(bool bold);
  bool GetBold() const;
  void SetItalic(bool italic);
  bool GetItalic() const;
  void SetShadow(bool shadow);
  bool GetShadow() const;
  void SetOutline(bool outline);
  bool GetOutline() const;
  void SetColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetColor() const;
  void SetOutColor(scoped_refptr<Color> color);
  scoped_refptr<Color> GetOutColor() const;

  void EnsureLoadFont();
  TTF_Font* AsSDLFont();
  std::string FixupString(const std::string& text);
  SDL_Surface* RenderText(const std::string& text, uint8_t* font_opacity);

 private:
  void LoadFontInternal();

  std::vector<std::string> name_;
  int size_;
  bool bold_;
  bool italic_;
  bool outline_;
  bool shadow_;
  scoped_refptr<Color> color_;
  scoped_refptr<Color> out_color_;

  ScopedFontData* parent_;
  TTF_Font* font_;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_FONT_H_
// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CANVAS_FONT_IMPL_H_
#define CONTENT_CANVAS_FONT_IMPL_H_

#include <map>
#include <string>

#include "SDL3/SDL_surface.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "content/canvas/font_context.h"
#include "content/common/color_impl.h"
#include "content/content_config.h"
#include "content/public/engine_font.h"

namespace content {

class FontImpl : public Font {
 public:
  FontImpl(ScopedFontData* parent);
  FontImpl(const std::string& name, uint32_t size, ScopedFontData* parent);
  FontImpl(const FontImpl& other);
  ~FontImpl() override = default;

  FontImpl& operator=(const FontImpl& other);

  static scoped_refptr<FontImpl> From(scoped_refptr<Font> host);

  TTF_Font* GetCanonicalFont(ExceptionState& exception_state);
  SDL_Surface* RenderText(const std::string& text,
                          uint8_t* font_opacity,
                          ExceptionState& exception_state);

 protected:
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Name, std::vector<std::string>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Size, uint32_t);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Bold, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Italic, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Outline, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Shadow, bool);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Color, scoped_refptr<Color>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(OutColor, scoped_refptr<Color>);
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Solid, bool);

 private:
  void LoadFontInternal(ExceptionState& exception_state);
  void EnsureFontSurfaceFormatInternal(SDL_Surface*& surf);

  std::vector<std::string> name_;
  int32_t size_;
  bool bold_;
  bool italic_;
  bool outline_;
  bool shadow_;
  bool solid_;
  scoped_refptr<ColorImpl> color_;
  scoped_refptr<ColorImpl> out_color_;

  ScopedFontData* parent_;
  TTF_Font* font_;
};

}  // namespace content

#endif  //! CONTENT_CANVAS_FONT_IMPL_H_

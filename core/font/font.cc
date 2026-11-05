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

#include "core/font/font.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "core/font/font_context.h"
#include "core/font/font_renderer.h"
#include "core/system/filesystem.h"

namespace urge {

Font::Font(std::vector<std::string> names, int32_t size)
    : name_(names),
      size_(size > 0 ? size : *Attr_DefaultSize()),
      bold_(*Attr_DefaultBold()),
      italic_(*Attr_DefaultItalic()),
      outline_(*Attr_DefaultOutline()),
      shadow_(*Attr_DefaultShadow()),
      solid_(*Attr_DefaultSolid()),
      color_(MakeRefCounted<Color>(*Attr_DefaultColor())),
      out_color_(MakeRefCounted<Color>(*Attr_DefaultOutColor())),
      gradient_color_(MakeRefCounted<Color>(*Attr_DefaultGradientColor())) {
  if (name_.empty()) {
    auto default_name = Attr_DefaultName();
    if (default_name.has_value() && !default_name->empty())
      name_ = *default_name;
  }
}

Font::Font(RefPtr<Font> other)
    : name_(other->name_),
      size_(other->size_),
      bold_(other->bold_),
      italic_(other->italic_),
      outline_(other->outline_),
      shadow_(other->shadow_),
      solid_(other->solid_),
      color_(MakeRefCounted<Color>(other->color_)),
      out_color_(MakeRefCounted<Color>(other->out_color_)),
      gradient_color_(MakeRefCounted<Color>(other->gradient_color_)) {}

bool Font::Existed(std::string name) {
  return FontContext::Get().FontExists(name);
}

ATTR_DEF(Font, std::vector<std::string>, Name) {
  if (value.has_value()) {
    name_ = *value;
    return std::nullopt;
  } else {
    return name_;
  }
}

ATTR_DEF(Font, int32_t, Size) {
  if (value.has_value()) {
    size_ = *value;
    return std::nullopt;
  } else {
    return size_;
  }
}

ATTR_DEF(Font, bool, Bold) {
  if (value.has_value()) {
    bold_ = *value;
    return std::nullopt;
  } else {
    return bold_;
  }
}

ATTR_DEF(Font, bool, Italic) {
  if (value.has_value()) {
    italic_ = *value;
    return std::nullopt;
  } else {
    return italic_;
  }
}

ATTR_DEF(Font, bool, Outline) {
  if (value.has_value()) {
    outline_ = *value;
    return std::nullopt;
  } else {
    return outline_;
  }
}

ATTR_DEF(Font, bool, Shadow) {
  if (value.has_value()) {
    shadow_ = *value;
    return std::nullopt;
  } else {
    return shadow_;
  }
}

ATTR_DEF(Font, bool, Solid) {
  if (value.has_value()) {
    solid_ = *value;
    return std::nullopt;
  } else {
    return solid_;
  }
}

ATTR_DEF(Font, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_ = *value;
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Font, RefPtr<Color>, OutColor) {
  if (value.has_value()) {
    out_color_ = *value;
    return std::nullopt;
  } else {
    return out_color_;
  }
}

ATTR_DEF(Font, RefPtr<Color>, GradientColor) {
  if (value.has_value()) {
    gradient_color_ = *value;
    return std::nullopt;
  } else {
    return gradient_color_;
  }
}

ATTR_DEF(Font, std::vector<std::string>, DefaultName) {
  static std::vector<std::string> default_names;
  if (value.has_value()) {
    default_names = *value;
    return std::nullopt;
  } else if (!default_names.empty()) {
    return default_names;
  } else {
    return FontContext::Get().default_name();
  }
}

ATTR_DEF(Font, int32_t, DefaultSize) {
  static int32_t default_size = 24;
  if (value.has_value()) {
    default_size = *value;
    return std::nullopt;
  } else {
    return default_size;
  }
}

ATTR_DEF(Font, bool, DefaultBold) {
  static bool default_bold = false;
  if (value.has_value()) {
    default_bold = *value;
    return std::nullopt;
  } else {
    return default_bold;
  }
}

ATTR_DEF(Font, bool, DefaultItalic) {
  static bool default_italic = false;
  if (value.has_value()) {
    default_italic = *value;
    return std::nullopt;
  } else {
    return default_italic;
  }
}

ATTR_DEF(Font, bool, DefaultOutline) {
  static bool default_outline = true;
  if (value.has_value()) {
    default_outline = *value;
    return std::nullopt;
  } else {
    return default_outline;
  }
}

ATTR_DEF(Font, bool, DefaultShadow) {
  static bool default_shadow = false;
  if (value.has_value()) {
    default_shadow = *value;
    return std::nullopt;
  } else {
    return default_shadow;
  }
}

ATTR_DEF(Font, bool, DefaultSolid) {
  static bool default_solid = false;
  if (value.has_value()) {
    default_solid = *value;
    return std::nullopt;
  } else {
    return default_solid;
  }
}

ATTR_DEF(Font, RefPtr<Color>, DefaultColor) {
  static RefPtr<Color> default_color =
      MakeRefCounted<Color>(255.0f, 255.0f, 255.0f, 255.0f);
  if (value.has_value()) {
    default_color = *value;
    return std::nullopt;
  } else {
    return default_color;
  }
}

ATTR_DEF(Font, RefPtr<Color>, DefaultOutColor) {
  static RefPtr<Color> default_out_color =
      MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 128.0f);
  if (value.has_value()) {
    default_out_color = *value;
    return std::nullopt;
  } else {
    return default_out_color;
  }
}

ATTR_DEF(Font, RefPtr<Color>, DefaultGradientColor) {
  static RefPtr<Color> default_gradient_color =
      MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 0.0f);
  if (value.has_value()) {
    default_gradient_color = *value;
    return std::nullopt;
  } else {
    return default_gradient_color;
  }
}

bool Font::MeasureText(const std::string& text,
                       int32_t* width,
                       int32_t* height) {
  return FontRenderer::Measure(*this, text, width, height);
}

}

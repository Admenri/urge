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

#include "core/font/font_backend.h"

#if !defined(__APPLE__)
#include <ft2build.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_SYNTHESIS_H
#include FT_TRUETYPE_TABLES_H
#include "core/font/font.h"
#include "core/font/font_context.h"

namespace urge::font {
namespace {
class FreeTypeFace final : public Face {
 public:
  FT_Face face = nullptr;
  CellMetrics cell;
  bool bold = false, italic = false;
  std::unordered_map<uint32_t, float> advances;
  float Advance(uint32_t glyph) {
    auto found = advances.find(glyph);
    if (found != advances.end())
      return found->second;
    FT_Set_Transform(face, nullptr, nullptr);
    if (FT_Load_Glyph(face, glyph, FT_LOAD_DEFAULT | FT_LOAD_NO_BITMAP))
      return 0;
    if (bold)
      FT_GlyphSlot_Embolden(face->glyph);
    float value = face->glyph->advance.x / 64.0f;
    if (advances.size() >= 4096)
      advances.erase(advances.begin());
    advances.emplace(glyph, value);
    return value;
  }
  ~FreeTypeFace() override {
    if (face)
      FT_Done_Face(face);
  }
  Mask Rasterize(uint32_t index,
                 int outline,
                 bool solid,
                 float px,
                 float py) override {
    Mask result;
    FT_Matrix matrix{0x10000, italic ? 0x3333L : 0L, 0, 0x10000};
    FT_Vector delta{static_cast<FT_Pos>(std::lround(px * 64)),
                    static_cast<FT_Pos>(std::lround(-py * 64))};
    FT_Set_Transform(face, &matrix, &delta);
    // Outline priority prevents SimHei/SimSun's monochrome strikes from
    // silently disabling gray antialiasing. Bitmap-only fonts still work.
    int flags = FT_LOAD_TARGET_NORMAL;
    if (FT_IS_SCALABLE(face))
      flags |= FT_LOAD_NO_BITMAP;
    if (FT_Load_Glyph(face, index, flags))
      return result;
    if (bold)
      FT_GlyphSlot_Embolden(face->glyph);
    FT_Glyph glyph = nullptr;
    if (FT_Get_Glyph(face->glyph, &glyph))
      return result;
    if (outline && glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
      FT_Stroker stroker = nullptr;
      if (!FT_Stroker_New(face->glyph->library, &stroker)) {
        FT_Stroker_Set(stroker, outline * 64, FT_STROKER_LINECAP_ROUND,
                       FT_STROKER_LINEJOIN_ROUND, 0);
        FT_Glyph_Stroke(&glyph, stroker, true);
        FT_Stroker_Done(stroker);
      }
    }
    FT_BBox bounds{};
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bounds);
    if (bounds.xMax - bounds.xMin > 512 || bounds.yMax - bounds.yMin > 512) {
      FT_Done_Glyph(glyph);
      return {};
    }
    if (FT_Glyph_To_Bitmap(&glyph,
                           solid ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL,
                           nullptr, true)) {
      FT_Done_Glyph(glyph);
      return result;
    }
    auto* bitmap = reinterpret_cast<FT_BitmapGlyph>(glyph);
    const auto& b = bitmap->bitmap;
    result.x = bitmap->left;
    result.y = -bitmap->top;
    result.width = static_cast<int>(b.width);
    result.height = static_cast<int>(b.rows);
    if (result.width <= 0 || result.height <= 0 || result.width > 512 ||
        result.height > 512) {
      FT_Done_Glyph(glyph);
      return {};
    }
    result.pixels.resize(static_cast<size_t>(result.width) * result.height);
    for (int y = 0; y < result.height; ++y) {
      const auto* row = b.buffer + (b.pitch >= 0 ? y : result.height - 1 - y) *
                                       std::abs(b.pitch);
      for (int x = 0; x < result.width; ++x) {
        uint8_t value = 0;
        switch (b.pixel_mode) {
          case FT_PIXEL_MODE_MONO:
            value = (row[x / 8] & (0x80 >> (x % 8))) ? 255 : 0;
            break;
          case FT_PIXEL_MODE_GRAY:
            value = b.num_grays > 1 ? row[x] * 255 / (b.num_grays - 1) : 0;
            break;
          case FT_PIXEL_MODE_GRAY2:
            value = ((row[x / 4] >> (6 - (x % 4) * 2)) & 3) * 85;
            break;
          case FT_PIXEL_MODE_GRAY4:
            value = ((row[x / 2] >> (4 - (x % 2) * 4)) & 15) * 17;
            break;
        }
        result.pixels[static_cast<size_t>(y) * result.width + x] = value;
      }
    }
    FT_Done_Glyph(glyph);
    return result;
  }
};
class FreeTypeBackend final : public Backend {
 public:
  explicit FreeTypeBackend(FontContext& context) : context_(context) {
    FT_Init_FreeType(&library_);
  }
  ~FreeTypeBackend() override {
    faces_.clear();
    if (library_)
      FT_Done_FreeType(library_);
  }
  Layout Shape(const Font& font, const std::string& text) override {
    Layout result;
    std::vector<uint32_t> characters;
    if (!library_ || !DecodeUTF8(text, characters))
      return result;
    auto names = font.name_;
    names.push_back(context_.default_font());
    names.emplace_back("Default.ttf");
    std::vector<std::shared_ptr<FreeTypeFace>> candidates;
    for (const auto& name : names) {
      auto face = GetFace(name, font);
      if (face && std::find(candidates.begin(), candidates.end(), face) ==
                      candidates.end())
        candidates.push_back(std::move(face));
    }
    if (candidates.empty())
      return result;
    auto primary = candidates.front();
    result.ascent = primary->cell.ascent;
    result.height = primary->cell.height;
    std::shared_ptr<FreeTypeFace> previous;
    FT_UInt previous_glyph = 0;
    for (auto c : characters) {
      auto selected = primary;
      FT_UInt glyph = 0;
      for (auto candidate : candidates) {
        glyph = FT_Get_Char_Index(candidate->face, c);
        if (glyph) {
          selected = candidate;
          break;
        }
      }
      FT_Set_Transform(selected->face, nullptr, nullptr);
      if (previous == selected && previous_glyph && glyph &&
          FT_HAS_KERNING(selected->face)) {
        FT_Vector kern{};
        FT_Get_Kerning(selected->face, previous_glyph, glyph,
                       FT_KERNING_DEFAULT, &kern);
        result.width += kern.x / 64.0f;
      }
      result.glyphs.push_back(
          {selected, glyph, result.width,
           static_cast<float>(selected->cell.ascent - primary->cell.ascent)});
      result.width += selected->Advance(glyph);
      previous = selected;
      previous_glyph = glyph;
    }
    return result;
  }

 private:
  std::shared_ptr<FreeTypeFace> GetFace(const std::string& name,
                                        const Font& font) {
    const auto* data = context_.FindFont(name);
    if (!data)
      return {};
    auto key =
        std::make_tuple(data->data, font.size_, font.bold_, font.italic_);
    auto it = faces_.find(key);
    if (it != faces_.end())
      return it->second;
    auto face = std::make_shared<FreeTypeFace>();
    if (FT_New_Memory_Face(library_, static_cast<const FT_Byte*>(data->data),
                           static_cast<FT_Long>(data->size), 0, &face->face))
      return {};
    if (FT_IS_SCALABLE(face->face) && face->face->units_per_EM) {
      double ascent = face->face->ascender;
      double descent = -face->face->descender;
      const auto* os2 = static_cast<const TT_OS2*>(
          FT_Get_Sfnt_Table(face->face, ft_sfnt_os2));
      if (os2 && os2->version != 0xffff &&
          (os2->usWinAscent || os2->usWinDescent)) {
        ascent = os2->usWinAscent;
        descent = os2->usWinDescent;
      }
      face->cell = MakeCellMetrics(font.size_, face->face->units_per_EM, ascent,
                                   descent);
      // Set an explicit design-unit scale: rounding the em size to an integer
      // first makes fonts with tall Windows cells grow beyond RGSS line boxes.
      FT_Size_RequestRec request{};
      request.type = FT_SIZE_REQUEST_TYPE_SCALES;
      request.height = static_cast<FT_Long>(std::lround(
          face->cell.em_size * 64 * 65536 / face->face->units_per_EM));
      if (FT_Request_Size(face->face, &request))
        return {};
    } else {
      if (FT_Set_Pixel_Sizes(face->face, 0, font.size_))
        return {};
      face->cell = {
          static_cast<double>(font.size_),
          static_cast<int>(face->face->size->metrics.ascender / 64),
          std::max(1, static_cast<int>(face->face->size->metrics.height / 64))};
    }
    face->bold = font.bold_;
    face->italic = font.italic_;
    face->id = ++next_id_;
    if (faces_.size() >= 128)
      faces_.erase(faces_.begin());
    faces_.emplace(key, face);
    return face;
  }
  FontContext& context_;
  FT_Library library_ = nullptr;
  uint64_t next_id_ = 0;
  std::map<std::tuple<void*, int, bool, bool>, std::shared_ptr<FreeTypeFace>>
      faces_;
};
}  // namespace
std::unique_ptr<Backend> CreateBackend(FontContext& context) {
  return std::make_unique<FreeTypeBackend>(context);
}
}  // namespace urge::font
#endif

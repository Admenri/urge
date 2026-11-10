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
#include <memory>
#include <string>
#include <vector>

namespace urge {
class Font;
class FontContext;
namespace font {

// RGSS's positive GDI font height describes a character cell, not an em.
// Keep the conversion shared so native and FreeType faces use one contract.
struct CellMetrics {
  double em_size = 0;
  int ascent = 0, height = 0;
};
CellMetrics MakeCellMetrics(int size,
                            double units_per_em,
                            double ascent,
                            double descent);

// Positions and metrics are in RGSS bitmap pixels, independent of display DPI.
struct Mask {
  int x = 0, y = 0, width = 0, height = 0;
  std::vector<uint8_t> pixels;
};
class Face {
 public:
  virtual ~Face() = default;
  uint64_t id = 0;
  virtual Mask Rasterize(uint32_t glyph,
                         int outline,
                         bool solid,
                         float phase_x,
                         float phase_y) = 0;
};
struct Glyph {
  std::shared_ptr<Face> face;
  uint32_t index = 0;
  float x = 0, y = 0;
};
struct Layout {
  float width = 0;
  int height = 0, ascent = 0;
  std::vector<Glyph> glyphs;
};
class Backend {
 public:
  virtual ~Backend() = default;
  virtual Layout Shape(const Font& font, const std::string& text) = 0;
};
std::unique_ptr<Backend> CreateBackend(FontContext& context);
// Strict UTF-8 decoding; invalid input fails without touching native APIs.
bool DecodeUTF8(const std::string& text, std::vector<uint32_t>& characters);
}  // namespace font
}  // namespace urge

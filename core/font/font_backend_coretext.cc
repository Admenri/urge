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

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>
#include "core/font/font.h"
#include "core/font/font_context.h"

namespace urge::font {
namespace {
template <class T>
class CF {
 public:
  explicit CF(T value) : value_(value) {}
  ~CF() {
    if (value_)
      CFRelease(value_);
  }
  CF(const CF&) = delete;
  CF& operator=(const CF&) = delete;
  T get() const { return value_; }

 private:
  T value_;
};
class CoreTextFace final : public Face {
 public:
  CTFontRef font = nullptr;
  bool bold = false, italic = false;
  struct Advance {
    CGGlyph glyph = 0;
    float width = 0;
  };
  std::unordered_map<uint32_t, Advance> advances;
  Advance Lookup(uint32_t c) {
    auto found = advances.find(c);
    if (found != advances.end())
      return found->second;
    UniChar character = static_cast<UniChar>(c);
    CGGlyph glyph = 0;
    CGSize advance{};
    CTFontGetGlyphsForCharacters(font, &character, &glyph, 1);
    CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, &glyph,
                               &advance, 1);
    Advance result{glyph, static_cast<float>(advance.width)};
    if (advances.size() >= 4096)
      advances.erase(advances.begin());
    advances.emplace(c, result);
    return result;
  }
  ~CoreTextFace() override {
    if (font)
      CFRelease(font);
  }
  Mask Rasterize(uint32_t index,
                 int outline,
                 bool solid,
                 float px,
                 float py) override {
    Mask result;
    const CGGlyph glyph = static_cast<CGGlyph>(index);
    CGAffineTransform matrix =
        CGAffineTransformMake(1, 0, italic ? 0.2 : 0, 1, px, -py);
    CF<CGPathRef> path(CTFontCreatePathForGlyph(font, glyph, &matrix));
    CGRect bounds = path.get() ? CGPathGetBoundingBox(path.get())
                               : CTFontGetBoundingRectsForGlyphs(
                                     font, kCTFontOrientationHorizontal, &glyph,
                                     nullptr, 1);
    if (!std::isfinite(bounds.origin.x) || !std::isfinite(bounds.origin.y) ||
        !std::isfinite(bounds.size.width) ||
        !std::isfinite(bounds.size.height) ||
        std::abs(bounds.origin.x) > 4096 || std::abs(bounds.origin.y) > 4096 ||
        bounds.size.width > 512 || bounds.size.height > 512 ||
        CGRectIsEmpty(bounds) || CGRectIsNull(bounds))
      return result;
    const float embolden = bold ? std::max(0.5, CTFontGetSize(font) / 40) : 0;
    const float stroke = outline + embolden;
    const int left =
        static_cast<int>(std::floor(CGRectGetMinX(bounds) - stroke)) - 1;
    const int top =
        static_cast<int>(std::ceil(CGRectGetMaxY(bounds) + stroke)) + 1;
    const int right =
        static_cast<int>(std::ceil(CGRectGetMaxX(bounds) + stroke)) + 1;
    const int bottom =
        static_cast<int>(std::floor(CGRectGetMinY(bounds) - stroke)) - 1;
    result.x = left;
    result.y = -top;
    result.width = right - left;
    result.height = top - bottom;
    if (result.width <= 0 || result.height <= 0 || result.width > 512 ||
        result.height > 512)
      return {};
    result.pixels.resize(static_cast<size_t>(result.width) * result.height);
    CF<CGContextRef> context(
        CGBitmapContextCreate(result.pixels.data(), result.width, result.height,
                              8, result.width, nullptr, kCGImageAlphaOnly));
    if (!context.get())
      return {};
    CGContextSetAllowsAntialiasing(context.get(), !solid);
    CGContextSetShouldAntialias(context.get(), !solid);
    // Gray coverage, never LCD color fringes on transparent game bitmaps.
    CGContextSetAllowsFontSmoothing(context.get(), false);
    CGContextSetShouldSmoothFonts(context.get(), false);
    CGContextSetGrayFillColor(context.get(), 1, 1);
    CGContextSetGrayStrokeColor(context.get(), 1, 1);
    CGContextTranslateCTM(context.get(), -left, top);
    CGContextScaleCTM(context.get(), 1, -1);
    if (path.get()) {
      // Drawing the outline path also bypasses monochrome strikes in TTFs.
      CGContextAddPath(context.get(), path.get());
      if (stroke > 0) {
        CGContextSetLineWidth(context.get(), stroke * 2);
        CGContextSetLineJoin(context.get(), kCGLineJoinRound);
        CGContextDrawPath(context.get(), kCGPathFillStroke);
      } else
        CGContextFillPath(context.get());
    } else {
      // Bitmap-only faces have no path; keep the native glyph fallback.
      CGPoint origin = CGPointZero;
      CTFontDrawGlyphs(font, &glyph, &origin, 1, context.get());
    }
    // CGBitmapContext's alpha storage is bottom-up for this native CTM;
    // atlas rows and the rest of the engine use top-down image coordinates.
    for (int y = 0; y < result.height / 2; ++y)
      for (int x = 0; x < result.width; ++x)
        std::swap(result.pixels[static_cast<size_t>(y) * result.width + x],
                  result.pixels[static_cast<size_t>(result.height - 1 - y) *
                                    result.width +
                                x]);
    return result;
  }
};
class CoreTextBackend final : public Backend {
 public:
  explicit CoreTextBackend(FontContext& context) : context_(context) {}
  Layout Shape(const Font& font, const std::string& text) override {
    Layout result;
    std::vector<uint32_t> check;
    if (!DecodeUTF8(text, check))
      return result;
    auto names = font.name_;
    names.push_back(context_.default_font());
    names.emplace_back("Default.ttf");
    std::shared_ptr<CoreTextFace> primary;
    for (const auto& name : names) {
      primary = GetFace(name, font);
      if (primary)
        break;
    }
    if (!primary)
      return result;
    // Numeric/CJK counters need no contextual shaping. Cache native advances
    // instead of constructing a CoreText line for every changing number.
    // Other scripts and alphabetic kerning continue through CTLine below.
    const bool simple = std::all_of(check.begin(), check.end(), [](uint32_t c) {
      return (c >= 0x3400 && c <= 0x9fff) ||
             (c >= 0x3040 && c <= 0x30ff && c != 0x3099 && c != 0x309a) ||
             (c >= '0' && c <= '9') || c == ' ' || c == '/' || c == '.' ||
             c == ':' || c == '%' || c == '+' || c == '-' || c == '=';
    });
    if (simple) {
      bool complete = true;
      for (auto c : check) {
        auto a = primary->Lookup(c);
        if (!a.glyph) {
          complete = false;
          break;
        }
        result.glyphs.push_back({primary, a.glyph, result.width, 0});
        result.width += a.width;
      }
      if (complete) {
        result.ascent =
            static_cast<int>(std::ceil(CTFontGetAscent(primary->font)));
        result.height = std::max(
            1, static_cast<int>(std::ceil(CTFontGetAscent(primary->font) +
                                          CTFontGetDescent(primary->font))));
        return result;
      }
      result = {};
    }
    CF<CFStringRef> string(CFStringCreateWithBytes(
        nullptr, reinterpret_cast<const UInt8*>(text.data()), text.size(),
        kCFStringEncodingUTF8, false));
    if (!string.get())
      return result;
    const void* keys[] = {kCTFontAttributeName,
                          kCTForegroundColorFromContextAttributeName,
                          kCTLigatureAttributeName};
    int zero = 0;
    CF<CFNumberRef> no_ligatures(
        CFNumberCreate(nullptr, kCFNumberIntType, &zero));
    const void* values[] = {primary->font, kCFBooleanTrue, no_ligatures.get()};
    CF<CFDictionaryRef> attributes(CFDictionaryCreate(
        nullptr, keys, values, 3, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks));
    CF<CFAttributedStringRef> attributed(
        CFAttributedStringCreate(nullptr, string.get(), attributes.get()));
    CF<CTLineRef> line(CTLineCreateWithAttributedString(attributed.get()));
    if (!line.get())
      return result;
    result.width = static_cast<float>(
        CTLineGetTypographicBounds(line.get(), nullptr, nullptr, nullptr));
    result.ascent = static_cast<int>(std::ceil(CTFontGetAscent(primary->font)));
    result.height = std::max(
        1, static_cast<int>(std::ceil(CTFontGetAscent(primary->font) +
                                      CTFontGetDescent(primary->font))));
    CFArrayRef runs = CTLineGetGlyphRuns(line.get());
    for (CFIndex i = 0; i < CFArrayGetCount(runs); ++i) {
      auto run = static_cast<CTRunRef>(
          const_cast<void*>(CFArrayGetValueAtIndex(runs, i)));
      const CFIndex count = CTRunGetGlyphCount(run);
      auto selected = static_cast<CTFontRef>(
          CFDictionaryGetValue(CTRunGetAttributes(run), kCTFontAttributeName));
      auto face = RunFace(selected, font.bold_, font.italic_);
      std::vector<CGGlyph> glyphs(count);
      std::vector<CGPoint> positions(count);
      CTRunGetGlyphs(run, CFRangeMake(0, 0), glyphs.data());
      CTRunGetPositions(run, CFRangeMake(0, 0), positions.data());
      for (CFIndex j = 0; j < count; ++j)
        result.glyphs.push_back({face, glyphs[j],
                                 static_cast<float>(positions[j].x),
                                 static_cast<float>(-positions[j].y)});
    }
    return result;
  }

 private:
  std::shared_ptr<CoreTextFace> RunFace(CTFontRef font,
                                        bool bold,
                                        bool italic) {
    std::erase_if(runs_, [](const auto& face) { return face.expired(); });
    for (auto& weak : runs_)
      if (auto face = weak.lock(); face && face->bold == bold &&
                                   face->italic == italic &&
                                   CFEqual(face->font, font))
        return face;
    auto face = std::make_shared<CoreTextFace>();
    face->font = font;
    CFRetain(font);
    face->bold = bold;
    face->italic = italic;
    face->id = ++next_id_;
    runs_.push_back(face);
    return face;
  }
  std::shared_ptr<CoreTextFace> GetFace(const std::string& name,
                                        const Font& spec) {
    const auto* bytes = context_.FindFont(name);
    if (!bytes)
      return {};
    auto key =
        std::make_tuple(bytes->data, spec.size_, spec.bold_, spec.italic_);
    auto found = faces_.find(key);
    if (found != faces_.end())
      return found->second;
    CF<CFDataRef> data(CFDataCreateWithBytesNoCopy(
        nullptr, static_cast<const UInt8*>(bytes->data), bytes->size,
        kCFAllocatorNull));
    CF<CGDataProviderRef> provider(CGDataProviderCreateWithCFData(data.get()));
    CF<CGFontRef> cgfont(CGFontCreateWithDataProvider(provider.get()));
    if (!cgfont.get())
      return {};
    CF<CTFontRef> ctfont(CTFontCreateWithGraphicsFont(cgfont.get(), spec.size_,
                                                      nullptr, nullptr));
    if (!ctfont.get())
      return {};
    auto face = RunFace(ctfont.get(), spec.bold_, spec.italic_);
    if (faces_.size() >= 128)
      faces_.erase(faces_.begin());
    faces_.emplace(key, face);
    return face;
  }
  FontContext& context_;
  uint64_t next_id_ = 0;
  std::vector<std::weak_ptr<CoreTextFace>> runs_;
  std::map<std::tuple<void*, int, bool, bool>, std::shared_ptr<CoreTextFace>>
      faces_;
};
}  // namespace
std::unique_ptr<Backend> CreateBackend(FontContext& context) {
  return std::make_unique<CoreTextBackend>(context);
}
}  // namespace urge::font
#endif

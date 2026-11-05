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

#include "core/font/font_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>
#include "SDL3/SDL_stdinc.h"
#include "core/canvas/bitmap.h"
#include "core/canvas/bitmap_queue.h"
#include "core/common/logger.h"
#include "core/font/font.h"
#include "core/font/font_backend.h"
#include "core/font/font_context.h"
#include "core/render/gpu_utils.h"
#include "core/render/pipeline.h"
#include "core/render/shader.h"

namespace urge {
namespace {
constexpr int kAtlasSize = 1024;
constexpr size_t kMaxAtlasPages = 8;
constexpr size_t kLayoutBudget = 2 * 1024 * 1024;
constexpr size_t kMaxLayouts = 1024;

// Compatible with Bitmap's existing scene/object uniforms. All font-specific
// shading stays here; the canvas queue only submits generic triangle batches.
const ShaderSource kVertex = {
    R"(
struct Scene { matrix: mat4x4<f32> };
struct Object { matrix: mat4x4<f32> };
@group(0) @binding(0) var<uniform> scene: Scene;
@group(1) @binding(0) var<uniform> object: Object;
struct Input {
 @location(0) position: vec4<f32>,
 @location(1) uv: vec2<f32>,
 @location(2) color: vec4<f32>,
};
struct Output {
 @builtin(position) position: vec4<f32>,
 @location(0) uv: vec2<f32>,
 @location(1) color: vec4<f32>,
};
@vertex fn vs_main(v: Input)->Output {
 var o: Output;
 o.position=scene.matrix*object.matrix*v.position;
 o.uv=v.uv; o.color=v.color; return o;
}
)",
    "vs_main",
    {{0,
      {{0, wgpu::ShaderStage::Vertex, util::BufferLayout::Uniform(false, 64)}}},
     {1,
      {{0, wgpu::ShaderStage::Vertex,
        util::BufferLayout::Uniform(false, 64)}}}}};
const ShaderSource kFragment = {
    R"(
@group(2) @binding(0) var atlas: texture_2d<f32>;
@group(2) @binding(1) var filtering: sampler;
@fragment fn fs_main(@location(0) uv:vec2<f32>,@location(1) color:vec4<f32>)->@location(0) vec4<f32> {
 return color*textureSample(atlas,filtering,uv).r;
}
)",
    "fs_main",
    {{2,
      {{0, wgpu::ShaderStage::Fragment,
        util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
       {1, wgpu::ShaderStage::Fragment, util::SamplerLayout::Filtering()}}}}};

std::string LayoutKey(const Font& f, const std::string& text) {
  std::string key =
      std::to_string(f.size_) + (f.bold_ ? "b" : "-") + (f.italic_ ? "i" : "-");
  for (const auto& name : f.name_)
    key += std::to_string(name.size()) + ":" + name;
  key += '\0';
  key += text;
  return key;
}
struct GlyphKey {
  uint64_t face;
  uint32_t glyph;
  uint8_t outline, solid, x, y;
  bool operator==(const GlyphKey&) const = default;
};
struct GlyphHash {
  size_t operator()(const GlyphKey& k) const {
    size_t h = std::hash<uint64_t>{}(k.face);
    h ^= static_cast<size_t>(k.glyph) * 0x9e3779b9;
    return h ^ (static_cast<size_t>(k.outline) << 24) ^
           (static_cast<size_t>(k.solid) << 20) ^ (k.x << 4) ^ k.y;
  }
};
struct Page {
  uint64_t id = 0, stamp = 0;
  int x = 0, y = 0, row_height = 0;
  int dirty_left = kAtlasSize, dirty_top = kAtlasSize, dirty_right = 0,
      dirty_bottom = 0;
  wgpu::Texture texture;
  wgpu::BindGroup group;
  std::vector<uint8_t> pixels;
  bool Pack(int width, int height, int& ox, int& oy) {
    if (x + width > kAtlasSize) {
      x = 0;
      y += row_height;
      row_height = 0;
    }
    if (y + height > kAtlasSize)
      return false;
    ox = x;
    oy = y;
    x += width;
    row_height = std::max(row_height, height);
    return true;
  }
};
struct AtlasGlyph {
  std::shared_ptr<Page> page;
  int x = 0, y = 0, width = 0, height = 0, bearing_x = 0, bearing_y = 0;
};
struct Prepared {
  AtlasGlyph image;
  float x = 0, y = 0;
};
glm::vec4 PremultipliedColor(const RefPtr<urge::Color>& color,
                             float opacity = 1) {
  if (!color)
    return glm::vec4(0);
  glm::vec4 c = glm::clamp(
      glm::vec4(color->data.r, color->data.g, color->data.b, color->data.a) /
          255.0f,
      glm::vec4(0), glm::vec4(1));
  c.a *= opacity;
  c.r *= c.a;
  c.g *= c.a;
  c.b *= c.a;
  return c;
}
void Quad(std::vector<VertexData>& out,
          const RectF& rect,
          const RectF& uv,
          const glm::vec4& top,
          const glm::vec4& bottom) {
  const VertexData a{{rect.x, rect.y, 0, 1}, {uv.x, uv.y}, top};
  const VertexData b{
      {rect.x + rect.width, rect.y, 0, 1}, {uv.x + uv.width, uv.y}, top};
  const VertexData c{
      {rect.x, rect.y + rect.height, 0, 1}, {uv.x, uv.y + uv.height}, bottom};
  const VertexData d{{rect.x + rect.width, rect.y + rect.height, 0, 1},
                     {uv.x + uv.width, uv.y + uv.height},
                     bottom};
  out.insert(out.end(), {a, b, c, c, b, d});
}
}  // namespace

struct FontRenderer::Impl {
  struct CachedLayout {
    font::Layout layout;
    size_t bytes = 0;
    uint64_t stamp = 0;
  };
  std::unique_ptr<font::Backend> backend =
      font::CreateBackend(FontContext::Get());
  std::unordered_map<std::string, CachedLayout> layouts;
  std::unordered_map<GlyphKey, AtlasGlyph, GlyphHash> glyphs;
  std::vector<std::shared_ptr<Page>> pages;
  size_t layout_bytes = 0;
  uint64_t stamp = 0, next_page = 0;
  wgpu::RenderPipeline pipeline;
  wgpu::Sampler sampler;
  Statistics stats;
  font::Layout transient_layout;
  std::vector<uint8_t> upload_scratch;

  const font::Layout* Layout(Font& f, const std::string& text) {
    transient_layout = {};
    if (f.size_ < 6 || f.size_ > 96 || text.empty())
      return nullptr;
    auto key = LayoutKey(f, text);
    auto it = layouts.find(key);
    if (it != layouts.end()) {
      ++stats.layout_hits;
      it->second.stamp = ++stamp;
      return &it->second.layout;
    }
    auto layout = backend->Shape(f, text);
    ++stats.layouts;
    if (layout.height <= 0 || !std::isfinite(layout.width) ||
        layout.width < 0 || layout.width > std::numeric_limits<int32_t>::max())
      return nullptr;
    const size_t bytes = key.size() + sizeof(CachedLayout) +
                         layout.glyphs.size() * sizeof(font::Glyph);
    while (!layouts.empty() && (layout_bytes + bytes > kLayoutBudget ||
                                layouts.size() >= kMaxLayouts)) {
      auto victim = std::min_element(layouts.begin(), layouts.end(),
                                     [](const auto& a, const auto& b) {
                                       return a.second.stamp < b.second.stamp;
                                     });
      layout_bytes -= victim->second.bytes;
      layouts.erase(victim);
    }
    if (bytes > kLayoutBudget) {
      transient_layout = std::move(layout);
      return &transient_layout;
    }
    auto entry = layouts.emplace(
        std::move(key), CachedLayout{std::move(layout), bytes, ++stamp});
    layout_bytes += bytes;
    return &entry.first->second.layout;
  }
  void EnsureGPU() {
    if (pipeline)
      return;
    Pipeline program(kVertex, kFragment, QuadVertexLayout());
    pipeline = program.MakeState(
        GetDefaultPrimitiveState(), *GetDepthStencilState(),
        {wgpu::ColorTargetState{.format = kColorTargetFormat,
                                .blend = GetBlendState(BLEND_NORMAL)}});
    wgpu::SamplerDescriptor desc;
    desc.magFilter = wgpu::FilterMode::Linear;
    desc.minFilter = wgpu::FilterMode::Linear;
    sampler = g_device.CreateSampler(&desc);
  }
  std::shared_ptr<Page> NewPage() {
    if (pages.size() >= kMaxAtlasPages) {
      auto victim = std::min_element(
          pages.begin(), pages.end(),
          [](const auto& a, const auto& b) { return a->stamp < b->stamp; });
      const auto id = (*victim)->id;
      std::erase_if(glyphs, [id](const auto& entry) {
        return entry.second.page && entry.second.page->id == id;
      });
      // Never overwrite an evicted texture: deferred/in-flight draw commands
      // may still use its cells. Bind groups keep the retired texture alive.
      pages.erase(victim);
      ++stats.atlas_evictions;
    }
    auto page = std::make_shared<Page>();
    page->id = ++next_page;
    page->stamp = ++stamp;
    page->pixels.resize(kAtlasSize * kAtlasSize);
    wgpu::TextureDescriptor desc;
    desc.size = {kAtlasSize, kAtlasSize, 1};
    desc.format = wgpu::TextureFormat::R8Unorm;
    desc.usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    page->texture = g_device.CreateTexture(&desc);
    page->group = util::CreateBindGroup(
        pipeline.GetBindGroupLayout(2),
        {{0, util::TextureViewSet(page->texture.CreateView(nullptr))},
         {1, util::SamplerSet(sampler)}});
    pages.push_back(page);
    stats.atlas_pages = pages.size();
    return page;
  }
  AtlasGlyph Glyph(const font::Glyph& glyph,
                   int outline,
                   bool solid,
                   int px,
                   int py) {
    GlyphKey key{glyph.face->id,
                 glyph.index,
                 static_cast<uint8_t>(outline),
                 static_cast<uint8_t>(solid),
                 static_cast<uint8_t>(px),
                 static_cast<uint8_t>(py)};
    auto found = glyphs.find(key);
    if (found != glyphs.end()) {
      ++stats.glyph_hits;
      if (found->second.page)
        found->second.page->stamp = ++stamp;
      return found->second;
    }
    auto mask = glyph.face->Rasterize(glyph.index, outline, solid, px / 4.0f,
                                      py / 4.0f);
    ++stats.rasterized_glyphs;
    AtlasGlyph image;
    if (mask.width <= 0 || mask.height <= 0 || mask.width + 2 > kAtlasSize ||
        mask.height + 2 > kAtlasSize ||
        mask.pixels.size() != static_cast<size_t>(mask.width) * mask.height) {
      // Empty glyphs (spaces) need no GPU storage, but still retain advances.
      if (glyphs.size() < 65536)
        glyphs.emplace(key, image);
      return image;
    }
    std::shared_ptr<Page> page;
    int x = 0, y = 0;
    for (auto& candidate : pages)
      if (candidate->Pack(mask.width + 2, mask.height + 2, x, y)) {
        page = candidate;
        break;
      }
    if (!page) {
      page = NewPage();
      page->Pack(mask.width + 2, mask.height + 2, x, y);
    }
    for (int row = 0; row < mask.height; ++row)
      std::copy_n(mask.pixels.data() + static_cast<size_t>(row) * mask.width,
                  mask.width,
                  page->pixels.data() +
                      static_cast<size_t>(y + 1 + row) * kAtlasSize + x + 1);
    page->dirty_left = std::min(page->dirty_left, x);
    page->dirty_top = std::min(page->dirty_top, y);
    page->dirty_right = std::max(page->dirty_right, x + mask.width + 2);
    page->dirty_bottom = std::max(page->dirty_bottom, y + mask.height + 2);
    page->stamp = ++stamp;
    image = {page, x + 1, y + 1, mask.width, mask.height, mask.x, mask.y};
    glyphs.emplace(key, image);
    return image;
  }
  void Upload(const std::shared_ptr<Page>& page) {
    if (!page || page->dirty_right <= page->dirty_left)
      return;
    const int x = page->dirty_left, y = page->dirty_top,
              w = page->dirty_right - x, h = page->dirty_bottom - y;
    wgpu::TexelCopyTextureInfo target;
    target.texture = page->texture;
    target.origin = {static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0};
    wgpu::TexelCopyBufferLayout source;
    source.bytesPerRow = w;
    source.rowsPerImage = h;
    wgpu::Extent3D extent{static_cast<uint32_t>(w), static_cast<uint32_t>(h),
                          1};
    const size_t bytes = static_cast<size_t>(w) * h;
    upload_scratch.resize(bytes);
    for (int row = 0; row < h; ++row)
      std::copy_n(
          page->pixels.data() + static_cast<size_t>(y + row) * kAtlasSize + x,
          w, upload_scratch.data() + static_cast<size_t>(row) * w);
    g_queue.WriteTexture(&target, upload_scratch.data(), bytes, &source,
                         &extent);
    ++stats.upload_calls;
    stats.upload_bytes += static_cast<uint64_t>(w) * h;
    page->dirty_left = page->dirty_top = kAtlasSize;
    page->dirty_right = page->dirty_bottom = 0;
  }
};

FontRenderer::FontRenderer() = default;
FontRenderer::~FontRenderer() {
  const char* diagnostic = SDL_getenv("URGE_FONT_STATS");
  if (impl_ && diagnostic && std::string_view(diagnostic) == "1") {
    const auto& s = impl_->stats;
    LOGGER_INFO(
        "[Font] Cache: layouts={} layout_hits={} glyph_hits={} rasterized={} "
        "uploads={} upload_bytes={} pages={} evictions={} batches={} "
        "vertex_bytes={}",
        s.layouts, s.layout_hits, s.glyph_hits, s.rasterized_glyphs,
        s.upload_calls, s.upload_bytes, s.atlas_pages, s.atlas_evictions,
        s.draw_batches, s.vertex_bytes);
  }
}
FontRenderer::Impl& FontRenderer::State() {
  if (!impl_)
    impl_ = std::make_unique<Impl>();
  return *impl_;
}
FontRenderer::Statistics FontRenderer::GetStatistics() {
  return FontContext::Get().renderer().State().stats;
}
bool FontRenderer::Measure(Font& font,
                           const std::string& text,
                           int32_t* width,
                           int32_t* height) {
  if (text.empty()) {
    if (width)
      *width = 0;
    if (height)
      *height = 0;
    return true;
  }
  auto* layout = FontContext::Get().renderer().State().Layout(font, text);
  if (!layout)
    return false;
  if (width)
    *width = static_cast<int32_t>(std::ceil(layout->width));
  if (height)
    *height = layout->height;
  return true;
}
void FontRenderer::Draw(Bitmap& bitmap,
                        Font& font,
                        const RectI& region,
                        const std::string& text,
                        int32_t align) {
  auto& state = FontContext::Get().renderer().State();
  const auto* layout = state.Layout(font, text);
  if (!layout || layout->glyphs.empty())
    return;
  state.EnsureGPU();
  // RGSS1/2/3 squeeze only the text advance, never the font size or height.
  // Use the same logical metrics as text_size: decoration bearings must not
  // make a string shrink merely because outline/shadow was enabled.
  const float width = std::ceil(layout->width);
  const float height = layout->height;
  const float scale =
      width > region.width ? std::max(0.6f, region.width / width) : 1.0f;
  // Once the minimum scale is reached, clip the remaining text from the
  // leading edge. RGSS does not use negative center/right alignment offsets.
  const float spare_width = std::max(0.0f, region.width - width * scale);
  float left = region.x;
  if (align == 1)
    left += std::floor(spare_width / 2);
  else if (align == 2)
    left += spare_width;
  const float top =
      region.y + std::floor(std::max(0.0f, region.height - height) / 2);
  const auto clip =
      MakeIntersect(region, RectI(0, 0, bitmap.size().x, bitmap.size().y));
  if (clip.width <= 0 || clip.height <= 0)
    return;
  std::vector<Prepared> fill, stroke;
  auto prepare = [&](int outline, std::vector<Prepared>& out) {
    for (const auto& glyph : layout->glyphs) {
      // Anchor fractional glyph origins to the destination pixel grid.
      // Layout advances can be fractional even with whole-pixel alignment.
      const float phase_origin = scale == 1.0f ? left : 0.0f;
      const float gx = glyph.x + phase_origin;
      const float gy = layout->ascent + glyph.y;
      const float x = std::floor(gx), y = std::floor(gy);
      const int px =
          std::clamp(static_cast<int>(std::floor((gx - x) * 4)), 0, 3);
      const int py =
          std::clamp(static_cast<int>(std::floor((gy - y) * 4)), 0, 3);
      auto image = state.Glyph(glyph, outline, font.solid_, px, py);
      if (image.page)
        out.push_back({std::move(image), x - phase_origin, y});
    }
  };
  prepare(0, fill);
  if (font.outline_)
    prepare(1, stroke);
  // Upload at queue flush, coalescing new cells across all text calls.
  BitmapDraw batch;
  uint64_t current_page = 0;
  auto flush = [&] {
    if (!batch.vertices.empty()) {
      state.stats.vertex_bytes += batch.vertices.size() * sizeof(VertexData);
      BitmapQueue::Get().Push(std::move(batch));
      batch = BitmapDraw{};
      ++state.stats.draw_batches;
    }
  };
  const float opacity =
      font.color_ ? std::clamp(font.color_->data.a / 255.0f, 0.0f, 1.0f) : 1.0f;
  auto emit = [&](const std::vector<Prepared>& images, int layer) {
    for (const auto& item : images) {
      const auto& im = item.image;
      const float offset = layer == 0 ? 1 : 0;
      RectF rect(left + (item.x + im.bearing_x + offset) * scale,
                 top + item.y + im.bearing_y + offset, im.width * scale,
                 static_cast<float>(im.height));
      const float x = std::max(rect.x, static_cast<float>(clip.x)),
                  y = std::max(rect.y, static_cast<float>(clip.y));
      const float right = std::min(rect.x + rect.width,
                                   static_cast<float>(clip.x + clip.width)),
                  bottom = std::min(rect.y + rect.height,
                                    static_cast<float>(clip.y + clip.height));
      if (right <= x || bottom <= y)
        continue;
      RectF uv((im.x + (x - rect.x) / scale) / kAtlasSize,
               (im.y + y - rect.y) / kAtlasSize,
               (right - x) / scale / kAtlasSize, (bottom - y) / kAtlasSize);
      glm::vec4 a, b;
      if (layer == 0)
        a = b = glm::vec4(0, 0, 0, opacity);
      else if (layer == 1)
        a = b = PremultipliedColor(font.out_color_, opacity);
      else {
        auto gradient = [&](float yy) {
          auto c = PremultipliedColor(font.color_);
          if (font.gradient_color_ && font.gradient_color_->data.a > 0) {
            const float t =
                std::clamp((yy - top) / std::max(1.0f, height), 0.0f, 1.0f) *
                std::clamp(font.gradient_color_->data.a / 255.0f, 0.0f, 1.0f);
            const glm::vec3 g =
                glm::clamp(glm::vec3(font.gradient_color_->data.r,
                                     font.gradient_color_->data.g,
                                     font.gradient_color_->data.b) /
                               255.0f,
                           glm::vec3(0), glm::vec3(1)) *
                c.a;
            c.r = c.r * (1 - t) + g.r * t;
            c.g = c.g * (1 - t) + g.g * t;
            c.b = c.b * (1 - t) + g.b * t;
          }
          return c;
        };
        a = gradient(y);
        b = gradient(bottom);
      }
      if (a.a == 0 && b.a == 0)
        continue;
      if (current_page != im.page->id) {
        flush();
        current_page = im.page->id;
      }
      batch.target = &bitmap;
      batch.text_group = im.page->group;
      batch.pipeline = state.pipeline;
      if (!batch.prepare)
        batch.prepare = [&state, page = im.page] { state.Upload(page); };
      Quad(batch.vertices, RectF(x, y, right - x, bottom - y), uv, a, b);
    }
  };
  if (font.shadow_)
    emit(font.outline_ ? stroke : fill, 0);
  if (font.outline_)
    emit(stroke, 1);
  emit(fill, 2);
  flush();
}
}  // namespace urge

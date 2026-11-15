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
#include <map>
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
 @location(2) point: vec2<f32>,
 @location(3) @interpolate(flat) layer: i32,
};
@vertex fn vs_main(v: Input)->Output {
 var o: Output;
 o.position=scene.matrix*object.matrix*vec4<f32>(v.position.xy,0,1);
 o.uv=v.uv; o.color=v.color; o.point=v.position.xy;
 o.layer=i32(v.position.z); return o;
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
struct TextClip { rect: vec4<f32>, margin: vec4<f32> };
@group(2) @binding(2) var<uniform> text_clip: TextClip;
fn coverage(uv:vec2<f32>,point:vec2<f32>)->f32 {
 if (any(point < text_clip.rect.xy) || any(point >= text_clip.rect.zw)) { return 0; }
 return textureSampleLevel(atlas,filtering,uv,0).r;
}
@fragment fn fs_main(@location(0) uv:vec2<f32>,@location(1) color:vec4<f32>,
 @location(2) point:vec2<f32>,@location(3) @interpolate(flat) layer:i32)->@location(0) vec4<f32> {
 let edges=layer>>2;
 let draw_layer=layer&3;
 let edge=((edges&1)!=0 && point.x < text_clip.rect.x+text_clip.margin.x) ||
          ((edges&2)!=0 && point.x >= text_clip.rect.z-text_clip.margin.x) ||
          ((edges&4)!=0 && point.y < text_clip.rect.y+text_clip.margin.y) ||
          ((edges&8)!=0 && point.y >= text_clip.rect.w-text_clip.margin.y);
 // Interior glyphs retain their vector outlines. Only the cut edges are
 // rebuilt from clipped coverage, so the outline closes inside the text box.
 if (draw_layer==3) {
  if (!edge) { return vec4<f32>(0); }
  var mask=0.0;
  for (var y=-1; y<=1; y++) {
   for (var x=-1; x<=1; x++) {
    let step=vec2<f32>(f32(x),f32(y));
    mask=max(mask,coverage(uv+step/1024.0,point+step*text_clip.margin.xy));
   }
  }
  return color*mask;
 }
 if (edge && (draw_layer==1 || draw_layer==2)) { return vec4<f32>(0); }
 return color*textureSampleLevel(atlas,filtering,uv,0).r;
}
)",
    "fs_main",
    {{2,
      {{0, wgpu::ShaderStage::Fragment,
        util::TextureLayout::Float(wgpu::TextureViewDimension::e2D)},
       {1, wgpu::ShaderStage::Fragment, util::SamplerLayout::Filtering()},
       {2, wgpu::ShaderStage::Fragment, util::BufferLayout::Uniform(false, 32)}}}}};

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
  int ink_left = 0, ink_right = 0, ink_top = 0, ink_bottom = 0;
};
struct Prepared {
  AtlasGlyph image;
  float x = 0, y = 0;
  RectF ink;
  int edges = 0;
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
          const glm::vec4& bottom,
          int layer) {
  const VertexData a{{rect.x, rect.y, layer, 1}, {uv.x, uv.y}, top};
  const VertexData b{
      {rect.x + rect.width, rect.y, layer, 1}, {uv.x + uv.width, uv.y}, top};
  const VertexData c{
      {rect.x, rect.y + rect.height, layer, 1}, {uv.x, uv.y + uv.height}, bottom};
  const VertexData d{{rect.x + rect.width, rect.y + rect.height, layer, 1},
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
  struct CachedClip {
    wgpu::Buffer buffer;
    std::unordered_map<uint64_t, wgpu::BindGroup> groups;
    uint64_t stamp = 0;
  };
  std::map<std::array<float, 8>, CachedClip> clips;

  CachedClip& Clipping(const std::array<float, 8>& values) {
    auto found = clips.find(values);
    if (found != clips.end()) {
      found->second.stamp = ++stamp;
      return found->second;
    }
    // Repeated Window redraws reuse immutable clip uniforms and bind groups.
    if (clips.size() >= 256) {
      auto victim = std::min_element(clips.begin(), clips.end(),
          [](const auto& a, const auto& b) { return a.second.stamp < b.second.stamp; });
      clips.erase(victim);
    }
    auto& entry = clips[values];
    wgpu::BufferDescriptor desc{};
    desc.size = sizeof(values);
    desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
    entry.buffer = g_device.CreateBuffer(&desc);
    g_queue.WriteBuffer(entry.buffer, 0, values.data(), sizeof(values));
    entry.stamp = ++stamp;
    return entry;
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
      for (auto& [key, clip] : clips)
        clip.groups.erase((*victim)->id);
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
    if (mask.width <= 0 || mask.height <= 0 || mask.width + 4 > kAtlasSize ||
        mask.height + 4 > kAtlasSize ||
        mask.pixels.size() != static_cast<size_t>(mask.width) * mask.height) {
      // Empty glyphs (spaces) need no GPU storage, but still retain advances.
      if (glyphs.size() < 65536)
        glyphs.emplace(key, image);
      return image;
    }
    // Boundary caps sample neighbours outside an expanded glyph quad. Keep
    // two transparent texels around each cell to isolate adjacent glyphs.
    std::shared_ptr<Page> page;
    int x = 0, y = 0;
    for (auto& candidate : pages)
      if (candidate->Pack(mask.width + 4, mask.height + 4, x, y)) {
        page = candidate;
        break;
      }
    if (!page) {
      page = NewPage();
      page->Pack(mask.width + 4, mask.height + 4, x, y);
    }
    for (int row = 0; row < mask.height; ++row)
      std::copy_n(mask.pixels.data() + static_cast<size_t>(row) * mask.width,
                  mask.width,
                  page->pixels.data() +
                      static_cast<size_t>(y + 2 + row) * kAtlasSize + x + 2);
    page->dirty_left = std::min(page->dirty_left, x);
    page->dirty_top = std::min(page->dirty_top, y);
    page->dirty_right = std::max(page->dirty_right, x + mask.width + 4);
    page->dirty_bottom = std::max(page->dirty_bottom, y + mask.height + 4);
    page->stamp = ++stamp;
    image = {page, x + 2, y + 2, mask.width, mask.height, mask.x, mask.y};
    image.ink_left = mask.width;
    image.ink_top = mask.height;
    for (int row = 0; row < mask.height; ++row)
      for (int col = 0; col < mask.width; ++col)
        if (mask.pixels[static_cast<size_t>(row) * mask.width + col]) {
          image.ink_left = std::min(image.ink_left, col);
          image.ink_right = std::max(image.ink_right, col + 1);
          image.ink_top = std::min(image.ink_top, row);
          image.ink_bottom = std::max(image.ink_bottom, row + 1);
        }
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
  // Raster phase is unchanged by whole-pixel alignment. Compute decoration
  // bounds from the prepared glyphs, keeping advance-only squeeze/text_size.
  float left = region.x;
  const float top =
      region.y + std::floor(std::max(0.0f, region.height - height) / 2);
  const auto clip =
      MakeIntersect(region, RectI(0, 0, bitmap.size().x, bitmap.size().y));
  if (clip.width <= 0 || clip.height <= 0)
    return;
  const std::array<float, 8> clipping = {
      static_cast<float>(clip.x), static_cast<float>(clip.y),
      static_cast<float>(clip.x + clip.width), static_cast<float>(clip.y + clip.height),
      scale, 1.0f, 0, 0};
  auto& clip_state = state.Clipping(clipping);
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
      auto body = state.Glyph(glyph, 0, font.solid_, px, py);
      auto image = outline ? state.Glyph(glyph, outline, font.solid_, px, py) : body;
      if (image.page) {
        RectF ink(x - phase_origin + body.bearing_x + body.ink_left,
                  y + body.bearing_y + body.ink_top,
                  body.ink_right - body.ink_left, body.ink_bottom - body.ink_top);
        out.push_back({std::move(image), x - phase_origin, y, ink});
      }
    }
  };
  prepare(0, fill);
  if (font.outline_)
    prepare(1, stroke);
  float ink_left = 0, ink_right = width;
  for (const auto& item : fill) {
    ink_left = std::min(ink_left, item.x + item.image.bearing_x + item.image.ink_left);
    ink_right = std::max(ink_right, item.x + item.image.bearing_x + item.image.ink_right);
  }
  // Reserve the outline on both sides, including for left alignment. Negative
  // bearings and italic overhang belong to the rendered box, not its advance.
  const float padding = font.outline_ ? 1.0f : 0.0f;
  const float shadow = font.shadow_ ? 1.0f : 0.0f;
  const float decorated_width = (ink_right - ink_left + 2 * padding + shadow) * scale;
  const float spare_width = std::max(0.0f, region.width - decorated_width);
  left += (padding - ink_left) * scale;
  if (align == 1)
    left += std::floor(spare_width / 2);
  else if (align == 2)
    left += spare_width;
  // A stroke merely touching the box is still a complete outline. Rebuild
  // only edges touched by the body, never a normally padded glyph's border.
  for (auto* list : {&fill, &stroke})
    for (auto& item : *list) {
      const float x0 = left + item.ink.x * scale;
      const float x1 = x0 + item.ink.width * scale;
      const float y0 = top + item.ink.y;
      const float y1 = y0 + item.ink.height;
      item.edges = (x0 < clip.x + scale ? 1 : 0) |
                   (x1 > clip.x + clip.width - scale ? 2 : 0) |
                   (y0 < clip.y + 1 ? 4 : 0) |
                   (y1 > clip.y + clip.height - 1 ? 8 : 0);
    }
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
      auto im = item.image;
      if (layer == 3) {
        if (!item.edges)
          continue;
        --im.x; --im.y; --im.bearing_x; --im.bearing_y;
        im.width += 2; im.height += 2;
      }
      const float offset = layer == 0 ? 1 : 0;
      RectF rect(left + (item.x + im.bearing_x + offset) * scale,
                 top + item.y + im.bearing_y + offset, im.width * scale,
                 static_cast<float>(im.height));
      // Interior glyphs need no extra cap geometry or atlas sampling pass.
      if (layer == 3 && rect.x >= clip.x + scale && rect.y >= clip.y + 1 &&
          rect.x + rect.width <= clip.x + clip.width - scale &&
          rect.y + rect.height <= clip.y + clip.height - 1)
        continue;
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
      else if (layer == 1 || layer == 3)
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
      auto& group = clip_state.groups[im.page->id];
      if (!group)
        group = util::CreateBindGroup(
            state.pipeline.GetBindGroupLayout(2),
            {{0, util::TextureViewSet(im.page->texture.CreateView(nullptr))},
             {1, util::SamplerSet(state.sampler)},
             {2, util::BufferSet(clip_state.buffer)}});
      batch.text_group = group;
      batch.pipeline = state.pipeline;
      if (!batch.prepare)
        batch.prepare = [&state, page = im.page] { state.Upload(page); };
      Quad(batch.vertices, RectF(x, y, right - x, bottom - y), uv, a, b,
           (layer == 2 && !font.outline_ ? 0 : layer) | (item.edges << 2));
    }
  };
  if (font.shadow_)
    emit(font.outline_ ? stroke : fill, 0);
  if (font.outline_) {
    emit(stroke, 1);
    emit(fill, 3);
  }
  emit(fill, 2);
  flush();
}
}  // namespace urge

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

#include "core/font_renderer.h"

#include <algorithm>
#include <string>
#include <utility>

#include "core/gpu_utils.h"
#include "core/pipeline.h"

namespace urge {

namespace {

constexpr std::size_t kTextRunBudget = 8 * 1024 * 1024;

std::size_t TextRunBytes(const glm::ivec2& size) {
  return static_cast<std::size_t>(size.x) * static_cast<std::size_t>(size.y) *
         4;
}

std::string ChannelKey(float value) {
  return std::to_string(static_cast<int>(std::clamp(value, 0.0f, 255.0f)));
}

std::string ColorKey(const RefPtr<Color>& color) {
  if (!color)
    return "255,255,255,255";

  return ChannelKey(color->data.r) + "," + ChannelKey(color->data.g) + "," +
         ChannelKey(color->data.b) + "," + ChannelKey(color->data.a);
}

std::string MakeTextRunKey(const Font& font, const std::string& text) {
  std::string key;
  for (const std::string& name : font.name_) {
    key += name;
    key += '\x1f';
  }

  key += '\x1e';
  key += std::to_string(font.size_);
  key += font.bold_ ? 'b' : '-';
  key += font.italic_ ? 'i' : '-';
  key += font.outline_ ? 'o' : '-';
  key += font.shadow_ ? 's' : '-';
  key += font.solid_ ? 'l' : '-';
  key += '\x1e';
  key += ColorKey(font.color_);
  key += '\x1f';
  key += ColorKey(font.out_color_);
  key += '\x1f';
  key += ColorKey(font.gradient_color_);
  key += '\x1e';
  key += text;
  return key;
}

std::pair<wgpu::Texture, wgpu::TextureView> CreateTextTexture(
    SDL_Surface* surface) {
  if (surface->w <= 0 || surface->h <= 0)
    return {nullptr, nullptr};

  SDL_PremultiplySurfaceAlpha(surface, false);

  wgpu::TextureDescriptor desc;
  desc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
  desc.dimension = wgpu::TextureDimension::e2D;
  desc.size.width = surface->w;
  desc.size.height = surface->h;
  desc.format = wgpu::TextureFormat::RGBA8Unorm;

  wgpu::Texture texture = g_device.CreateTexture(&desc);
  if (!texture)
    return {nullptr, nullptr};

  wgpu::TexelCopyTextureInfo destination;
  destination.texture = texture;

  wgpu::TexelCopyBufferLayout layout;
  layout.bytesPerRow = surface->pitch;
  layout.rowsPerImage = surface->h;

  wgpu::Extent3D size;
  size.width = surface->w;
  size.height = surface->h;

  g_queue.WriteTexture(&destination, surface->pixels,
                       surface->pitch * surface->h, &layout, &size);

  return {texture, texture.CreateView(nullptr)};
}

}  // namespace

FontRenderer::TextRun* FontRenderer::Acquire(Font& font,
                                             const std::string& text) {
  const std::string key = MakeTextRunKey(font, text);
  auto found = runs_.find(key);
  if (found != runs_.end()) {
    found->second.stamp = ++stamp_;
    return &found->second;
  }

  uint8_t font_opacity = 255;
  SDL_Surface* text_surface = font.RenderText(text, &font_opacity);
  if (!text_surface)
    return nullptr;

  auto [text_texture, text_view] = CreateTextTexture(text_surface);
  const glm::ivec2 text_size{text_surface->w, text_surface->h};
  SDL_DestroySurface(text_surface);

  if (!text_texture || text_size.x <= 0 || text_size.y <= 0)
    return nullptr;

  if (!sampler_) {
    wgpu::SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_desc.magFilter = wgpu::FilterMode::Linear;
    sampler_desc.minFilter = wgpu::FilterMode::Linear;
    sampler_ = g_device.CreateSampler(&sampler_desc);
  }

  auto pipeline = ShaderSet::Get().state.bitmap.texture_pma;
  wgpu::BindGroup group = util::CreateBindGroup(
      pipeline.GetBindGroupLayout(2),
      {{0, util::TextureViewSet(text_view)}, {1, util::SamplerSet(sampler_)}});

  return Store(key, text_texture, group, text_size, font_opacity);
}

FontRenderer::TextRun* FontRenderer::Store(std::string key,
                                           wgpu::Texture texture,
                                           wgpu::BindGroup group,
                                           const glm::ivec2& size,
                                           std::uint8_t opacity) {
  const std::size_t bytes = TextRunBytes(size);

  while (!runs_.empty() && bytes_ + bytes > kTextRunBudget) {
    auto victim = runs_.begin();
    for (auto current = runs_.begin(); current != runs_.end(); ++current)
      if (current->second.stamp < victim->second.stamp)
        victim = current;

    bytes_ -= TextRunBytes(victim->second.size);
    runs_.erase(victim);
  }

  TextRun run;
  run.texture = texture;
  run.group = group;
  run.size = size;
  run.opacity = opacity;
  run.stamp = ++stamp_;

  auto entry = runs_.emplace(std::move(key), std::move(run));
  bytes_ += bytes;
  return &entry.first->second;
}

}  // namespace urge

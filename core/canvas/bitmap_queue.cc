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

#include "core/canvas/bitmap_queue.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "SDL3/SDL_timer.h"

#include "core/graphics/gpu.h"
#include "core/render/pipeline.h"
#include "core/render/render_stats.h"
#include "core/render/shader.h"

namespace urge {

namespace {

constexpr std::size_t kInitialVertexCapacity = 8192;

}

BitmapQueue::BitmapQueue() {
  vertices_.Reserve(kInitialVertexCapacity);
  filter_vertices_.Reserve(kQuadVertexCount * 2);
}

BitmapQueue::~BitmapQueue() {
  Flush();
  vertices_.Reset();
  filter_vertices_.Reset();
}

void BitmapQueue::Push(BitmapDraw draw) {
  draws_.push_back(std::move(draw));
}

void BitmapQueue::OpenPass(const RefPtr<Bitmap>& target) {
  if (!encoder_)
    encoder_ = g_device.CreateCommandEncoder(nullptr);

  if (pass_ && target_ == target)
    return;

  ClosePass();

  target_ = target;
  pass_ = target->BeginRendering(encoder_);
}

void BitmapQueue::ClosePass() {
  if (!pass_)
    return;

  pass_.End();
  pass_ = nullptr;
}

void BitmapQueue::Record(const BitmapDraw& draw, std::uint32_t first_vertex) {
  const bool masked = draw.kind == BitmapDraw::Kind::kTextureMask;

  OpenPass(draw.target);

  pass_.SetPipeline(draw.kind == BitmapDraw::Kind::kColor
                        ? ShaderSet::Get().state.bitmap.color_noblend
                        : (masked
                               ? ShaderSet::Get().state.bitmap.texture_mask_pma
                               : ShaderSet::Get().state.bitmap.texture_pma));
  pass_.SetBindGroup(0, draw.target->scene_group(), 0, nullptr);
  pass_.SetBindGroup(1, draw.target->object_group(), 0, nullptr);

  if (draw.kind != BitmapDraw::Kind::kColor) {
    pass_.SetBindGroup(
        2, draw.text_group ? draw.text_group : draw.source->texture_group(), 0,
        nullptr);
    if (masked)
      pass_.SetBindGroup(3, draw.mask->texture_group(), 0, nullptr);
  }

  pass_.SetVertexBuffer(0, vertices_.buffer(), 0, WGPU_WHOLE_SIZE);
  pass_.Draw(kQuadVertexCount, 1, first_vertex, 0);
}

void BitmapQueue::Flush() {
  if (draws_.empty() || executing_)
    return;

  std::vector<BitmapDraw> draws;
  draws.swap(draws_);

  for (const BitmapDraw& draw : draws) {
    if (draw.target)
      draw.target->TouchContent();
  }

  executing_ = true;

  const std::uint64_t begin = SDL_GetPerformanceCounter();

  for (const BitmapDraw& draw : draws) {
    vertices_.EmitQuad(draw.rect, draw.texcoord, draw.colors[0], draw.colors[1],
                       draw.colors[2], draw.colors[3]);
    vertices_.End();
  }
  vertices_.Upload();

  const std::uint64_t uploaded = SDL_GetPerformanceCounter();

  for (std::size_t index = 0; index < draws.size(); ++index)
    Record(draws[index], static_cast<std::uint32_t>(index) * kQuadVertexCount);

  ClosePass();

  const std::uint64_t recorded = SDL_GetPerformanceCounter();
  auto command = encoder_.Finish(nullptr);
  const std::uint64_t finished = SDL_GetPerformanceCounter();
  encoder_ = nullptr;
  target_ = nullptr;

  CountSubmit(true);
  g_queue.Submit(1, &command);
  const std::uint64_t submitted = SDL_GetPerformanceCounter();

  RenderFrameCounters& counters = GetRenderStats().current;
  counters.bitmap_ops += static_cast<std::uint32_t>(draws.size());
  counters.bitmap_upload_ms += TicksToMilliseconds(uploaded - begin);
  counters.bitmap_record_ms += TicksToMilliseconds(recorded - uploaded);
  counters.bitmap_finish_ms += TicksToMilliseconds(finished - recorded);
  counters.bitmap_submit_ms += TicksToMilliseconds(submitted - finished);

  executing_ = false;
}

void BitmapQueue::EnsureFilterResources(const glm::ivec2& size) {
  if (!filter_sampler_) {
    wgpu::SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_desc.magFilter = wgpu::FilterMode::Linear;
    sampler_desc.minFilter = wgpu::FilterMode::Linear;
    filter_sampler_ = g_device.CreateSampler(&sampler_desc);
  }

  if (!filter_texture_ || filter_size_.x != size.x ||
      filter_size_.y != size.y) {
    wgpu::TextureDescriptor texture_desc;
    texture_desc.usage = wgpu::TextureUsage::RenderAttachment |
                         wgpu::TextureUsage::TextureBinding |
                         wgpu::TextureUsage::CopySrc |
                         wgpu::TextureUsage::CopyDst;
    texture_desc.dimension = wgpu::TextureDimension::e2D;
    texture_desc.size.width = static_cast<std::uint32_t>(size.x);
    texture_desc.size.height = static_cast<std::uint32_t>(size.y);
    texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    filter_texture_ = g_device.CreateTexture(&texture_desc);
    filter_view_ = filter_texture_.CreateView(nullptr);
    filter_size_ = size;
  }
}

wgpu::BindGroup BitmapQueue::MakeFilterSourceGroup(
    const wgpu::RenderPipeline& pipeline, wgpu::TextureView view) {
  return util::CreateBindGroup(
      pipeline.GetBindGroupLayout(2),
      {{0, util::TextureViewSet(view)}, {1, util::SamplerSet(filter_sampler_)}});
}

void BitmapQueue::DrawFilterQuad(wgpu::CommandEncoder encoder,
                                 const wgpu::RenderPipeline& pipeline,
                                 wgpu::TextureView destination,
                                 wgpu::BindGroup scene_group,
                                 wgpu::BindGroup object_group,
                                 wgpu::BindGroup source_group,
                                 std::uint32_t first_vertex) {
  wgpu::RenderPassColorAttachment color_attachment = {
      .view = destination,
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = wgpu::Color{0.0, 0.0, 0.0, 0.0},
  };
  wgpu::RenderPassDescriptor pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
  };

  auto pass = encoder.BeginRenderPass(&pass_desc);
  pass.SetPipeline(pipeline);
  pass.SetBindGroup(0, scene_group, 0, nullptr);
  pass.SetBindGroup(1, object_group, 0, nullptr);
  pass.SetBindGroup(2, source_group, 0, nullptr);
  pass.SetVertexBuffer(0, filter_vertices_.buffer(), 0, WGPU_WHOLE_SIZE);
  pass.Draw(kQuadVertexCount, 1, first_vertex, 0);
  pass.End();
}

void BitmapQueue::SubmitFilter(wgpu::CommandEncoder encoder, Bitmap& target) {
  auto command = encoder.Finish(nullptr);
  CountSubmit(false);
  g_queue.Submit(1, &command);
  target.TouchContent();
}

void BitmapQueue::ApplyHue(Bitmap& target, float hue) {
  Flush();

  const glm::ivec2 size = target.size();
  if (size.x <= 0 || size.y <= 0)
    return;
  EnsureFilterResources(size);

  const auto& pipeline = ShaderSet::Get().state.filter.hue;
  const auto scene = target.scene_group();
  const auto object = target.object_group();
  const auto target_view = target.texture_view();
  const auto target_texture = target.texture();
  const auto source = MakeFilterSourceGroup(pipeline, target_view);

  filter_vertices_.Clear();
  filter_vertices_.EmitQuad(RectF(0.0f, 0.0f, static_cast<float>(size.x),
                                  static_cast<float>(size.y)),
                            RectF(0.0f, 0.0f, 1.0f, 1.0f),
                            glm::vec4(hue, 0.0f, 0.0f, 1.0f));
  filter_vertices_.End();
  filter_vertices_.Upload();

  auto encoder = g_device.CreateCommandEncoder(nullptr);
  DrawFilterQuad(encoder, pipeline, filter_view_, scene, object, source, 0);

  wgpu::TexelCopyTextureInfo source_info;
  source_info.texture = filter_texture_;
  wgpu::TexelCopyTextureInfo destination_info;
  destination_info.texture = target_texture;
  wgpu::Extent3D extent{static_cast<std::uint32_t>(size.x),
                        static_cast<std::uint32_t>(size.y), 1};
  encoder.CopyTextureToTexture(&source_info, &destination_info, &extent);

  SubmitFilter(encoder, target);
}

void BitmapQueue::ApplyBlur(Bitmap& target) {
  Flush();

  const glm::ivec2 size = target.size();
  if (size.x <= 0 || size.y <= 0)
    return;
  EnsureFilterResources(size);

  const auto& pipeline = ShaderSet::Get().state.filter.blur;
  const auto scene = target.scene_group();
  const auto object = target.object_group();
  const auto target_view = target.texture_view();
  const auto bitmap_source = MakeFilterSourceGroup(pipeline, target_view);
  const auto temp_source = MakeFilterSourceGroup(pipeline, filter_view_);

  const RectF rect(0.0f, 0.0f, static_cast<float>(size.x),
                   static_cast<float>(size.y));
  const RectF texcoord(0.0f, 0.0f, 1.0f, 1.0f);

  filter_vertices_.Clear();
  filter_vertices_.EmitQuad(rect, texcoord, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
  filter_vertices_.End();
  filter_vertices_.EmitQuad(rect, texcoord, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
  filter_vertices_.End();
  filter_vertices_.Upload();

  auto encoder = g_device.CreateCommandEncoder(nullptr);
  DrawFilterQuad(encoder, pipeline, filter_view_, scene, object, bitmap_source,
                 0);
  DrawFilterQuad(encoder, pipeline, target_view, scene, object, temp_source,
                 static_cast<std::uint32_t>(kQuadVertexCount));

  SubmitFilter(encoder, target);
}

void BitmapQueue::ApplyRadialBlur(Bitmap& target,
                                  float angle,
                                  float division) {
  Flush();

  const glm::ivec2 size = target.size();
  if (size.x <= 0 || size.y <= 0)
    return;
  EnsureFilterResources(size);

  const auto& pipeline = ShaderSet::Get().state.filter.radial;
  const auto scene = target.scene_group();
  const auto object = target.object_group();
  const auto target_view = target.texture_view();
  const auto target_texture = target.texture();
  const auto source = MakeFilterSourceGroup(pipeline, target_view);

  filter_vertices_.Clear();
  filter_vertices_.EmitQuad(RectF(0.0f, 0.0f, static_cast<float>(size.x),
                                  static_cast<float>(size.y)),
                            RectF(0.0f, 0.0f, 1.0f, 1.0f),
                            glm::vec4(angle, division, 0.0f, 1.0f));
  filter_vertices_.End();
  filter_vertices_.Upload();

  auto encoder = g_device.CreateCommandEncoder(nullptr);
  DrawFilterQuad(encoder, pipeline, filter_view_, scene, object, source, 0);

  wgpu::TexelCopyTextureInfo source_info;
  source_info.texture = filter_texture_;
  wgpu::TexelCopyTextureInfo destination_info;
  destination_info.texture = target_texture;
  wgpu::Extent3D extent{static_cast<std::uint32_t>(size.x),
                        static_cast<std::uint32_t>(size.y), 1};
  encoder.CopyTextureToTexture(&source_info, &destination_info, &extent);

  SubmitFilter(encoder, target);
}

}

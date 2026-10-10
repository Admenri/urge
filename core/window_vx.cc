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

#include "core/window_vx.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "glm/gtc/matrix_transform.hpp"

#include "core/config.h"
#include "core/device.h"
#include "core/gpu_utils.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

constexpr int32_t kPauseIndexTable[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

constexpr int32_t kCursorAlphaTable[] = {
    255, 247, 239, 231, 223, 215, 207, 199, 191, 183, 175, 167, 159, 151,
    143, 135, 127, 119, 111, 103, 95,  103, 111, 119, 127, 135, 143, 151,
    159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239, 247,
};

constexpr int32_t kSliceSize = 8;

constexpr int32_t kCellSize = 32;

}  // namespace

WindowVX::WindowVX(int32_t x, int32_t y, int32_t width, int32_t height)
    : Node(nullptr, ZValue(100, std::numeric_limits<int32_t>::max())),
      contents_(MakeRefCounted<Bitmap>(1, 1)),
      cursor_rect_(MakeRefCounted<Rect>()),
      x_(x),
      y_(y),
      width_(width),
      height_(height),
      padding_(Config::Get().vxa() ? 12 : 16),
      padding_bottom_(padding_),
      tone_(MakeRefCounted<Tone>()),
      rgss3_style_(Config::Get().vxa()) {
  Node::SetupTrait(this);
  CreateTintBinding();
}

WindowVX::WindowVX(RefPtr<Viewport> viewport) : WindowVX(0, 0, 0, 0) {
  Attr_Viewport(viewport);
}

WindowVX::~WindowVX() {
  Disposable::Dispose();
}

void WindowVX::Update() {
  if (pause_) {
    pause_index_ += 1;
    pause_index_ =
        pause_index_ % static_cast<int32_t>(std::size(kPauseIndexTable));
  }

  if (active_) {
    cursor_index_ += 1;
    cursor_index_ =
        cursor_index_ % static_cast<int32_t>(std::size(kCursorAlphaTable));
  }
}

void WindowVX::Move(int32_t x, int32_t y, int32_t width, int32_t height) {
  x_ = x;
  y_ = y;
  width_ = width;
  height_ = height;
}

bool WindowVX::Opened() {
  return openness_ == 255;
}

bool WindowVX::Closed() {
  return openness_ == 0;
}

ATTR_DEF(WindowVX, RefPtr<Viewport>, Viewport) {
  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(WindowVX, RefPtr<Bitmap>, Windowskin) {
  if (value.has_value()) {
    window_skin_ = *value;
    return std::nullopt;
  } else {
    return window_skin_;
  }
}

ATTR_DEF(WindowVX, RefPtr<Bitmap>, Contents) {
  if (value.has_value()) {
    contents_ = *value;
    return std::nullopt;
  } else {
    return contents_;
  }
}

ATTR_DEF(WindowVX, RefPtr<Rect>, CursorRect) {
  if (value.has_value()) {
    cursor_rect_->Set(*value);
    return std::nullopt;
  } else {
    return cursor_rect_;
  }
}

ATTR_DEF(WindowVX, bool, Active) {
  if (value.has_value()) {
    active_ = *value;
    return std::nullopt;
  } else {
    return active_;
  }
}

ATTR_DEF(WindowVX, bool, ArrowsVisible) {
  if (value.has_value()) {
    arrows_visible_ = *value;
    return std::nullopt;
  } else {
    return arrows_visible_;
  }
}

ATTR_DEF(WindowVX, bool, Pause) {
  if (value.has_value()) {
    pause_ = *value;
    return std::nullopt;
  } else {
    return pause_;
  }
}

ATTR_DEF(WindowVX, int32_t, X) {
  if (value.has_value()) {
    x_ = *value;
    return std::nullopt;
  } else {
    return x_;
  }
}

ATTR_DEF(WindowVX, int32_t, Y) {
  if (value.has_value()) {
    y_ = *value;
    return std::nullopt;
  } else {
    return y_;
  }
}

ATTR_DEF(WindowVX, int32_t, Width) {
  if (value.has_value()) {
    width_ = *value;
    return std::nullopt;
  } else {
    return width_;
  }
}

ATTR_DEF(WindowVX, int32_t, Height) {
  if (value.has_value()) {
    height_ = *value;
    return std::nullopt;
  } else {
    return height_;
  }
}

ATTR_DEF(WindowVX, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}

ATTR_DEF(WindowVX, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}

ATTR_DEF(WindowVX, int32_t, Padding) {
  if (value.has_value()) {
    padding_ = *value;
    padding_bottom_ = *value;
    return std::nullopt;
  } else {
    return padding_;
  }
}

ATTR_DEF(WindowVX, int32_t, PaddingBottom) {
  if (value.has_value()) {
    padding_bottom_ = *value;
    return std::nullopt;
  } else {
    return padding_bottom_;
  }
}

ATTR_DEF(WindowVX, int32_t, Opacity) {
  if (value.has_value()) {
    opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return opacity_;
  }
}

ATTR_DEF(WindowVX, int32_t, BackOpacity) {
  if (value.has_value()) {
    back_opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return back_opacity_;
  }
}

ATTR_DEF(WindowVX, int32_t, ContentsOpacity) {
  if (value.has_value()) {
    contents_opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return contents_opacity_;
  }
}

ATTR_DEF(WindowVX, int32_t, Openness) {
  if (value.has_value()) {
    openness_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return openness_;
  }
}

ATTR_DEF(WindowVX, RefPtr<Tone>, Tone) {
  if (value.has_value()) {
    tone_->Set(*value);
    return std::nullopt;
  } else {
    return tone_;
  }
}

void WindowVX::DisposeObject() {
  Node::DisposeObject();

  window_skin_.reset();
  contents_.reset();
}

bool WindowVX::Prepare(DrawParam param) {
  background_slot_ = {};
  ground_slot_ = {};
  stencil_clear_slot_ = {};
  stencil_slot_ = {};
  clipped_slot_ = {};

  if (!Disposable::Check(window_skin_))
    return false;

  const glm::mat4 transform =
      world_transform() *
      glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x_),
                                                static_cast<float>(y_), 0.0f));

  const ObjectData object_data = {transform};
  object_slot_ = UniformManager::Get().object_uniforms().Acquire(object_data);
  if (object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return false;

  PrimitiveEmitter& emitter = *param->vertices;

  const auto capture = [&](const std::size_t first) {
    return PrimitiveEmitter::Slot{
        static_cast<std::uint32_t>(first),
        static_cast<std::uint32_t>(emitter.size() - first)};
  };

  const std::size_t background_first = emitter.size();
  EmitBackgroundInternal(emitter);
  background_slot_ = capture(background_first);

  const std::size_t ground_first = emitter.size();
  EmitGroundInternal(emitter);
  ground_slot_ = capture(ground_first);

  const std::size_t stencil_clear_first = emitter.size();
  EmitStencilClearInternal(emitter, param->target->size());
  stencil_clear_slot_ = capture(stencil_clear_first);

  const std::size_t stencil_first = emitter.size();
  EmitStencilInternal(emitter);
  stencil_slot_ = capture(stencil_first);

  const std::size_t cursor_first = emitter.size();
  EmitCursorInternal(emitter);
  cursor_slot_ = capture(cursor_first);

  const std::size_t contents_first = emitter.size();
  EmitContentsInternal(emitter);
  contents_slot_ = capture(contents_first);

  clipped_slot_ = PrimitiveEmitter::Slot{
      static_cast<std::uint32_t>(cursor_first),
      static_cast<std::uint32_t>(emitter.size() - cursor_first)};

  return true;
}

bool WindowVX::DoDraw(DrawParam param) {
  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);

  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);

  if (background_slot_.count) {
    TintBase::TintParam tint = {};
    tint.blend_tone = tone_->Normalize();
    GPUDevice::Get().queue().WriteBuffer(tint_uniform_, 0, &tint, sizeof(tint));

    param->pass.SetPipeline(
        ShaderSet::Get().state.window.tint_blends.at(BLEND_NORMAL));
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
    param->pass.Draw(background_slot_.count, 1, background_slot_.first, 0);
  }

  if (ground_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.window.texture_dynamic_pma);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.Draw(ground_slot_.count, 1, ground_slot_.first, 0);
  }

  if (stencil_clear_slot_.count) {
    param->pass.SetPipeline(
        ShaderSet::Get().state.window.texture_stencil_write);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.SetStencilReference(kStencilClear);
    param->pass.Draw(stencil_clear_slot_.count, 1, stencil_clear_slot_.first,
                     0);
  }

  if (stencil_slot_.count) {
    param->pass.SetPipeline(
        ShaderSet::Get().state.window.texture_stencil_write);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.SetStencilReference(kStencilReference);
    param->pass.Draw(stencil_slot_.count, 1, stencil_slot_.first, 0);
  }

  if (clipped_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.window.texture_stencil_test);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetStencilReference(kStencilReference);

    if (cursor_slot_.count) {
      param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
      param->pass.Draw(cursor_slot_.count, 1, cursor_slot_.first, 0);
    }

    if (contents_slot_.count && Disposable::Check(contents_)) {
      param->pass.SetBindGroup(2, contents_->texture_group(), 0, nullptr);
      param->pass.Draw(contents_slot_.count, 1, contents_slot_.first, 0);
    }
  }

  return false;
}

void WindowVX::EmitGroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const int32_t cell = kCellSize * scale_;
  const int32_t unit = kSliceSize * scale_;

  const RectI background_tile_source(0, cell, cell, cell);

  const RectI frame_source(32 * scale_, 0, unit * 4, unit * 4);

  const RectI background_dest(scale_, scale_, width_ - 2 * scale_,
                              height_ - 2 * scale_);
  const RectI frame_dest(0, 0, width_, height_);

  const float openness = static_cast<float>(openness_) / 255.0f;
  const float center_y = static_cast<float>(height_) / 2.0f;

  const auto apply_openness = [&](const RectI& rect) -> RectI {
    const float top = static_cast<float>(rect.y);
    const float bottom = top + static_cast<float>(rect.height);
    const float scaled_top = center_y + (top - center_y) * openness;
    const float scaled_bottom = center_y + (bottom - center_y) * openness;
    return RectI(rect.x, static_cast<int32_t>(scaled_top), rect.width,
                 static_cast<int32_t>(scaled_bottom - scaled_top));
  };

  const glm::vec4 background_color =
      glm::vec4(static_cast<float>(opacity_) / 255.0f *
                static_cast<float>(back_opacity_) / 255.0f);
  EmitTiledInternal(emitter, background_tile_source,
                    apply_openness(background_dest), background_color);

  const glm::vec4 frame_color(static_cast<float>(opacity_) / 255.0f);
  EmitNineSliceInternal(emitter, frame_source, apply_openness(frame_dest), unit,
                        frame_color, false);

  if (openness_ != 255)
    return;

  const RectI region = ContentRectInternal();

  const int32_t arrow_x = (width_ - unit) / 2;
  const int32_t arrow_y = (height_ - unit) / 2;

  if (arrows_visible_ && Disposable::Check(contents_)) {
    const glm::ivec2 contents_size = contents_->size();

    const RectI up_src(44 * scale_, 8 * scale_, unit, unit / 2);
    const RectI down_src(44 * scale_, 20 * scale_, unit, unit / 2);
    const RectI left_src(40 * scale_, 12 * scale_, unit / 2, unit);
    const RectI right_src(52 * scale_, 12 * scale_, unit / 2, unit);

    if (ox_ > 0)
      EmitSliceInternal(emitter, left_src,
                        RectI(2 * scale_, arrow_y, unit / 2, unit),
                        glm::vec4(1.0f));
    if (oy_ > 0)
      EmitSliceInternal(emitter, up_src,
                        RectI(arrow_x, 2 * scale_, unit, unit / 2),
                        glm::vec4(1.0f));
    if (region.width < contents_size.x - ox_)
      EmitSliceInternal(emitter, right_src,
                        RectI(width_ - 6 * scale_, arrow_y, unit / 2, unit),
                        glm::vec4(1.0f));
    if (region.height < contents_size.y - oy_)
      EmitSliceInternal(emitter, down_src,
                        RectI(arrow_x, height_ - 6 * scale_, unit, unit / 2),
                        glm::vec4(1.0f));
  }

  if (pause_) {
    const int32_t frame = kPauseIndexTable[pause_index_];
    const RectI pause_src((48 + (frame % 2) * 8) * scale_,
                          (32 + (frame / 2) * 8) * scale_, unit, unit);
    const RectI pause_dest(arrow_x, height_ - unit, unit, unit);

    EmitSliceInternal(emitter, pause_src, pause_dest, glm::vec4(1.0f));
  }
}

void WindowVX::EmitBackgroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const int32_t cell = kCellSize * scale_;

  const RectI background_source(0, 0, cell, cell);

  const RectI background_dest(scale_, scale_, width_ - 2 * scale_,
                              height_ - 2 * scale_);

  const float openness = static_cast<float>(openness_) / 255.0f;
  const float center_y = static_cast<float>(height_) / 2.0f;

  const float top = static_cast<float>(background_dest.y);
  const float bottom = top + static_cast<float>(background_dest.height);
  const float scaled_top = center_y + (top - center_y) * openness;
  const float scaled_bottom = center_y + (bottom - center_y) * openness;

  const RectI dest(background_dest.x, static_cast<int32_t>(scaled_top),
                   background_dest.width,
                   static_cast<int32_t>(scaled_bottom - scaled_top));

  const glm::vec4 background_color =
      glm::vec4(static_cast<float>(opacity_) / 255.0f *
                static_cast<float>(back_opacity_) / 255.0f);

  EmitSliceInternal(emitter, background_source, dest, background_color);
}

void WindowVX::EmitStencilClearInternal(PrimitiveEmitter& emitter,
                                        glm::ivec2 size) {
  emitter.EmitQuad(
      RectI(-x_, -y_, size.x, size.y),
      MakeNorm(RectF(0.0f, 0.0f, 1.0f, 1.0f), window_skin_->size()),
      glm::vec4(1.0f));
  emitter.End();
}

void WindowVX::EmitStencilInternal(PrimitiveEmitter& emitter) {
  if (openness_ != 255)
    return;

  const RectI region = ContentRectInternal();
  if (!region())
    return;

  const bool has_cursor =
      cursor_rect_->data.width > 0 && cursor_rect_->data.height > 0;
  if (!has_cursor && !Disposable::Check(contents_))
    return;

  emitter.EmitQuad(
      RectF(region),
      MakeNorm(RectF(0.0f, 0.0f, 1.0f, 1.0f), window_skin_->size()),
      glm::vec4(1.0f));
  emitter.End();
}

void WindowVX::EmitCursorInternal(PrimitiveEmitter& emitter) {
  if (openness_ != 255)
    return;

  const RectI cursor = cursor_rect_->data;
  if (cursor.width <= 0 || cursor.height <= 0)
    return;

  const RectI region = ContentRectInternal();
  const int32_t unit = scale_ >= 4 ? scale_ * 2 : 4;
  const int32_t alpha = kCursorAlphaTable[cursor_index_];
  const glm::vec4 color(static_cast<float>(contents_opacity_) / 255.0f *
                        static_cast<float>(alpha) / 255.0f);

  EmitNineSliceInternal(emitter,
                        RectI(kCellSize * scale_, kCellSize * scale_,
                              kSliceSize * 2 * scale_, kSliceSize * 2 * scale_),
                        RectI(region.x + cursor.x, region.y + cursor.y,
                              cursor.width, cursor.height),
                        unit, color, true);
}

void WindowVX::EmitContentsInternal(PrimitiveEmitter& emitter) {
  if (openness_ != 255 || !Disposable::Check(contents_))
    return;

  const RectI region = ContentRectInternal();
  const glm::ivec2 origin = glm::ivec2(ox_, oy_);
  const glm::ivec2 contents_size = contents_->size();
  const glm::vec4 color(static_cast<float>(contents_opacity_) / 255.0f);

  emitter.EmitQuad(RectF(region.Position() - origin, contents_size),
                   RectF(0.0f, 0.0f, 1.0f, 1.0f), color);
  emitter.End();
}

void WindowVX::EmitSliceInternal(PrimitiveEmitter& emitter,
                                 const RectI& src,
                                 const RectI& dest,
                                 const glm::vec4& color) {
  if (!src() || !dest())
    return;

  const glm::ivec2 skin_size = window_skin_->size();
  emitter.EmitQuad(RectF(dest), MakeNorm(RectF(src), skin_size), color);
  emitter.End();
}

void WindowVX::EmitTiledInternal(PrimitiveEmitter& emitter,
                                 const RectI& src,
                                 const RectI& dest,
                                 const glm::vec4& color) {
  if (!src() || !dest())
    return;

  const glm::ivec2 skin_size = window_skin_->size();

  const int32_t columns = (dest.width + src.width - 1) / src.width;
  const int32_t rows = (dest.height + src.height - 1) / src.height;

  emitter.BeginQuad().Color4f(color);
  for (int32_t row = 0; row < rows; ++row) {
    const int32_t y = dest.y + row * src.height;
    const int32_t height = std::min(src.height, dest.y + dest.height - y);
    if (height <= 0)
      break;

    for (int32_t column = 0; column < columns; ++column) {
      const int32_t x = dest.x + column * src.width;
      const int32_t width = std::min(src.width, dest.x + dest.width - x);
      if (width <= 0)
        break;

      const RectI src_part(src.x, src.y, width, height);
      emitter.Rect(RectF(static_cast<float>(x), static_cast<float>(y),
                         static_cast<float>(width), static_cast<float>(height)),
                   MakeNorm(RectF(src_part), skin_size));
    }
  }
  emitter.End();
}

void WindowVX::EmitNineSliceInternal(PrimitiveEmitter& emitter,
                                     const RectI& src,
                                     const RectI& dest,
                                     int32_t unit,
                                     const glm::vec4& color,
                                     bool draw_center) {
  if (!src() || !dest() || unit <= 0)
    return;

  const int32_t left = dest.x;
  const int32_t top = dest.y;
  const int32_t right = dest.x + dest.width;
  const int32_t bottom = dest.y + dest.height;

  const int32_t source_unit =
      std::min(unit, std::min(src.width, src.height) / 2);
  if (source_unit <= 0)
    return;

  EmitSliceInternal(emitter, RectI(src.x, src.y, source_unit, source_unit),
                    RectI(left, top, unit, unit), color);
  EmitSliceInternal(
      emitter,
      RectI(src.x + src.width - source_unit, src.y, source_unit, source_unit),
      RectI(right - unit, top, unit, unit), color);
  EmitSliceInternal(
      emitter,
      RectI(src.x + src.width - source_unit, src.y + src.height - source_unit,
            source_unit, source_unit),
      RectI(right - unit, bottom - unit, unit, unit), color);
  EmitSliceInternal(
      emitter,
      RectI(src.x, src.y + src.height - source_unit, source_unit, source_unit),
      RectI(left, bottom - unit, unit, unit), color);

  const int32_t inner_width = dest.width - unit * 2;
  const int32_t inner_height = dest.height - unit * 2;

  if (inner_width > 0) {
    const RectI edge_top(src.x + source_unit, src.y,
                         src.width - source_unit * 2, source_unit);
    const RectI edge_bottom(src.x + source_unit,
                            src.y + src.height - source_unit,
                            src.width - source_unit * 2, source_unit);

    EmitTiledInternal(emitter, edge_top,
                      RectI(left + unit, top, inner_width, unit), color);
    EmitTiledInternal(emitter, edge_bottom,
                      RectI(left + unit, bottom - unit, inner_width, unit),
                      color);
  }

  if (inner_height > 0) {
    const RectI edge_left(src.x, src.y + source_unit, source_unit,
                          src.height - source_unit * 2);
    const RectI edge_right(src.x + src.width - source_unit, src.y + source_unit,
                           source_unit, src.height - source_unit * 2);

    EmitTiledInternal(emitter, edge_left,
                      RectI(left, top + unit, unit, inner_height), color);
    EmitTiledInternal(emitter, edge_right,
                      RectI(right - unit, top + unit, unit, inner_height),
                      color);
  }

  if (draw_center && inner_width > 0 && inner_height > 0) {
    const RectI centre(src.x + source_unit, src.y + source_unit,
                       src.width - source_unit * 2,
                       src.height - source_unit * 2);
    EmitTiledInternal(emitter, centre,
                      RectI(left + unit, top + unit, inner_width, inner_height),
                      color);
  }
}

RectI WindowVX::ContentRectInternal() const {
  return RectI(padding_, padding_, std::max(0, width_ - padding_ * 2),
               std::max(0, height_ - (padding_ + padding_bottom_)));
}

void WindowVX::CreateTintBinding() {
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.window.tint_blends.at(BLEND_NORMAL);

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(TintBase::TintParam);
  tint_uniform_ = GPUDevice::Get().device().CreateBuffer(&tint_desc);

  tint_group_ = util::CreateBindGroup(pipeline.GetBindGroupLayout(3),
                                      {{0, util::BufferSet(tint_uniform_)}});
}

}  // namespace urge

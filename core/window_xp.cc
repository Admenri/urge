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

#include "core/window_xp.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "glm/gtc/matrix_transform.hpp"

#include "core/device.h"
#include "core/gpu_utils.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

//! Distance of one slice of the nine-slice layout at scale 1, in pixels.
constexpr int32_t kSliceSize = 8;

//! The opacity the cursor breathes between, see WindowXP::Update().
constexpr int32_t kCursorOpacityMin = 128;
constexpr int32_t kCursorOpacityMax = 255;

/*! The opacity the cursor moves per tick of Update(): the reference renderer
    steps it by eight, so a full half of the range takes sixteen frames. */
constexpr int32_t kCursorOpacityStep = 8;

//! The offset of the contents inside the frame, at scale 1, in pixels.
constexpr int32_t kContentOffset = 8;

}  // namespace


// ------------------------------------------------------------------------
// WindowXPAbove
// ------------------------------------------------------------------------

WindowXPAbove::WindowXPAbove(WindowXP* parent, RefPtr<Viewport> viewport)
    : Node(viewport, ZValue()), parent_(parent) {
  Node::SetupTrait(this);
}


bool WindowXPAbove::Prepare(DrawParam param) {
  /* The node is the second half of its window and is skipped with it: a
     window which holds nothing inside its frame has no node above. */
  if (parent_ == nullptr)
    return false;

  parent_->stencil_clear_slot_ = {};
  parent_->stencil_slot_ = {};
  parent_->cursor_slot_ = {};
  parent_->arrows_slot_ = {};
  parent_->contents_slot_ = {};

  if (!parent_->above_prepare_)
    return false;

  // The node shares the window's transform, so it draws in the same space.
  const glm::mat4 transform =
      world_transform() *
      glm::translate(glm::mat4(1.0f),
                     glm::vec3(static_cast<float>(parent_->x_),
                               static_cast<float>(parent_->y_), 0.0f));

  const ObjectData object_data = {transform};
  parent_->above_object_slot_ =
      UniformManager::Get().object_uniforms().Acquire(object_data);
  if (parent_->above_object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return false;

  // All quads are emitted here; the batch is uploaded between prepare and draw.
  PrimitiveEmitter& emitter = *param->vertices;

  const auto capture = [&](const std::size_t first) {
    return PrimitiveEmitter::Slot{
        static_cast<std::uint32_t>(first),
        static_cast<std::uint32_t>(emitter.size() - first)};
  };

  // Erase the window area, mark the inner region, then emit the clipped parts.
  const std::size_t stencil_clear_first = emitter.size();
  parent_->EmitStencilClearInternal(emitter);
  parent_->stencil_clear_slot_ = capture(stencil_clear_first);

  const std::size_t stencil_first = emitter.size();
  parent_->EmitStencilInternal(emitter);
  parent_->stencil_slot_ = capture(stencil_first);

  // Cursor and arrows read the same skin but draw in different places, so separate ranges.
  const std::size_t cursor_first = emitter.size();
  parent_->EmitCursorInternal(emitter);
  parent_->cursor_slot_ = capture(cursor_first);

  const std::size_t arrows_first = emitter.size();
  parent_->EmitArrowsInternal(emitter);
  parent_->arrows_slot_ = capture(arrows_first);

  // The contents read their own bitmap, so they are a draw of their own
  const std::size_t contents_first = emitter.size();
  parent_->EmitContentsInternal(emitter);
  parent_->contents_slot_ = capture(contents_first);

  return true;
}


bool WindowXPAbove::DoDraw(DrawParam param) {
  if (parent_ == nullptr || !parent_->above_prepare_)
    return false;

  if (parent_->above_object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return false;

  WindowXP* window = parent_;

  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(window->above_object_slot_.chunk);

  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetBindGroup(1, object_chunk.group, 1,
                           &window->above_object_slot_.offset);
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);

  // Erase the whole window area at kStencilClear so every window can share one reference.
  if (window->stencil_clear_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.window.texture_stencil_write);
    param->pass.SetBindGroup(2, window->windowskin_->texture_group(), 0,
                             nullptr);
    param->pass.SetStencilReference(WindowXP::kStencilClear);
    param->pass.Draw(window->stencil_clear_slot_.count, 1,
                     window->stencil_clear_slot_.first, 0);
  }

  // Mark with a no-colour pass; the write mask drops the colour it would write.
  if (window->stencil_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.window.texture_stencil_write);
    param->pass.SetBindGroup(2, window->windowskin_->texture_group(), 0,
                             nullptr);
    param->pass.SetStencilReference(WindowXP::kStencilReference);
    param->pass.Draw(window->stencil_slot_.count, 1,
                     window->stencil_slot_.first, 0);
  }

  // Cursor and arrows share set 2 from the skin; contents rebind their own texture.
  if (window->cursor_slot_.count || window->arrows_slot_.count ||
      window->contents_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.window.texture_stencil_test);
    param->pass.SetStencilReference(WindowXP::kStencilReference);

    if (window->cursor_slot_.count) {
      param->pass.SetBindGroup(2, window->windowskin_->texture_group(), 0,
                               nullptr);
      param->pass.Draw(window->cursor_slot_.count, 1,
                       window->cursor_slot_.first, 0);
    }

    if (window->arrows_slot_.count) {
      param->pass.SetBindGroup(2, window->windowskin_->texture_group(), 0,
                               nullptr);
      param->pass.Draw(window->arrows_slot_.count, 1,
                       window->arrows_slot_.first, 0);
    }

    if (window->contents_slot_.count && Disposable::Check(window->contents_)) {
      param->pass.SetBindGroup(2, window->contents_->texture_group(), 0,
                               nullptr);
      param->pass.Draw(window->contents_slot_.count, 1,
                       window->contents_slot_.first, 0);
    }
  }

  return false;
}


WindowXP::WindowXP(RefPtr<Viewport> viewport)
    : Node(viewport, ZValue()),
      above_(MakeRefCounted<WindowXPAbove>(this, viewport)),
      contents_(MakeRefCounted<Bitmap>(1, 1)),
      cursor_rect_(MakeRefCounted<Rect>()) {
  Node::SetupTrait(this);
  CreateTintBinding();
}

WindowXP::~WindowXP() {
  Disposable::Dispose();
}

void WindowXP::Update() {
  ++pause_index_;
  if (pause_index_ >= 32)
    pause_index_ = 0;

  if (active_) {
    cursor_opacity_ += cursor_fade_ ? -kCursorOpacityStep : kCursorOpacityStep;
    if (cursor_opacity_ > kCursorOpacityMax) {
      cursor_opacity_ = kCursorOpacityMax;
      cursor_fade_ = true;
    } else if (cursor_opacity_ < kCursorOpacityMin) {
      cursor_opacity_ = kCursorOpacityMin;
      cursor_fade_ = false;
    }
  } else {
    cursor_opacity_ = kCursorOpacityMin;
  }
}


ATTR_DEF(WindowXP, bool, Visible) {
  // Hiding the window must hide the sibling node above, or its parts stay on screen.
  if (above_)
    above_->Attr_Visible(value);

  return Node::Attr_Visible(value);
}


ATTR_DEF(WindowXP, RefPtr<Viewport>, Viewport) {
  /* The node above follows the window into whatever viewport it belongs to,
     the two are one window and are clipped by the same region. */
  if (above_)
    above_->Attr_Parent(value.has_value() ? *value : nullptr);

  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}


ATTR_DEF(WindowXP, RefPtr<Bitmap>, Windowskin) {
  if (value.has_value()) {
    windowskin_ = *value;
    return std::nullopt;
  } else {
    return windowskin_;
  }
}


ATTR_DEF(WindowXP, RefPtr<Bitmap>, Contents) {
  if (value.has_value()) {
    contents_ = *value;
    return std::nullopt;
  } else {
    return contents_;
  }
}


ATTR_DEF(WindowXP, bool, Stretch) {
  if (value.has_value()) {
    stretch_ = *value;
    return std::nullopt;
  } else {
    return stretch_;
  }
}


ATTR_DEF(WindowXP, RefPtr<Rect>, CursorRect) {
  if (value.has_value()) {
    cursor_rect_->Set(*value);
    return std::nullopt;
  } else {
    return cursor_rect_;
  }
}


ATTR_DEF(WindowXP, bool, Active) {
  if (value.has_value()) {
    active_ = *value;
    return std::nullopt;
  } else {
    return active_;
  }
}


ATTR_DEF(WindowXP, bool, Pause) {
  if (value.has_value()) {
    pause_ = *value;
    return std::nullopt;
  } else {
    return pause_;
  }
}


ATTR_DEF(WindowXP, int32_t, X) {
  if (value.has_value()) {
    x_ = *value;
    return std::nullopt;
  } else {
    return x_;
  }
}


ATTR_DEF(WindowXP, int32_t, Y) {
  if (value.has_value()) {
    y_ = *value;
    return std::nullopt;
  } else {
    return y_;
  }
}


ATTR_DEF(WindowXP, int32_t, Width) {
  if (value.has_value()) {
    width_ = *value;
    return std::nullopt;
  } else {
    return width_;
  }
}


ATTR_DEF(WindowXP, int32_t, Height) {
  if (value.has_value()) {
    height_ = *value;
    return std::nullopt;
  } else {
    return height_;
  }
}


ATTR_DEF(WindowXP, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}


ATTR_DEF(WindowXP, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}


ATTR_DEF(WindowXP, int32_t, Opacity) {
  if (value.has_value()) {
    opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return opacity_;
  }
}


ATTR_DEF(WindowXP, int32_t, BackOpacity) {
  if (value.has_value()) {
    back_opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return back_opacity_;
  }
}


ATTR_DEF(WindowXP, int32_t, ContentsOpacity) {
  if (value.has_value()) {
    contents_opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return contents_opacity_;
  }
}


ATTR_DEF(WindowXP, int32_t, Z) {
  // Keep the node above at the window's Z plus two, so it lands after its siblings.
  if (value.has_value()) {
    if (above_)
      above_->Attr_Z(*value + 2);
    return Node::Attr_Z(value);
  } else {
    return Node::Attr_Z(std::nullopt);
  }
}


void WindowXP::DisposeObject() {
  Node::DisposeObject();

  above_.reset();
  windowskin_.reset();
  contents_.reset();
}


bool WindowXP::Prepare(DrawParam param) {
  background_slot_ = {};
  ground_slot_ = {};

  if (!Disposable::Check(windowskin_))
    return false;

  const glm::mat4 transform =
      world_transform() *
      glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x_),
                                                static_cast<float>(y_), 0.0f));

  const ObjectData object_data = {transform};
  object_slot_ = UniformManager::Get().object_uniforms().Acquire(object_data);
  if (object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return false;

  // Tell the node above whether the window holds anything to draw.
  above_prepare_ =
      (cursor_rect_->data.width > 0 && cursor_rect_->data.height > 0) ||
      pause_ || Disposable::Check(contents_);

  // Every quad of the ground, see WindowVX::Prepare()
  PrimitiveEmitter& emitter = *param->vertices;
  const auto capture = [&](const std::size_t first) {
    return PrimitiveEmitter::Slot{
        static_cast<std::uint32_t>(first),
        static_cast<std::uint32_t>(emitter.size() - first)};
  };

  // Only the stretched background goes through the tint pipeline; a tiled one has none.
  const std::size_t background_first = emitter.size();
  if (stretch_)
    EmitBackgroundInternal(emitter);
  background_slot_ = capture(background_first);

  const std::size_t ground_first = emitter.size();
  EmitGroundInternal(emitter);
  ground_slot_ = capture(ground_first);

  return true;
}


bool WindowXP::DoDraw(DrawParam param) {
  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);

  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);

  /* The stretched background layer, through the tint pipeline, see
     WindowVX::DoDraw(). */
  if (background_slot_.count) {
    const TintBase::TintParam tint = {};
    GPUDevice::Get().queue().WriteBuffer(tint_uniform_, 0, &tint, sizeof(tint));

    param->pass.SetPipeline(
        ShaderSet::Get().state.window.tint_blends.at(BLEND_NORMAL));
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, windowskin_->texture_group(), 0, nullptr);
    param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
    param->pass.Draw(background_slot_.count, 1, background_slot_.first, 0);
  }

  /* The frame and, for a window which tiles it, the tiled background layer,
     through the texture pipeline. */
  if (ground_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.window.texture_dynamic_pma);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, windowskin_->texture_group(), 0, nullptr);
    param->pass.Draw(ground_slot_.count, 1, ground_slot_.first, 0);
  }

  return false;
}


void WindowXP::EmitGroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const int32_t slice = kSliceSize * scale_;

  // The frame is the nine slices of the 32x32 skin block at (64, 0), at scale 1.
  const RectI frame_source(64 * scale_, 0, slice * 4, slice * 4);
  const RectI frame_dest(0, 0, width_, height_);

  // 1. The background of a window which tiles it, as a plain texture
  if (!stretch_) {
    const int32_t cell = 64 * scale_;
    const RectI background_source(0, 0, cell, cell);
    const RectI background_dest(scale_, scale_, width_ - 2 * scale_,
                                height_ - 2 * scale_);
    const glm::vec4 background_color =
        glm::vec4(static_cast<float>(opacity_) / 255.0f *
                  static_cast<float>(back_opacity_) / 255.0f);
    EmitTiledInternal(emitter, background_source, background_dest,
                      background_color);
  }

  // 2. The frame: the nine slices of the cell at (64, 0), at full opacity
  const glm::vec4 frame_color(static_cast<float>(opacity_) / 255.0f);
  EmitNineSliceInternal(emitter, frame_source, frame_dest, slice, frame_color,
                        /*draw_center=*/false);
}


void WindowXP::EmitBackgroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  /* The background of an XP skin is the cell at (0, 0), which is 64x64 at
     scale 1, stretched over the inner area. */
  const int32_t cell = 64 * scale_;

  const RectI background_source(0, 0, cell, cell);
  const RectI background_dest(scale_, scale_, width_ - 2 * scale_,
                              height_ - 2 * scale_);

  const glm::vec4 background_color =
      glm::vec4(static_cast<float>(opacity_) / 255.0f *
                static_cast<float>(back_opacity_) / 255.0f);

  EmitSliceInternal(emitter, background_source, background_dest,
                    background_color);
}


void WindowXP::EmitStencilClearInternal(PrimitiveEmitter& emitter) {
  // Erase the whole window area (frame included); emitted in the node above's space.
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const RectI area(0, 0, width_, height_);
  if (!area())
    return;

  emitter.EmitQuad(RectF(area),
                   MakeNorm(RectF(RectI(0, 0, 1, 1)), windowskin_->size()),
                   glm::vec4(1.0f));
  emitter.End();
}


void WindowXP::EmitStencilInternal(PrimitiveEmitter& emitter) {
  const RectI region = ContentRectInternal();
  if (!region())
    return;

  const bool has_cursor =
      cursor_rect_->data.width > 0 && cursor_rect_->data.height > 0;
  if (!has_cursor && !pause_ && !Disposable::Check(contents_))
    return;

  /* The mark is a plain quad over the inner region; its texture and colour
     are irrelevant, because the marking pipeline writes no colour. */
  emitter.EmitQuad(RectF(region),
                   MakeNorm(RectF(RectI(0, 0, 1, 1)), windowskin_->size()),
                   glm::vec4(1.0f));
  emitter.End();
}


void WindowXP::EmitCursorInternal(PrimitiveEmitter& emitter) {
  const RectI cursor = cursor_rect_->data;
  if (cursor.width <= 0 || cursor.height <= 0)
    return;

  const glm::vec4 color(static_cast<float>(contents_opacity_) / 255.0f *
                        static_cast<float>(cursor_opacity_) / 255.0f);

  // Cursor: the nine slices of the (64, 32) cell, stretched to CursorRect, centre kept.
  EmitNineSliceInternal(
      emitter,
      RectI(64 * scale_, 32 * scale_, kSliceSize * 2 * scale_,
            kSliceSize * 2 * scale_),
      RectI(kContentOffset * scale_ + cursor.x,
            kContentOffset * scale_ + cursor.y, cursor.width, cursor.height),
      scale_, color, /*draw_center=*/true);
}


void WindowXP::EmitArrowsInternal(PrimitiveEmitter& emitter) {
  const RectI region = ContentRectInternal();
  const glm::ivec2 origin = LimitedOriginInternal();
  const int32_t slice = kSliceSize * scale_;

  // The arrows and the pause icon are centred on the inner region
  const int32_t center_x = (width_ - slice) / 2;
  const int32_t center_y = (height_ - slice) / 2;

  if (Disposable::Check(contents_)) {
    const glm::ivec2 contents_size = contents_->size();

    const RectI up_src(76 * scale_, 8 * scale_, slice, slice / 2);
    const RectI down_src(76 * scale_, 20 * scale_, slice, slice / 2);
    const RectI left_src(72 * scale_, 12 * scale_, slice / 2, slice);
    const RectI right_src(84 * scale_, 12 * scale_, slice / 2, slice);

    if (origin.x > 0)
      EmitSliceInternal(emitter, left_src,
                        RectI(2 * scale_, center_y, slice / 2, slice),
                        glm::vec4(1.0f));
    if (origin.y > 0)
      EmitSliceInternal(emitter, up_src,
                        RectI(center_x, 2 * scale_, slice, slice / 2),
                        glm::vec4(1.0f));
    if (region.width < contents_size.x - origin.x)
      EmitSliceInternal(emitter, right_src,
                        RectI(width_ - 6 * scale_, center_y, slice / 2, slice),
                        glm::vec4(1.0f));
    if (region.height < contents_size.y - origin.y)
      EmitSliceInternal(emitter, down_src,
                        RectI(center_x, height_ - 6 * scale_, slice, slice / 2),
                        glm::vec4(1.0f));
  }

  /* The four frames of the pause icon are the 2x2 block at (80, 32) of the
     skin; the animation walks it one frame per eighth of the cycle. */
  if (pause_) {
    const int32_t frame = pause_index_ / 8;
    const RectI pause_src((80 + (frame % 2) * 8) * scale_,
                          (32 + (frame / 2) * 8) * scale_, slice, slice);
    const RectI pause_dest(center_x, height_ - slice, slice, slice);

    EmitSliceInternal(emitter, pause_src, pause_dest, glm::vec4(1.0f));
  }
}


void WindowXP::EmitContentsInternal(PrimitiveEmitter& emitter) {
  if (!Disposable::Check(contents_))
    return;

  const RectI region = ContentRectInternal();
  const glm::ivec2 origin = LimitedOriginInternal();
  const glm::ivec2 contents_size = contents_->size();
  const glm::vec4 color(static_cast<float>(contents_opacity_) / 255.0f);

  const RectI src(origin.x, origin.y,
                  std::min(region.width, contents_size.x - origin.x),
                  std::min(region.height, contents_size.y - origin.y));

  if (src()) {
    emitter.EmitQuad(
        RectF(static_cast<float>(kContentOffset * scale_),
              static_cast<float>(kContentOffset * scale_),
              static_cast<float>(src.width), static_cast<float>(src.height)),
        MakeNorm(RectF(src), glm::vec2(contents_size)), color);
    emitter.End();
  }
}


void WindowXP::EmitSliceInternal(PrimitiveEmitter& emitter,
                                 const RectI& src,
                                 const RectI& dest,
                                 const glm::vec4& color) {
  if (!src() || !dest())
    return;

  // Close the batch EmitQuad left open, so callers may emit slices freely.
  const glm::ivec2 skin_size = windowskin_->size();
  emitter.EmitQuad(RectF(dest), MakeNorm(RectF(src), skin_size), color);
  emitter.End();
}


void WindowXP::EmitTiledInternal(PrimitiveEmitter& emitter,
                                 const RectI& src,
                                 const RectI& dest,
                                 const glm::vec4& color) {
  if (!src() || !dest())
    return;

  const glm::ivec2 skin_size = windowskin_->size();

  // Emit the tiles one by one, cutting the tile which overruns a row or column.
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


void WindowXP::EmitNineSliceInternal(PrimitiveEmitter& emitter,
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

  // Corners
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

  // RGSS1 tiles the four frame edges instead of stretching them.
  if (inner_width > 0) {
    EmitTiledInternal(emitter,
                      RectI(src.x + source_unit, src.y,
                            src.width - source_unit * 2, source_unit),
                      RectI(left + unit, top, inner_width, unit), color);
    EmitTiledInternal(
        emitter,
        RectI(src.x + source_unit, src.y + src.height - source_unit,
              src.width - source_unit * 2, source_unit),
        RectI(left + unit, bottom - unit, inner_width, unit), color);
  }

  if (inner_height > 0) {
    EmitTiledInternal(emitter,
                      RectI(src.x, src.y + source_unit, source_unit,
                            src.height - source_unit * 2),
                      RectI(left, top + unit, unit, inner_height), color);
    EmitTiledInternal(
        emitter,
        RectI(src.x + src.width - source_unit, src.y + source_unit, source_unit,
              src.height - source_unit * 2),
        RectI(right - unit, top + unit, unit, inner_height), color);
  }

  // draw_center: the frame leaves the middle to the background, the cursor keeps it.
  if (draw_center && inner_width > 0 && inner_height > 0) {
    EmitTiledInternal(
        emitter,
        RectI(src.x + source_unit, src.y + source_unit,
              src.width - source_unit * 2, src.height - source_unit * 2),
        RectI(left + unit, top + unit, inner_width, inner_height), color);
  }
}


void WindowXP::CreateTintBinding() {
  // The tint buffer is written every drawn frame in the prepare stage.
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.window.tint_blends.at(BLEND_NORMAL);

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(TintBase::TintParam);
  tint_uniform_ = GPUDevice::Get().device().CreateBuffer(&tint_desc);

  tint_group_ = util::CreateBindGroup(pipeline.GetBindGroupLayout(3),
                                   {{0, util::BufferSet(tint_uniform_)}});
}


glm::ivec2 WindowXP::LimitedOriginInternal() const {
  // Contents sit at the slice offset inside the frame, clipped by the limited origin.
  const RectI region = ContentRectInternal();
  const glm::ivec2 contents_size =
      contents_ ? contents_->size() : glm::ivec2(0);
  const int32_t max_x = std::max(0, contents_size.x - region.width);
  const int32_t max_y = std::max(0, contents_size.y - region.height);
  return glm::ivec2(std::clamp(ox_, 0, max_x), std::clamp(oy_, 0, max_y));
}


RectI WindowXP::ContentRectInternal() const {
  // The inner region of the frame: the window inset by one slice.
  return RectI(kContentOffset * scale_, kContentOffset * scale_,
               std::max(0, width_ - kContentOffset * 2 * scale_),
               std::max(0, height_ - kContentOffset * 2 * scale_));
}

}  // namespace urge

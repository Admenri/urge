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

#include "core/gpu.h"
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

  /* The node is a sibling of its window and the parent places both of them at
     the same point, so the transform of the node is the one the window draws
     its own geometry with: the transform of the node hierarchy with the
     offset of the window on top, see WindowXP::Prepare(). */
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

  /* Every quad the node draws is emitted here, because the vertex batch of
     the frame is uploaded between the prepare and the draw stage, see
     Node::Render(). The helpers open a batch of their own for every quad, so
     a group which shares a pipeline is captured as the range of vertices the
     emitter grew by while it was emitted. */
  PrimitiveEmitter& emitter = *param->vertices;

  const auto capture = [&](const std::size_t first) {
    return PrimitiveEmitter::Slot{
        static_cast<std::uint32_t>(first),
        static_cast<std::uint32_t>(emitter.size() - first)};
  };

  /* The region the cursor and the contents are clipped to is marked into the
     stencil first -- after the whole area of the window is erased, so the
     window never reads the mark of another one -- then every clipped part is
     emitted and drawn through the stencil test afterwards. */
  const std::size_t stencil_clear_first = emitter.size();
  parent_->EmitStencilClearInternal(emitter);
  parent_->stencil_clear_slot_ = capture(stencil_clear_first);

  const std::size_t stencil_first = emitter.size();
  parent_->EmitStencilInternal(emitter);
  parent_->stencil_slot_ = capture(stencil_first);

  /* The cursor, the arrows and the pause icon all read the skin, but the
     cursor and the other two are drawn in different places of the frame, so
     they are separate ranges. */
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

  /* The area of the window is erased from the stencil first, through the same
     marking pipeline the inner region is marked with, at kStencilClear. This
     is what lets every window share one reference value: the erase wipes the
     mark an earlier window left in the area this window is about to use, so
     the two can never see each other's region. The erase covers the whole
     area of the window, so it also drops the mark of a window which overlaps
     this one from any direction. */
  if (window->stencil_clear_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_stencil_write);
    param->pass.SetBindGroup(2, window->windowskin_->texture_group(), 0,
                             nullptr);
    param->pass.SetStencilReference(WindowXP::kStencilClear);
    param->pass.Draw(window->stencil_clear_slot_.count, 1,
                     window->stencil_clear_slot_.first, 0);
  }

  /* The mark into the stencil: a pass which writes no colour, so the mask is
     drawn from the skin like the rest of the window and the colour its
     pipeline would write is dropped by the write mask of the pipeline. */
  if (window->stencil_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_stencil_write);
    param->pass.SetBindGroup(2, window->windowskin_->texture_group(), 0,
                             nullptr);
    param->pass.SetStencilReference(WindowXP::kStencilReference);
    param->pass.Draw(window->stencil_slot_.count, 1,
                     window->stencil_slot_.first, 0);
  }

  /* The cursor, the arrows and the contents, clipped to the region the
     stencil carries. The cursor and the arrows read the skin and the
     contents read their own bitmap, so the first two share a bind of set 2
     and the last one binds its texture again. */
  if (window->cursor_slot_.count || window->arrows_slot_.count ||
      window->contents_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_stencil_test);
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

// ------------------------------------------------------------------------
// WindowXP
// ------------------------------------------------------------------------

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

void WindowXP::CreateTintBinding() {
  /* The tint of the stretched background layer is written during the prepare
     stage of every frame the window is drawn in, see
     WindowVX::CreateTintBinding(). */
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.tint_blends.at(BLEND_NORMAL);

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(TintBase::TintParam);
  tint_uniform_ = GPUDevice::Get().device().CreateBuffer(&tint_desc);

  tint_group_ = CreateWGroup(pipeline.GetBindGroupLayout(3),
                             {{0, WBufferSet(tint_uniform_)}});
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
  /* The cursor, the contents, the arrows and the pause icon are drawn by the
     node above the window, which is a sibling in the tree rather than a child
     of this node, see WindowXPAbove.  Hiding the window has to hide that node
     as well, otherwise the contents of a hidden window stay on the screen --
     the frame would disappear while everything inside it did not. */
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

void WindowXP::DisposeObject() {
  Node::DisposeObject();

  above_.reset();
  windowskin_.reset();
  contents_.reset();
}

ATTR_DEF(WindowXP, int32_t, Z) {
  /* The node which draws the cursor and the contents is a sibling of its
     window and has to land after the nodes of the same parent, so it is kept
     two sorting values above the window, see the class documentation. */
  if (value.has_value()) {
    if (above_)
      above_->Attr_Z(*value + 2);
    return Node::Attr_Z(value);
  } else {
    return Node::Attr_Z(std::nullopt);
  }
}

RectI WindowXP::ContentRectInternal() const {
  // The inner region of the frame: the window inset by one slice.
  return RectI(kContentOffset * scale_, kContentOffset * scale_,
               std::max(0, width_ - kContentOffset * 2 * scale_),
               std::max(0, height_ - kContentOffset * 2 * scale_));
}

glm::ivec2 WindowXP::LimitedOriginInternal() const {
  /* The contents of an XP window are placed at the offset of one slice inside
     its frame and only the part of them which the origin can reach is drawn:
     the origin is limited to the overflow of the contents over the inner
     region, so scrolling past their end leaves them in place. */
  const RectI region = ContentRectInternal();
  const glm::ivec2 contents_size =
      contents_ ? contents_->size() : glm::ivec2(0);
  const int32_t max_x = std::max(0, contents_size.x - region.width);
  const int32_t max_y = std::max(0, contents_size.y - region.height);
  return glm::ivec2(std::clamp(ox_, 0, max_x), std::clamp(oy_, 0, max_y));
}

void WindowXP::EmitSliceInternal(PrimitiveEmitter& emitter,
                                 const RectI& src,
                                 const RectI& dest,
                                 const glm::vec4& color) {
  if (!src() || !dest())
    return;

  /* EmitQuad opens a batch of its own and leaves it open: the batch of one
     quad is closed here, so the callers may emit as many slices as they need
     without tripping the one-batch-at-a-time rule of the emitter. */
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

  /* The emitter has no tiled mode and the sampler of a bitmap clamps, so the
     tiles are emitted one by one and a tile which runs over the end of a row
     or a column is cut to the remainder, see WindowVX::EmitTiledInternal. */
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

  /* The reference renderer tiles the four edges of a frame of RGSS1 rather
     than stretching them, so a frame which is wider than its cell repeats it
     instead of scaling it up. */
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

  /* The centre is only part of a nine slice which is meant to be a filled
     frame of its own, i.e. the cursor: the frame of a window draws its four
     corners and the four edges between them and leaves the inner region to
     the background, so its centre is left out, see
     WindowXP::EmitGroundInternal(). The reference renderer draws the cursor
     through a helper which emits all nine, and the frame by hand without it. */
  if (draw_center && inner_width > 0 && inner_height > 0) {
    EmitTiledInternal(
        emitter,
        RectI(src.x + source_unit, src.y + source_unit,
              src.width - source_unit * 2, src.height - source_unit * 2),
        RectI(left + unit, top + unit, inner_width, inner_height), color);
  }
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

  /* The node above is prepared as a sibling, so it has to be told whether the
     window holds anything: its own transform comes from its own node, which
     the parent placed at the same point as this one. */
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

  /* The stretched layer of the background is the only part of a window the
     reference renderer draws through a tint shader, see the class
     documentation. A window which tiles its background has no such layer. */
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
        ShaderSet::Get().state.tint_blends.at(BLEND_NORMAL));
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, windowskin_->texture_group(), 0, nullptr);
    param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
    param->pass.Draw(background_slot_.count, 1, background_slot_.first, 0);
  }

  /* The frame and, for a window which tiles it, the tiled background layer,
     through the texture pipeline. */
  if (ground_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_dynamic_pma);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, windowskin_->texture_group(), 0, nullptr);
    param->pass.Draw(ground_slot_.count, 1, ground_slot_.first, 0);
  }

  return false;
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

void WindowXP::EmitGroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const int32_t slice = kSliceSize * scale_;

  /* The frame of an XP skin is the 32x32 block at (64, 0) at scale 1: its
     corners sit at (64, 0), (88, 0), (88, 24) and (64, 24), so the block has
     to span the full 32 pixels of height for the nine slice to read the
     bottom corners from y = 24 rather than y = 16, see
     EmitNineSliceInternal(). */
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

void WindowXP::EmitStencilClearInternal(PrimitiveEmitter& emitter) {
  /* The erase covers the whole area of the window, not only the inner region:
     a window which overlaps this one keeps its own mark elsewhere in the
     window, and erasing only the inner region would leave that mark in place
     for a later neighbour to trip over. Erasing the frame as well is
     harmless, because the frame is never tested against the stencil.

     The node above draws the erase, so it is emitted in the space of that
     node, which is the window placed at its own (x, y) -- the same space the
     mark and the clipped parts use, see WindowXPAbove::Prepare(). */
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

  /* The cursor is the nine slices of the cell at (64, 32) of the skin,
     stretched to the size of CursorRect and placed at the offset of the
     window inside its frame. The cursor keeps its centre: the cell is a
     frame whose middle is filled. */
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

}  // namespace urge

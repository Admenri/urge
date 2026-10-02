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
#include "core/gpu.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

/*! Index of the animation frame of the pause icon over a cycle of 64 ticks:
    every frame holds a quarter of the cycle, so the icon blinks in place. */
constexpr int32_t kPauseIndexTable[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
};

/*! Opacity of the cursor over the cycle of the animation, i.e. a fade from
    full to half and back, see WindowVX::Update(). */
constexpr int32_t kCursorAlphaTable[] = {
    255, 247, 239, 231, 223, 215, 207, 199, 191, 183, 175, 167, 159, 151,
    143, 135, 127, 119, 111, 103, 95,  103, 111, 119, 127, 135, 143, 151,
    159, 167, 175, 183, 191, 199, 207, 215, 223, 231, 239, 247,
};

//! Distance of one slice of the nine-slice layout at scale 1, in pixels.
constexpr int32_t kSliceSize = 8;

//! Distance of one cell of the skin the window reads from, at scale 1.
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
      tone_(MakeRefCounted<Tone>()) {
  Node::SetupTrait(this);
  CreateTintBinding();
}

WindowVX::WindowVX(RefPtr<Viewport> viewport) : WindowVX(0, 0, 0, 0) {
  Attr_Viewport(viewport);
}

WindowVX::~WindowVX() {
  Disposable::Dispose();
}

void WindowVX::CreateTintBinding() {
  /* The tint of the stretched background layer is written during the prepare
     stage of every frame the window is drawn in, so the contents of the
     buffer are not staged here -- see Plane::CreateEffectBindings(), which
     does the same for the tint of a plane. */
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.tint_blends.at(BLEND_NORMAL);

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(TintBase::TintParam);
  tint_uniform_ = GPUDevice::Get().device().CreateBuffer(&tint_desc);

  tint_group_ = CreateWGroup(pipeline.GetBindGroupLayout(3),
                             {{0, WBufferSet(tint_uniform_)}});
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
  /* A window is a node of the tree like any other, so the viewport it belongs
     to is the viewport its parent chain leads up to, see Sprite::Viewport. */
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

RectI WindowVX::ContentRectInternal() const {
  return RectI(padding_, padding_, std::max(0, width_ - padding_ * 2),
               std::max(0, height_ - (padding_ + padding_bottom_)));
}

glm::ivec2 WindowVX::LimitedOriginInternal() const {
  /* An origin only has to move what can be moved: scrolling past the end of
     the contents would keep them fixed against the inner region and leave a
     gap, so it is limited to the amount the contents overflow the region. */
  const RectI region = ContentRectInternal();
  const glm::ivec2 contents_size =
      contents_ ? contents_->size() : glm::ivec2(0);
  const int32_t max_x = std::max(0, contents_size.x - region.width);
  const int32_t max_y = std::max(0, contents_size.y - region.height);
  return glm::ivec2(std::clamp(ox_, 0, max_x), std::clamp(oy_, 0, max_y));
}

void WindowVX::EmitSliceInternal(PrimitiveEmitter& emitter,
                                 const RectI& src,
                                 const RectI& dest,
                                 const glm::vec4& color) {
  if (!src() || !dest())
    return;

  /* The texture coordinate of a quad follows a top-left origin, which is what
     the skins of both engines store, so a cell maps onto its destination
     without a flip. */
  /* EmitQuad opens a batch of its own and leaves it open: the batch of one
     quad is closed here, so the callers may emit as many slices as they need
     without tripping the one-batch-at-a-time rule of the emitter. */
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

  /* The emitter has no tiled mode and the sampler of a bitmap clamps, so the
     tiles are emitted one by one: every tile of the destination reads the
     same cell of the skin, and the tile which runs over the end of a row or a
     column is cut to the remainder. */
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

      /* A cut tile reads the matching part of the cell: the texture
         coordinate is the fraction of the cell the tile covers. */
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

  /* A source which is smaller than two units has no room for an inner cell
     either, the unit of the slices is then half of what it holds. */
  const int32_t source_unit =
      std::min(unit, std::min(src.width, src.height) / 2);
  if (source_unit <= 0)
    return;

  // Corners: every corner of the destination reads the corner of the cell.
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

  /* The edges and the centre of the nine slices: a destination which is
     smaller than two units leaves no room for them, its corners then cover
     the whole of it. */
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

  /* The centre is only part of a nine slice which is meant to be a filled
     frame of its own, i.e. the cursor: the frame of a window draws its four
     corners and the four edges between them and leaves the inner region to
     the background, so its centre is left out, see
     WindowVX::EmitGroundInternal(). The reference renderer draws the cursor
     through a helper which emits all nine, and the frame by hand without it. */
  if (draw_center && inner_width > 0 && inner_height > 0) {
    const RectI centre(src.x + source_unit, src.y + source_unit,
                       src.width - source_unit * 2,
                       src.height - source_unit * 2);
    EmitTiledInternal(emitter, centre,
                      RectI(left + unit, top + unit, inner_width, inner_height),
                      color);
  }
}

bool WindowVX::Prepare(DrawParam param) {
  background_slot_ = {};
  ground_slot_ = {};
  stencil_clear_slot_ = {};
  stencil_slot_ = {};
  clipped_slot_ = {};

  /* The frame and the background of a window are drawn from its skin alone,
     the cursor and the contents from the skin and the contents bitmap, and
     the whole of it is skipped for a skin which was disposed. */
  if (!Disposable::Check(window_skin_))
    return false;

  /* The window is placed by the transform of the node hierarchy, so its own
     (x, y) is the offset from that placement, see Sprite::Prepare(). */
  const glm::mat4 transform =
      world_transform() *
      glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x_),
                                                static_cast<float>(y_), 0.0f));

  const ObjectData object_data = {transform};
  object_slot_ = UniformManager::Get().object_uniforms().Acquire(object_data);
  if (object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return false;

  /* Every quad of the window is emitted here: the vertex batch of the frame
     is uploaded between the prepare and the draw stage, so a draw stage which
     emitted geometry would append it to a batch nobody reads, see
     Node::Render().

     The helpers of the emitter open a batch of their own for every quad they
     append, so a group which is drawn with one pipeline is captured as the
     range of vertices the emitter grew by while it was emitted -- the index
     of a vertex holds in the vertex buffer after the upload, which is what a
     draw addresses, see PrimitiveEmitter::Slot. */
  PrimitiveEmitter& emitter = *param->vertices;

  const auto capture = [&](const std::size_t first) {
    return PrimitiveEmitter::Slot{
        static_cast<std::uint32_t>(first),
        static_cast<std::uint32_t>(emitter.size() - first)};
  };

  /* The stretch layer of the background is the only part of the window the
     reference renderer tints, so it takes a range of its own and is drawn
     through the tint pipeline, see the class documentation. */
  const std::size_t background_first = emitter.size();
  EmitBackgroundInternal(emitter);
  background_slot_ = capture(background_first);

  /* Everything else of the frame and the background is drawn through the
     texture pipeline, which needs no tint of its own. */
  const std::size_t ground_first = emitter.size();
  EmitGroundInternal(emitter);
  ground_slot_ = capture(ground_first);

  /* The cursor and the contents are clipped to the inner region of the
     frame, so the region is marked into the stencil first and both of them
     are drawn through the stencil test afterwards. Before the mark is laid
     down, the whole area of the window is erased to kStencilClear so the
     window does not depend on the mark of any other window, see
     kStencilReference. */
  const std::size_t stencil_clear_first = emitter.size();
  EmitStencilClearInternal(emitter);
  stencil_clear_slot_ = capture(stencil_clear_first);

  const std::size_t stencil_first = emitter.size();
  EmitStencilInternal(emitter);
  stencil_slot_ = capture(stencil_first);

  /* The cursor is read from the skin and the contents from their own bitmap,
     so the two form a range each even though they share a pipeline. */
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

  // The scene and the object transform are shared by every draw of the window
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);

  /* The stretched background layer, through the tint pipeline: it is the only
     part of the window the reference renderer draws under a tint shader. The
     param of the shader is the tint of the window, i.e. no colour and no
     tone, exactly what the reference renderer sets for it. */
  if (background_slot_.count) {
    TintBase::TintParam tint = {};
    tint.blend_tone = tone_->Normalize();
    GPUDevice::Get().queue().WriteBuffer(tint_uniform_, 0, &tint, sizeof(tint));

    param->pass.SetPipeline(
        ShaderSet::Get().state.tint_blends.at(BLEND_NORMAL));
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
    param->pass.Draw(background_slot_.count, 1, background_slot_.first, 0);
  }

  /* The tiled background layer and the nine-slice frame, through the texture
     pipeline, which only needs the colour of their vertices. */
  if (ground_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_dynamic_pma);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.Draw(ground_slot_.count, 1, ground_slot_.first, 0);
  }

  /* The area of the window is erased from the stencil first, through the same
     marking pipeline the inner region is marked with, at kStencilClear. This
     is what lets every window share one reference value: the erase wipes the
     mark an earlier window left in the area this window is about to use, so
     the two can never see each other's region. The erase covers the whole
     area of the window rather than only the inner region, so it also drops
     the mark of a window which overlaps this one from any direction. */
  if (stencil_clear_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_stencil_write);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.SetStencilReference(kStencilClear);
    param->pass.Draw(stencil_clear_slot_.count, 1, stencil_clear_slot_.first,
                     0);
  }

  /* The inner region is marked into the stencil by a pass which writes no
     colour: the mask is drawn from the skin like the rest of the window, so
     the texture its pipeline expects is bound, and the colour it would write
     is discarded by the write mask of the pipeline. */
  if (stencil_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_stencil_write);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, window_skin_->texture_group(), 0, nullptr);
    param->pass.SetStencilReference(kStencilReference);
    param->pass.Draw(stencil_slot_.count, 1, stencil_slot_.first, 0);
  }

  /* The cursor and the contents, clipped to the region the stencil carries.
     The cursor is drawn from the skin and the contents from their own bitmap,
     so the two are separate draws with a texture each. */
  if (clipped_slot_.count) {
    param->pass.SetPipeline(ShaderSet::Get().state.texture_stencil_test);
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

void WindowVX::EmitBackgroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const int32_t cell = kCellSize * scale_;

  /* The two cells at the top of the skin hold the background of the window:
     the first one is stretched over the inner area, the second one is tiled
     over it. A skin which carries no background leaves both of them empty,
     which reads white and is what the default skin of RGSS3 is. */
  const RectI background_source(0, 0, cell, cell);

  const RectI background_dest(scale_, scale_, width_ - 2 * scale_,
                              height_ - 2 * scale_);

  /* The background is scaled around the horizontal centre line of the window
     while it opens or closes, see the class documentation. Its colour carries
     the opacity of the background of the window on every channel, because the
     state of the engine is premultiplied alpha. */
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

void WindowVX::EmitGroundInternal(PrimitiveEmitter& emitter) {
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const int32_t cell = kCellSize * scale_;
  const int32_t unit = kSliceSize * scale_;

  /* The tiled layer of the background is the second cell of the skin, drawn
     as a plain texture, see the class documentation. */
  const RectI background_tile_source(0, cell, cell, cell);

  /* The frame is the nine slices of the block which holds the border of the
     skin: the four corners at (32, 0), (56, 0), (32, 24) and (56, 24), the
     four edges between them and the 16x16 middle, all at scale 1 inside a
     32x32 block which starts at x = 32, see EmitNineSliceInternal(). */
  const RectI frame_source(32 * scale_, 0, unit * 4, unit * 4);

  const RectI background_dest(scale_, scale_, width_ - 2 * scale_,
                              height_ - 2 * scale_);
  const RectI frame_dest(0, 0, width_, height_);

  /* The frame and the tiled layer of the background are scaled around the
     horizontal centre line of the window while it opens or closes. */
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

  // The frame is drawn at the full opacity of the window
  const glm::vec4 frame_color(static_cast<float>(opacity_) / 255.0f);
  EmitNineSliceInternal(emitter, frame_source, apply_openness(frame_dest), unit,
                        frame_color, /*draw_center=*/false);

  if (openness_ != 255)
    return;

  // The arrows: one slice per side, drawn while the contents overflow it
  const RectI region = ContentRectInternal();
  const glm::ivec2 origin = LimitedOriginInternal();

  // Both the arrows and the pause icon are centred on the window
  const int32_t arrow_x = (width_ - unit) / 2;
  const int32_t arrow_y = (height_ - unit) / 2;

  if (arrows_visible_ && Disposable::Check(contents_)) {
    const glm::ivec2 contents_size = contents_->size();

    const RectI up_src(44 * scale_, 8 * scale_, unit, unit / 2);
    const RectI down_src(44 * scale_, 20 * scale_, unit, unit / 2);
    const RectI left_src(40 * scale_, 12 * scale_, unit / 2, unit);
    const RectI right_src(52 * scale_, 12 * scale_, unit / 2, unit);

    if (origin.x > 0)
      EmitSliceInternal(emitter, left_src,
                        RectI(2 * scale_, arrow_y, unit / 2, unit),
                        glm::vec4(1.0f));
    if (origin.y > 0)
      EmitSliceInternal(emitter, up_src,
                        RectI(arrow_x, 2 * scale_, unit, unit / 2),
                        glm::vec4(1.0f));
    if (region.width < contents_size.x - origin.x)
      EmitSliceInternal(emitter, right_src,
                        RectI(width_ - 6 * scale_, arrow_y, unit / 2, unit),
                        glm::vec4(1.0f));
    if (region.height < contents_size.y - origin.y)
      EmitSliceInternal(emitter, down_src,
                        RectI(arrow_x, height_ - 6 * scale_, unit, unit / 2),
                        glm::vec4(1.0f));
  }

  // The pause icon: the four frames of it, cycling through kPauseIndexTable
  if (pause_) {
    const int32_t frame = kPauseIndexTable[pause_index_];
    const RectI pause_src((48 + (frame % 2) * 8) * scale_,
                          (32 + (frame / 2) * 8) * scale_, unit, unit);
    const RectI pause_dest(arrow_x, height_ - unit, unit, unit);

    EmitSliceInternal(emitter, pause_src, pause_dest, glm::vec4(1.0f));
  }
}

void WindowVX::EmitStencilClearInternal(PrimitiveEmitter& emitter) {
  /* The erase covers the whole area of the window, not only the inner region:
     a window which overlaps this one keeps its own mark elsewhere in the
     window, and erasing only the inner region would leave that mark in place
     for a later neighbour to trip over. Erasing the frame as well is
     harmless, because the frame is never tested against the stencil. */
  if (width_ < scale_ * 2 || height_ < scale_ * 2)
    return;

  const RectI area(0, 0, width_, height_);
  if (!area())
    return;

  /* A quad of the same shape as the mark: its colour and its texture are
     irrelevant, because the pipeline of the pass writes no colour. */
  emitter.EmitQuad(RectF(area),
                   MakeNorm(RectF(RectI(0, 0, 1, 1)), window_skin_->size()),
                   glm::vec4(1.0f));
  emitter.End();
}

void WindowVX::EmitStencilInternal(PrimitiveEmitter& emitter) {
  /* The cursor and the contents of a closing window are not drawn, so there
     is nothing to mark for them either. */
  if (openness_ != 255)
    return;

  const RectI region = ContentRectInternal();
  if (!region())
    return;

  const bool has_cursor =
      cursor_rect_->data.width > 0 && cursor_rect_->data.height > 0;
  if (!has_cursor && !Disposable::Check(contents_))
    return;

  /* The mark is a plain quad over the inner region. Its colour and its
     texture are irrelevant: the pipeline of the marking pass writes no
     colour. The quad is emitted with an opaque colour so the vertices are
     well defined, and it is emitted into the same object space as the
     cursor and the contents. */
  emitter.EmitQuad(RectF(region),
                   MakeNorm(RectF(RectI(0, 0, 1, 1)), window_skin_->size()),
                   glm::vec4(1.0f));
  emitter.End();
}

void WindowVX::EmitCursorInternal(PrimitiveEmitter& emitter) {
  /* The cursor of a window which is not fully open is not drawn, and there is
     nothing for the stencil to mark either, see EmitStencilInternal(). */
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

  /* The cursor is the nine slices of the cell at (32, 32) of the skin,
     stretched to the size of CursorRect and placed inside the frame. The
     cursor keeps its centre: the cell is a frame whose middle is filled. */
  EmitNineSliceInternal(emitter,
                        RectI(kCellSize * scale_, kCellSize * scale_,
                              kSliceSize * 2 * scale_, kSliceSize * 2 * scale_),
                        RectI(region.x + cursor.x, region.y + cursor.y,
                              cursor.width, cursor.height),
                        unit, color, /*draw_center=*/true);
}

void WindowVX::EmitContentsInternal(PrimitiveEmitter& emitter) {
  if (openness_ != 255 || !Disposable::Check(contents_))
    return;

  const RectI region = ContentRectInternal();
  const glm::ivec2 origin = LimitedOriginInternal();
  const glm::ivec2 contents_size = contents_->size();
  const glm::vec4 color(static_cast<float>(contents_opacity_) / 255.0f);

  const RectI src(origin.x, origin.y,
                  std::min(region.width, contents_size.x - origin.x),
                  std::min(region.height, contents_size.y - origin.y));

  /* The contents are placed at the inner region of the window, moved by the
     origin: only the part of them the region holds is read from the bitmap,
     and the part which lies outside is left alone instead of being drawn
     over the frame. */
  if (src()) {
    emitter.EmitQuad(
        RectF(static_cast<float>(region.x), static_cast<float>(region.y),
              static_cast<float>(src.width), static_cast<float>(src.height)),
        MakeNorm(RectF(src), glm::vec2(contents_size)), color);
    emitter.End();
  }
}

}  // namespace urge

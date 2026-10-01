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

#include "core/bitmap.h"
#include "core/node.h"
#include "core/primitive.h"
#include "core/uniform.h"
#include "core/utility.h"
#include "core/viewport.h"

namespace urge {

/**
\brief Window of RGSS3 (VX / VXA): a nine-slice frame, an inner background,
a cursor frame, scroll arrows, a pause animation and the contents bitmap.

The window is one node of the tree. Its whole geometry is a handful of quads
of one windowskin bitmap, so every part of it is emitted into the vertex batch
of the frame. The reference renderer draws the background of the window
through a tint shader and every other part of it as a plain texture, so the
window owns two pipelines:

- The stretched background layer is drawn through the tint pipeline, under the
  colour of the opacity of the background of the window. It is the only part
  of a window which the reference renderer tints, so it is the only part drawn
  through tint_blends[BLEND_NORMAL] here as well.
- The tiled background layer, the frame, the arrows, the pause animation, the
  cursor and the contents are drawn through the texture pipeline: they only
  need the colour of their vertex, which the premultiplied-alpha state of the
  engine already carries.

The parts of the window are laid out as:

- The frame is the nine slices of a 32x32 cell of the skin: four corners, four
  tiled edges and the inner area.
- The background is the first cell of the skin stretched over the inner area
  and its second cell tiled over it.
- The cursor is the nine slices of the cell at (32, 32), stretched to the size
  of CursorRect and animated by kCursorAlphaTable.
- The contents bitmap is drawn at the padding of the window, scrolled by the
  origin of the window, which is limited to the overflow of the contents.
- The arrows and the pause animation are single slices of the skin.

The window opens and closes with Openness: the region of the frame and the
background is scaled around its horizontal centre line, and the cursor, the
arrows, the pause and the contents are only drawn once the window is fully
open, which is the behaviour of the RGSS3 Window class.

The reference renderer clips the cursor and the contents against the inner
region of the frame through a stencil buffer, and this engine does the same:
it binds the depth-stencil texture of the render target to every frame, so the
window erases the whole area it covers from the stencil first, marks its inner
region into it -- with a pass which writes no colour -- and then draws the
cursor and the contents through a pipeline which only passes where the stencil
carries the mark. The scissor of the engine is reserved for the viewport, see
Viewport, and is not used here.

Marking in two steps is what lets every window share one reference value,
see kStencilReference: the erase wipes the mark of whichever window was drawn
over this area before, so the number of windows one frame draws is not limited
by the number of values the stencil can hold.
*/
class WindowVX : public Node {
 public:
  /*-export.begin-*/
  WindowVX(int32_t x, int32_t y, int32_t width, int32_t height);
  WindowVX(RefPtr<Viewport> viewport = nullptr);
  ~WindowVX() override;

  void Update();
  void Move(int32_t x, int32_t y, int32_t width, int32_t height);
  bool Opened();
  bool Closed();

  ATTR(RefPtr<Viewport>, Viewport);
  ATTR(RefPtr<Bitmap>, Windowskin);
  ATTR(RefPtr<Bitmap>, Contents);
  ATTR(RefPtr<Rect>, CursorRect);
  ATTR(bool, Active);
  ATTR(bool, ArrowsVisible);
  ATTR(bool, Pause);
  ATTR(int32_t, X);
  ATTR(int32_t, Y);
  ATTR(int32_t, Width);
  ATTR(int32_t, Height);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  ATTR(int32_t, Padding);
  ATTR(int32_t, PaddingBottom);
  ATTR(int32_t, Opacity);
  ATTR(int32_t, BackOpacity);
  ATTR(int32_t, ContentsOpacity);
  ATTR(int32_t, Openness);
  ATTR(int32_t, Scale);
  /*-export.end-*/

  /*! The reference value the window marks its inner region with, see the
      class documentation. Every window uses the SAME value: the mark of one
      window stays in the stencil until the frame ends, so an earlier design
      handed out one value per window -- which broke as soon as a scene held
      more windows than the stencil holds values, because two of them then
      shared a mark and the clip of one passed inside the region of the other.
      In this design a window instead CLEARS the region it is about to use by
      writing zero into it, which is why a single constant is enough. */
  static constexpr uint32_t kStencilReference = 1;

  /*! The value a window writes to erase a stencil region, i.e. the value the
      attachment is cleared to at the start of the frame, see
      Node::Render(). */
  static constexpr uint32_t kStencilClear = 0;

 private:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  /*! Emits the tiled background layer and the nine-slice frame of the window,
      i.e. the parts of it which are drawn through the texture pipeline and
      are not clipped to the inner region. */
  void EmitGroundInternal(PrimitiveEmitter& emitter);
  //! Emits the stretched background layer, which the tint pipeline draws.
  void EmitBackgroundInternal(PrimitiveEmitter& emitter);
  /*! Emits the quad which erases the stencil over the whole area of the
      window, see kStencilReference -- it is drawn through the marking
      pipeline at kStencilClear before the inner region is marked. */
  void EmitStencilClearInternal(PrimitiveEmitter& emitter);
  /*! Emits the quad which marks the inner region of the window into the
      stencil, see the class documentation. */
  void EmitStencilInternal(PrimitiveEmitter& emitter);
  //! Emits the cursor of the window, which is clipped to the inner region.
  void EmitCursorInternal(PrimitiveEmitter& emitter);
  //! Emits the contents of the window, which are clipped to the inner region.
  void EmitContentsInternal(PrimitiveEmitter& emitter);

  //! Emits the quad of one slice of the skin, stretched over \p dest.
  void EmitSliceInternal(PrimitiveEmitter& emitter,
                         const RectI& src,
                         const RectI& dest,
                         const glm::vec4& color);
  //! Emits one slice of the skin tiled over \p dest.
  void EmitTiledInternal(PrimitiveEmitter& emitter,
                         const RectI& src,
                         const RectI& dest,
                         const glm::vec4& color);
  /*! Emits the nine slices of \p src stretched over \p dest: the four corners
      at the size of one unit, the four edges between them and, when \p
      draw_center is set, the centre filling what is left. The frame of a
      window draws without its centre -- the inner region belongs to the
      background -- while the cursor is a filled frame and keeps it. */
  void EmitNineSliceInternal(PrimitiveEmitter& emitter,
                             const RectI& src,
                             const RectI& dest,
                             int32_t unit,
                             const glm::vec4& color,
                             bool draw_center = true);

  //! The inner region of the frame, in window coordinates, i.e. the area the
  //! contents and the cursor are placed in and clipped to.
  RectI ContentRectInternal() const;
  //! The origin of the contents, i.e. the window origin limited to the parts
  //! of the contents which can actually scroll.
  glm::ivec2 LimitedOriginInternal() const;

  /*! Creates the buffer and the bind group of set 3 of the tint pipeline,
      which the stretched background layer is drawn through, see
      WindowVX::EmitBackgroundInternal(). It matches
      Plane::CreateEffectBindings(). */
  void CreateTintBinding();

  RefPtr<Viewport> viewport_;
  RefPtr<Bitmap> window_skin_;
  RefPtr<Bitmap> contents_;
  RefPtr<Rect> cursor_rect_;
  bool active_ = true, arrows_visible_ = true, pause_ = false;
  int32_t x_ = 0, y_ = 0, width_ = 0, height_ = 0;
  int32_t ox_ = 0, oy_ = 0;
  int32_t padding_ = 12, padding_bottom_ = 12;
  int32_t opacity_ = 255, back_opacity_ = 192, contents_opacity_ = 255;
  int32_t openness_ = 255;
  int32_t scale_ = 2;

  int32_t pause_index_ = 0;
  int32_t cursor_index_ = 0;

  //! The object pool slot of this window, bound at set 1.
  UniformBlockPool::Slot object_slot_ = {};

  /*! The param of the tint shader of the stretched background layer. It is a
      buffer of its own, written in the prepare stage of every frame the
      window is drawn in, see TintBase::TintParam. */
  wgpu::Buffer tint_uniform_;
  //! The bind group of set 3 of the tint pipeline, see CreateTintBinding().
  wgpu::BindGroup tint_group_;

  //! The range the stretched background layer appended to the batch.
  PrimitiveEmitter::Slot background_slot_ = {};
  //! The range the tiled background layer and the frame appended.
  PrimitiveEmitter::Slot ground_slot_ = {};
  /*! The range which erases the stencil over the area of the window, drawn at
      kStencilClear before stencil_slot_ marks it again. */
  PrimitiveEmitter::Slot stencil_clear_slot_ = {};
  //! The range which marks the inner region into the stencil.
  PrimitiveEmitter::Slot stencil_slot_ = {};
  //! The whole range the cursor and the contents appended, which is drawn
  //! through the stencil test.
  PrimitiveEmitter::Slot clipped_slot_ = {};
  //! The part of clipped_slot_ which is the cursor, read from the skin.
  PrimitiveEmitter::Slot cursor_slot_ = {};
  //! The part of clipped_slot_ which is the contents, read from their bitmap.
  PrimitiveEmitter::Slot contents_slot_ = {};
};
}  // namespace urge

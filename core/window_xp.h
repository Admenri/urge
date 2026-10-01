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

class WindowXP;

/**
\brief The second half of a WindowXP: a hidden node which draws the cursor,
the arrows, the pause icon and the contents of its window after every other
node of the same parent.
A window of RGSS1 keeps its frame and its background in the window itself and
everything inside its frame in a node which is drawn above the rest of the
scene: the reference renderer gives that node the sorting value of the window
plus two, so it lands after the nodes which share the window's parent. The
node is not part of the public API -- it is created, driven and drawn by the
WindowXP which owns it, and it draws nothing on its own.

The node marks the inner region of its window into the stencil before it
draws the parts which are clipped to it, and it draws them through the
stencil test, which is what the reference renderer does with its content
area, see WindowXP.

\remarks The node is a child of the same parent as its WindowXP, so it is
clipped by the same viewport and follows the same transform chain. Its `Z` is
kept at the `Z` of the window plus two, see WindowXP::Attr_Z.
*/
class WindowXPAbove : public Node {
 public:
  WindowXPAbove(WindowXP* parent, RefPtr<Viewport> viewport);

 protected:
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  WindowXP* parent_ = nullptr;
};

/**
\brief Window of RGSS1 (XP): a nine-slice frame over a background, with a
cursor frame, scroll arrows, a pause animation and the contents bitmap.

The window is two nodes of the tree. The WindowXP itself draws the frame and
the background -- the nine slices of the cell at (64, 0) of the skin over the
cell at (0, 0), which is stretched over the inner area or tiled over it, see
Stretch -- and its WindowXPAbove draws everything inside the frame, so the
contents of a window land above the other nodes of its parent instead of
interleaving with them by their own sort value.

- The frame is the nine slices of the skin at scale 1: the corners at (64, 0),
  (88, 0), (64, 24) and (88, 24), the edges between them and the cell itself,
  which the reference renderer tiles rather than stretching.
- The background is the first cell of the skin, stretched over the inner area
  while Stretch is set and tiled over it while it is not, under the colour of
  the opacity of the background of the window. The stretched layer alone is
  drawn through the tint pipeline, which is what the reference renderer does
  with it, see WindowVX.
- The cursor is the nine slices of the cell at (64, 32), stretched to the size
  of CursorRect and animated by the breathing opacity of the window, which
  fades between 128 and 255 rather than stepping through a table.
- The contents are drawn at the offset (8, 8) of the window, scrolled by the
  origin, and are limited to the parts of them which the origin can reach.
- The arrows and the pause icon are single slices of the skin; the pause icon
  cycles through the four frames of the 2x2 block at (80, 32) of the skin.

The reference renderer clips the cursor and the contents against the inner
region of the frame through a stencil buffer, and this engine does the same:
every frame binds the depth-stencil texture of its render target, so the node
above marks the inner region into the stencil with a pass which writes no
colour and then draws the cursor and the contents through a pipeline which
only passes where the stencil carries the mark. The scissor of the engine is
reserved for the viewport, see Viewport, and is not used here.
*/
class WindowXP : public Node {
 public:
  /*-export.begin-*/
  WindowXP(RefPtr<Viewport> viewport = nullptr);
  ~WindowXP() override;

  void Update();

  /*! Hides the node above the window together with the window itself, see
      WindowXP::Attr_Visible -- the two are one window, and a hidden window
      must not leave its contents on the screen. */
  virtual ATTR(bool, Visible);
  ATTR(RefPtr<Viewport>, Viewport);
  ATTR(RefPtr<Bitmap>, Windowskin);
  ATTR(RefPtr<Bitmap>, Contents);
  ATTR(bool, Stretch);
  ATTR(RefPtr<Rect>, CursorRect);
  ATTR(bool, Active);
  ATTR(bool, Pause);
  ATTR(int32_t, X);
  ATTR(int32_t, Y);
  ATTR(int32_t, Width);
  ATTR(int32_t, Height);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  ATTR(int32_t, Opacity);
  ATTR(int32_t, BackOpacity);
  ATTR(int32_t, ContentsOpacity);
  ATTR(int32_t, Scale);
  ATTR(int32_t, Z);
  /*-export.end-*/

 private:
  friend class WindowXPAbove;

  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  /*! Emits the tiled background layer and the nine-slice frame of the window,
      i.e. the parts of it which are drawn through the texture pipeline. */
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
  //! Emits the arrows and the pause icon, which are clipped to the frame.
  void EmitArrowsInternal(PrimitiveEmitter& emitter);
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
  /*! Emits the nine slices of the cell of \p src stretched over \p dest, see
      WindowVX::EmitNineSliceInternal. The centre is only part of a nine slice
      which is meant to be a filled frame of its own, i.e. the cursor, so the
      frame of a window passes \c false for it. */
  void EmitNineSliceInternal(PrimitiveEmitter& emitter,
                             const RectI& src,
                             const RectI& dest,
                             int32_t unit,
                             const glm::vec4& color,
                             bool draw_center = true);

  /*! Creates the buffer and the bind group of set 3 of the tint pipeline,
      which the stretched background layer is drawn through, see
      WindowVX::CreateTintBinding(). */
  void CreateTintBinding();

  //! The origin of the contents, i.e. limited to the parts they can reach.
  glm::ivec2 LimitedOriginInternal() const;

  //! The inner region of the frame, in window coordinates.
  RectI ContentRectInternal() const;

  /*! The reference value the window marks its inner region with, and the
      value it erases that region to first, see WindowVX::kStencilReference.
      Every window shares the pair: the erase is what keeps two windows from
      reading each other's mark, so the reference needs no longer be unique
      per window. */
  static constexpr uint32_t kStencilReference = 1;
  static constexpr uint32_t kStencilClear = 0;

  //! The node which draws the cursor and the contents above the parent of
  //! this window, see the class documentation.
  RefPtr<WindowXPAbove> above_;

  /*! True while the node above has something to draw: a cursor rectangle or
      a contents bitmap. The node above is a sibling and cannot read the
      state of its window in the stage it draws in, so the window answers
      that question in its own prepare stage and the node reads the answer,
      see WindowXPAbove::Prepare(). */
  bool above_prepare_ = false;

  int32_t scale_ = 2;
  int32_t pause_index_ = 0;
  int32_t cursor_opacity_ = 255;
  bool cursor_fade_ = false;

  RefPtr<Bitmap> windowskin_;
  RefPtr<Bitmap> contents_;
  bool stretch_ = true;
  RefPtr<Rect> cursor_rect_;
  bool active_ = true;
  bool pause_ = false;
  int32_t x_ = 0, y_ = 0, width_ = 0, height_ = 0;
  int32_t ox_ = 0, oy_ = 0;
  int32_t opacity_ = 255, back_opacity_ = 255, contents_opacity_ = 255;

  //! The object pool slot of the ground of this window, bound at set 1.
  UniformBlockPool::Slot object_slot_ = {};

  /*! The param of the tint shader of the stretched background layer, see
      WindowVX::CreateTintBinding(). */
  wgpu::Buffer tint_uniform_;
  //! The bind group of set 3 of the tint pipeline.
  wgpu::BindGroup tint_group_;

  //! The range the stretched background layer appended to the batch.
  PrimitiveEmitter::Slot background_slot_ = {};
  //! The range the tiled background layer and the frame appended.
  PrimitiveEmitter::Slot ground_slot_ = {};

  //! The object pool slot of the node above, bound at set 1.
  UniformBlockPool::Slot above_object_slot_ = {};
  /*! The range the node above appended which erases the stencil over the area
      of the window, drawn at kStencilClear before the region is marked. */
  PrimitiveEmitter::Slot stencil_clear_slot_ = {};
  //! The range the node above appended which marks the inner region into the
  //! stencil.
  PrimitiveEmitter::Slot stencil_slot_ = {};
  //! The range the node above appended which is the cursor, from the skin.
  PrimitiveEmitter::Slot cursor_slot_ = {};
  //! The range the node above appended which are the arrows and the pause
  //! icon, from the skin.
  PrimitiveEmitter::Slot arrows_slot_ = {};
  //! The range the node above appended which are the contents, from their
  //! own bitmap.
  PrimitiveEmitter::Slot contents_slot_ = {};
};

}  // namespace urge

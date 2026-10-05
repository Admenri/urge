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
#include "core/definition.h"
#include "core/node.h"
#include "core/primitive.h"
#include "core/uniform.h"
#include "core/utility.h"
#include "core/viewport.h"

namespace urge {

class WindowXP;

//! Second half of a WindowXP: a hidden sibling node which draws the cursor,
//! arrows, pause icon and contents through the stencil, so they land above the
//! rest of the parent. Not public API -- owned and driven by its WindowXP.
class WindowXPAbove : public Node {
 public:
  WindowXPAbove(WindowXP* parent, RefPtr<Viewport> viewport);

 protected:
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  WindowXP* parent_ = nullptr;
};

//! Window of RGSS1 (XP): frame and background from the skin, with everything
//! inside drawn by a WindowXPAbove sibling and clipped through the stencil.
URGE_BINDING()
class WindowXP : public Node {
 public:
  URGE_BINDING()
  WindowXP(RefPtr<Viewport> viewport = nullptr);
  URGE_BINDING()
  ~WindowXP() override;

  URGE_BINDING()
  void Update();

  URGE_BINDING()
  ATTR(bool, Visible) override;
  URGE_BINDING()
  ATTR(RefPtr<Viewport>, Viewport);
  URGE_BINDING()
  ATTR(RefPtr<Bitmap>, Windowskin);
  URGE_BINDING()
  ATTR(RefPtr<Bitmap>, Contents);
  URGE_BINDING()
  ATTR(bool, Stretch);
  URGE_BINDING()
  ATTR(RefPtr<Rect>, CursorRect);
  URGE_BINDING()
  ATTR(bool, Active);
  URGE_BINDING()
  ATTR(bool, Pause);
  URGE_BINDING()
  ATTR(int32_t, X);
  URGE_BINDING()
  ATTR(int32_t, Y);
  URGE_BINDING()
  ATTR(int32_t, Width);
  URGE_BINDING()
  ATTR(int32_t, Height);
  URGE_BINDING()
  ATTR(int32_t, OX);
  URGE_BINDING()
  ATTR(int32_t, OY);
  URGE_BINDING()
  ATTR(int32_t, Opacity);
  URGE_BINDING()
  ATTR(int32_t, BackOpacity);
  URGE_BINDING()
  ATTR(int32_t, ContentsOpacity);
  URGE_BINDING()
  ATTR(int32_t, Z) override;

 private:
  friend class WindowXPAbove;

  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  //! Emits the tiled background and the nine-slice frame (texture pipeline).
  void EmitGroundInternal(PrimitiveEmitter& emitter);
  //! Emits the stretched background layer (tint pipeline).
  void EmitBackgroundInternal(PrimitiveEmitter& emitter);
  //! Emits the quad erasing the stencil over the whole window at kStencilClear.
  void EmitStencilClearInternal(PrimitiveEmitter& emitter);
  //! Emits the quad marking the inner region into the stencil.
  void EmitStencilInternal(PrimitiveEmitter& emitter);
  //! Emits the cursor, clipped to the inner region.
  void EmitCursorInternal(PrimitiveEmitter& emitter);
  //! Emits the arrows and the pause icon, clipped to the frame.
  void EmitArrowsInternal(PrimitiveEmitter& emitter);
  //! Emits the contents, clipped to the inner region.
  void EmitContentsInternal(PrimitiveEmitter& emitter);

  //! Emits one slice of the skin, stretched over \p dest.
  void EmitSliceInternal(PrimitiveEmitter& emitter,
                         const RectI& src,
                         const RectI& dest,
                         const glm::vec4& color);
  //! Emits one slice of the skin, tiled over \p dest.
  void EmitTiledInternal(PrimitiveEmitter& emitter,
                         const RectI& src,
                         const RectI& dest,
                         const glm::vec4& color);
  //! Emits the nine slices of \p src over \p dest; \p draw_center fills the
  //! middle (the frame leaves it out, the cursor keeps it).
  void EmitNineSliceInternal(PrimitiveEmitter& emitter,
                             const RectI& src,
                             const RectI& dest,
                             int32_t unit,
                             const glm::vec4& color,
                             bool draw_center = true);

  //! Creates set 3 of the tint pipeline used by EmitBackgroundInternal().
  void CreateTintBinding();

  //! Contents origin, limited to the scrollable overflow.
  glm::ivec2 LimitedOriginInternal() const;

  //! Inner region of the frame, where the contents and the cursor are clipped.
  RectI ContentRectInternal() const;

  //! Stencil value the node above marks the inner region with; shared with
  //! WindowVX, because the erase at kStencilClear keeps marks from colliding.
  static constexpr uint32_t kStencilReference = 1;
  static constexpr uint32_t kStencilClear = 0;

  //! Node drawing the cursor and the contents above this window's parent.
  RefPtr<WindowXPAbove> above_;

  //! True while the node above has something to draw; answered here because the
  //! sibling cannot read this window's state in the stage it draws in.
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

  //! Object pool slot of the ground of this window, bound at set 1.
  UniformBlockPool::Slot object_slot_ = {};

  //! Tint param of the stretched background layer.
  wgpu::Buffer tint_uniform_;
  //! Bind group of set 3 of the tint pipeline.
  wgpu::BindGroup tint_group_;

  //! Range the stretched background layer appended to the batch.
  PrimitiveEmitter::Slot background_slot_ = {};
  //! Range the tiled background layer and the frame appended.
  PrimitiveEmitter::Slot ground_slot_ = {};

  //! Object pool slot of the node above, bound at set 1.
  UniformBlockPool::Slot above_object_slot_ = {};
  //! Range the node above appended to erase the stencil over the window.
  PrimitiveEmitter::Slot stencil_clear_slot_ = {};
  //! Range the node above appended to mark the inner region into the stencil.
  PrimitiveEmitter::Slot stencil_slot_ = {};
  //! Range the node above appended which is the cursor, from the skin.
  PrimitiveEmitter::Slot cursor_slot_ = {};
  //! Range the node above appended which is the arrows and pause icon.
  PrimitiveEmitter::Slot arrows_slot_ = {};
  //! Range the node above appended which is the contents, from their bitmap.
  PrimitiveEmitter::Slot contents_slot_ = {};
};

}  // namespace urge

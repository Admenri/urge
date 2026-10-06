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

class WindowXPAbove : public Node {
 public:
  WindowXPAbove(WindowXP* parent, RefPtr<Viewport> viewport);

 protected:
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;

  WindowXP* parent_ = nullptr;
};

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

  void EmitGroundInternal(PrimitiveEmitter& emitter);

  void EmitBackgroundInternal(PrimitiveEmitter& emitter);

  void EmitStencilClearInternal(PrimitiveEmitter& emitter);

  void EmitStencilInternal(PrimitiveEmitter& emitter);

  void EmitCursorInternal(PrimitiveEmitter& emitter);

  void EmitArrowsInternal(PrimitiveEmitter& emitter);

  void EmitContentsInternal(PrimitiveEmitter& emitter);

  void EmitSliceInternal(PrimitiveEmitter& emitter,
                         const RectI& src,
                         const RectI& dest,
                         const glm::vec4& color);

  void EmitTiledInternal(PrimitiveEmitter& emitter,
                         const RectI& src,
                         const RectI& dest,
                         const glm::vec4& color);

  void EmitNineSliceInternal(PrimitiveEmitter& emitter,
                             const RectI& src,
                             const RectI& dest,
                             int32_t unit,
                             const glm::vec4& color,
                             bool draw_center = true);

  void CreateTintBinding();

  glm::ivec2 LimitedOriginInternal() const;

  RectI ContentRectInternal() const;

  static constexpr uint32_t kStencilReference = 1;
  static constexpr uint32_t kStencilClear = 0;

  RefPtr<WindowXPAbove> above_;

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

  UniformBlockPool::Slot object_slot_ = {};

  wgpu::Buffer tint_uniform_;

  wgpu::BindGroup tint_group_;

  PrimitiveEmitter::Slot background_slot_ = {};

  PrimitiveEmitter::Slot ground_slot_ = {};

  UniformBlockPool::Slot above_object_slot_ = {};

  PrimitiveEmitter::Slot stencil_clear_slot_ = {};

  PrimitiveEmitter::Slot stencil_slot_ = {};

  PrimitiveEmitter::Slot cursor_slot_ = {};

  PrimitiveEmitter::Slot arrows_slot_ = {};

  PrimitiveEmitter::Slot contents_slot_ = {};
};

}  // namespace urge

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

#include "core/effect.h"
#include "core/node.h"
#include "core/utility.h"

namespace urge {

class Viewport : public Node {
 public:
  /*-export.begin-*/
  Viewport(int32_t x, int32_t y, int32_t width, int32_t height);
  Viewport(RefPtr<Rect> rect);
  Viewport();
  ~Viewport() override;

  void Flash(RefPtr<Color> color, int32_t duration);
  void Update();

  ATTR(RefPtr<Rect>, Rect);
  ATTR(int32_t, OX);
  ATTR(int32_t, OY);
  ATTR(RefPtr<Color>, Color);
  ATTR(RefPtr<Tone>, Tone);
  ATTR(RefPtr<Effect>, Effect);
  /*-export.end-*/

 protected:
  void DisposeObject() override;
  bool Prepare(DrawParam param) override;
  bool DoDraw(DrawParam param) override;
  void PostDraw(DrawParam param) override;

 private:
  void ResetTransform();
  void CreateEffectBindings();
  void AcquireOffscreen(const RectI& region);

  /*! Closes the pass of the parent and opens one on the texture this viewport
      filters with its effect, so its children draw into that texture instead of
      the render target, see PostDraw(). \p parent_scissor is the clip they
      inherit and \p screen_scissor the rect of this viewport, both in the
      coordinates of the render target. */
  bool BeginFilter(DrawParam param,
                   const RectI& parent_scissor,
                   const RectI& screen_scissor);

  /*! Draws the texture the children of this viewport filled back into the
      render target with the shader of the effect, see BeginFilter(). */
  void FinishFilter(DrawParam param);

  RefPtr<Rect> rect_;
  glm::ivec2 origin_ = glm::ivec2(0);
  RefPtr<Color> color_;
  RefPtr<Tone> tone_;
  RefPtr<Effect> effect_;

  struct {
    glm::vec4 color = glm::vec4(0.0f);
    float step = 0.0f;
  } flash_;

  /*! The scratch render target of this viewport: the texture its children fill
      while it carries an effect, and the copy of the render target the tint
      pass reads otherwise. One texture serves both because the two paths never
      run in the same frame. */
  RefPtr<Bitmap> offscreen_;
  wgpu::Buffer object_uniform_, tint_uniform_;
  wgpu::BindGroup object_group_, tint_group_;

  //! What BeginFilter() took the children away from, put back by FinishFilter().
  RefPtr<Bitmap> filter_target_;
  wgpu::BindGroup filter_scene_;
  /*! Where the filtered region lies in the render target and the part of it the
      parent left visible, both in the coordinates of the target, and whether
      the children of this frame went into \p offscreen_ at all. */
  RectI filter_region_, filter_scissor_;
  bool filtering_ = false;

  //! The emitter of the quad which draws the region of this viewport back after
  //! its effect ran. That quad is emitted in the drawing stage, after the
  //! vertex batch of the frame was uploaded, so this node cannot use that
  //! batch.
  PrimitiveEmitter primitive_;
};

}  // namespace urge

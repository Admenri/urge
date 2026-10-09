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

#include <cstddef>
#include <cstdint>
#include <vector>

#include "glm/glm.hpp"

#include "core/bitmap.h"
#include "core/object.h"
#include "core/primitive.h"

namespace urge {

struct DrawContext;

/*! Everything the sprite shader reads about one sprite of the frame.

    A sprite is drawn with the model matrix and the blend parameters of its
    own, and the shader takes both from the array of these which the batch
    uploads: the quad a vertex belongs to names the element it reads, see
    kVS_SpriteBatch. Keeping the per-sprite data out of the bindings is what
    lets a run of sprites which share a texture and a blend state become one
    draw call, because nothing but the texture changes between them. */
struct alignas(16) SpriteParam {
  glm::mat4 model_mat;
  glm::vec4 blend_color;
  glm::vec4 blend_tone;
  float bush_depth = 0.0f;
  float bush_opacity = 1.0f;
  float padding[2] = {0.0f, 0.0f};
};

/*! The sprite geometry and the per-sprite parameters of one frame.

    Sprites do not draw into the vertex batch of the frame the way the other
    nodes do, they share a stream of their own: the shader finds the
    parameters of a quad by the index of that quad in the stream, so the
    stream holds nothing but sprite quads and the element of a quad is its
    ordinal. The stream is written in the prepare stage and uploaded once,
    with the parameter array, before the draws of the frame run. */
class SpriteBatch : public Singleton<SpriteBatch> {
 public:
  /*! The stretch of consecutive sprites which is one draw call.

      A run is opened by its first sprite and stays open while the next node
      of every member is a sprite which can join it, see Sprite::DoDraw(). The
      member which finds that the next node cannot join closes the run, and
      the run is what the draw call covers: the vertices of its members are
      contiguous in the stream and in the order they were prepared, so one
      draw of the whole range draws them exactly as separate draws would. */
  struct Run {
    bool active = false;
    uint32_t first_vertex = 0;
    uint32_t end_vertex = 0;
    int32_t blend_type = 0;
    RefPtr<Bitmap> texture;
    /*! The scene the members of the run read, which is the one the node they
        hang from was entered with: a camera of a sibling replaces it, so the
        two cannot share a draw. */
    wgpu::BindGroup scene;
  };

  SpriteBatch();
  ~SpriteBatch();

  /*! Whether the merge is off. A sprite then draws on its own whatever
      follows it, which is what a comparison of a merged frame against the
      frame every sprite draws separately needs; the environment asks for it
      with URGE_NO_SPRITE_BATCH. */
  bool disabled() const { return disabled_; }
  void SetDisabled(bool disabled) { disabled_ = disabled; }

  /*! Starts a frame: the stream and the parameter array are empty again. */
  void BeginFrame();

  /*! Uploads the stream and the parameters of the frame, and closes any run
      which was left open. Has to run before the draws of the frame. */
  void Flush();

  PrimitiveEmitter& emitter() { return emitter_; }

  /*! Records \p param for the quad which starts at \p vertex_first. The
      ordinal of the quad is what the shader indexes the array with, so the
      vertex has to be the one the emitter handed out for it. */
  void SetParam(uint32_t vertex_first, const SpriteParam& param);

  wgpu::BindGroup param_group() { return param_group_; }

  Run& run() { return run_; }

 private:
  void EnsureParamBuffer(std::size_t bytes);

  PrimitiveEmitter emitter_;
  std::vector<SpriteParam> params_;
  wgpu::Buffer param_buffer_ = nullptr;
  wgpu::BindGroup param_group_ = nullptr;
  Run run_;
  bool disabled_ = false;
};

/*! Draws the run which is open in \p param, if one is. Nothing happens when
    no run is open, so a caller which is about to change the render pass can
    call it unconditionally. */
void FlushSpriteBatch(DrawContext* param);

}  // namespace urge
